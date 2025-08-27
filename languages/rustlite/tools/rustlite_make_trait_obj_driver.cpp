#include <iostream>
#include <cassert>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/traits.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

// Focused driver for rmake-trait-obj macro: constructs a trait object and invokes a method.
// We validate: successful typecheck, presence of vtable/object structs, indirect call, and that return value flows.
static std::string module_to_ir(llvm::Module* m){ std::string buf; llvm::raw_string_ostream os(buf); m->print(os,nullptr); os.flush(); return buf; }

static void must_find(const std::string& hay, const char* needle){
    if(hay.find(needle)==std::string::npos){
        std::cerr << "[rustlite-make-trait-obj] missing IR snippet: " << needle << "\n";
        std::cerr << hay.substr(0, 800) << "\n";
        assert(false);
    }
}

int main(){
    std::cout << "[rustlite-make-trait-obj] building trait object demo...\n";
    const char* edn = R"EDN((module :id "rl_make_trait_obj"
      (rtrait :name Show :methods [ (method :name print :type (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32))) ])
      (rfn :name "print_i32" :ret i32 :params [ (param (ptr i8) %ctx) (param i32 %v) ] :body [ (ret i32 %v) ])
      (fn :name "demo" :ret i32 :params [ (param i32 %x) ] :body [
        (rfnptr %fp (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) print_i32)
        (alloca %vt ShowVT)
        (member-addr %fld ShowVT %vt print)
        (store (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) %fld %fp)
        (alloca %obj ShowObj)
        (bitcast %vtp (ptr ShowVT) %vt)
        (rmake-trait-obj %o Show %obj %vtp)
        (rtrait-call %rv i32 Show %o print %x)
        (ret i32 %rv)
      ])
    ))EDN";

    auto ast = parse(edn);
    auto expanded = expand_traits(rustlite::expand_rustlite(ast));
    TypeContext tctx; TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-make-trait-obj] type check failed unexpectedly\n";
        for(auto &e: tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
    auto irs = module_to_ir(mod);
    must_find(irs, "%struct.ShowVT = type { ptr }");
    must_find(irs, "%struct.ShowObj = type { ptr, ptr }");
    must_find(irs, "getelementptr inbounds %struct.ShowVT");
    must_find(irs, "call i32 %"); // indirect call signature snippet
    std::cout << "[rustlite-make-trait-obj] ok\n";
    return 0;
}
