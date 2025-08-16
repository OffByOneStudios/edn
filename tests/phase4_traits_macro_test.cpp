// Phase 4: Traits expander smoke test (no Catch2; follow project style)
#include <cassert>
#include <string>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/traits.hpp"
#include "edn/ir_emitter.hpp"
#include <llvm/IR/Verifier.h>

using namespace edn;

void run_phase4_traits_macro_test(){
  std::cout << "[traits] start" << std::endl;
    // Module defines a trait Show with one method print: (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32))
    // We'll define an impl fn print_i32 with matching signature and test fnptr + call-indirect usage.
  node_ptr mod = parse(R"EDN(
        (module :name test
          (trait :name Show :methods [ (method :name print :type (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32))) ])
          ; Removed unsupported pointer-typed global with nil init
          (fn :name "main" :ret i32 :params [ (param i32 %x) ] :body [
            ; build vtable pointer for print_i32
            (fnptr %fp (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) print_i32)
            (alloca %vt ShowVT)
            (member-addr %p ShowVT %vt print)
            (store (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) %p %fp)
            (alloca %obj ShowObj)
            ; make trait object from object data pointer and local vtable address casted to ptr ShowVT
            (bitcast %vtp (ptr ShowVT) %vt)
            (make-trait-obj %o Show %obj %vtp)
            (trait-call %rv i32 Show %o print %x)
            (ret i32 %rv)
          ])
          (fn :name "print_i32" :ret i32 :params [ (param (ptr i8) %ctx) (param i32 %v) ] :body [
            (ret i32 %v)
          ])
        )
    )EDN");
    std::cout << "[traits] parsed module: " << (mod?"ok":"null") << std::endl;
    assert(mod);

    // Expand traits and ensure struct ShowVT exists
  std::cout << "[traits] expanding traits..." << std::endl;
  node_ptr expanded = expand_traits(mod);
  std::cout << "[traits] expanded" << std::endl;
    bool sawStruct=false;
    auto &top = std::get<list>(expanded->data).elems;
    for(size_t i=1;i<top.size(); ++i){
        if(!top[i] || !std::holds_alternative<list>(top[i]->data)) continue;
        auto &l = std::get<list>(top[i]->data).elems;
        if(l.empty() || !std::holds_alternative<symbol>(l[0]->data)) continue;
        if(std::get<symbol>(l[0]->data).name=="struct"){
            for(size_t j=1;j<l.size(); ++j){
                if(!l[j] || !std::holds_alternative<keyword>(l[j]->data)) break;
                std::string kw = std::get<keyword>(l[j]->data).name;
                if(++j>=l.size()) break;
                auto v = l[j];
                if(kw=="name" && std::holds_alternative<std::string>(v->data) && std::get<std::string>(v->data) == std::string("ShowVT")){
                    sawStruct=true; break;
                }
            }
        }
    }
  std::cout << "[traits] saw ShowVT struct: " << (sawStruct?"yes":"no") << std::endl;
  assert(sawStruct);

  std::cout << "[traits] emitting LLVM..." << std::endl;
  TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tc;
  auto *M = emitter.emit(expanded, tc);
  std::cout << "[traits] emitter result: success=" << (tc.success?"true":"false") << ", module=" << (M?"ok":"null") << std::endl;
  if(!tc.success){
    std::cerr << "[traits] type check errors:" << std::endl;
    for(const auto& e : tc.errors){
      std::cerr << "  " << e.code << ": " << e.message << " (" << e.line << ":" << e.col << ")" << std::endl;
      for(const auto& n : e.notes){
        std::cerr << "    note: " << n.message << " (" << n.line << ":" << n.col << ")" << std::endl;
      }
    }
  }
  assert(tc.success);
  assert(M != nullptr);
    // Verify module is well-formed
  std::string err; llvm::raw_string_ostream rso(err); bool bad = llvm::verifyModule(*M, &rso);
  if(bad){
    std::cerr << "[traits] LLVM verifyModule failed:\n" << rso.str() << std::endl;
    assert(!bad);
  }
  std::cout << "Phase 4 traits macro test passed" << std::endl;
}
