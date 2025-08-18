#include <iostream>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/traits.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace edn;
    std::cout << "[rustlite-rdot-neg] building demo...\n";

    const char* edn =
        "(module :id \"rl_rdot_neg\" "
        "  (rtrait :name Show :methods [ (method :name print :type (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32))) ]) "
        "  (rfn :name \"print_i32\" :ret i32 :params [ (param (ptr i8) %ctx) (param i32 %v) ] :body [ (ret i32 %v) ]) "
    "  (fn :name \"bad_trait_method\" :ret i32 :params [ (param i32 %x) ] :body [ "
        "    (rfnptr %fp (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) print_i32) "
        "    (alloca %vt ShowVT) (member-addr %p ShowVT %vt print) (store (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) %p %fp) "
        "    (alloca %obj ShowObj) (bitcast %vtp (ptr ShowVT) %vt) (rmake-trait-obj %o Show %obj %vtp) "
    "    (rdot %rv i32 Show %o println %x) (ret i32 %rv) "
    "  ]) "
    "  (fn :name \"bad_free_arity\" :ret i32 :params [ (param i32 %x) ] :body [ (rdot %rv i32 print_i32 %x) (ret i32 %rv) ]) "
        ")";

    auto ast = parse(edn);
    // Expand Rustlite first, then traits machinery
    auto expanded = expand_traits(rustlite::expand_rustlite(ast));

    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(tcres.success){
        std::cerr << "[rustlite-rdot-neg] expected failures but type check passed\n";
        return 1;
    }
    // Minimal smoke: ensure we saw at least one error
    if(tcres.errors.empty()){
        std::cerr << "[rustlite-rdot-neg] no errors returned (expected some)\n";
        return 2;
    }
    std::cout << "[rustlite-rdot-neg] saw expected errors\n";
    return 0;
}
