// Traits example (Phase 4 experimental): build a simple Show trait and call through it.
#include <iostream>
#include <string>
#include "edn/edn.hpp"
#include "edn/traits.hpp"
#include "edn/ir_emitter.hpp"
#include <llvm/IR/Verifier.h>

using namespace edn;

int main(){
    const char* src = R"EDN(
        (module :name ex_traits
          (trait :name Show :methods [ (method :name print :type (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32))) ])
          (fn :name "main" :ret i32 :params [ (param i32 %x) ] :body [
            (fnptr %fp (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) print_i32)
            (alloca %vt ShowVT)
            (member-addr %p ShowVT %vt print)
            (store (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) %p %fp)
            (alloca %obj ShowObj)
            (bitcast %vtp (ptr ShowVT) %vt)
            (make-trait-obj %o Show %obj %vtp)
            (trait-call %rv i32 Show %o print %x)
            (ret i32 %rv)
          ])
          (fn :name "print_i32" :ret i32 :params [ (param (ptr i8) %ctx) (param i32 %v) ] :body [ (ret i32 %v) ])
        )
    )EDN";

    auto ast = parse(src);
    auto expanded = expand_traits(ast);

    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tc;
    auto *M = emitter.emit(expanded, tc);
    if(!tc.success || !M){
        std::cerr << "Type check failed (" << tc.errors.size() << " errors)\n";
        for(const auto& e : tc.errors){
            std::cerr << e.code << ": " << e.message << " (" << e.line << ":" << e.col << ")\n";
            for(const auto& n : e.notes){ std::cerr << "  note: " << n.message << "\n"; }
        }
        return 1;
    }

    std::string err; llvm::raw_string_ostream rso(err);
    bool bad = llvm::verifyModule(*M, &rso);
    if(bad){ std::cerr << "LLVM verify failed:\n" << rso.str(); return 2; }

    std::cout << "traits example OK\n";
    return 0;
}
