/** Trait negative driver: missing value argument to rtrait-call should trigger arity/param error. */
#include <iostream>
#include <cassert>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/traits.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace edn;
    // NOTE: single (module ...) form; raw string avoids C++ concatenation errors.
    const char* edn_src = R"EDN((module :id "rl_traits_neg"
    (rtrait :name Show :methods [ (method :name print :type (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32))) ])
    (rfn :name "print_i32" :ret i32 :params [ (param (ptr i8) %ctx) (param i32 %v) ] :body [ (ret i32 %v) ])
    (fn :name "trait_bad" :ret i32 :params [ (param i32 %x) ] :body [
        (rfnptr %fp (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) print_i32)
        (alloca %vt ShowVT) (member-addr %p ShowVT %vt print) (store (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) %p %fp)
        (alloca %obj ShowObj) (bitcast %vtp (ptr ShowVT) %vt) (rmake-trait-obj %o Show %obj %vtp)
        (rtrait-call %rv i32 Show %o print) ; missing value argument intentionally
        (ret i32 %rv)
    ])
))EDN";

    auto ast = parse(edn_src);
    auto expanded = expand_traits(rustlite::expand_rustlite(ast));
    TypeContext tctx; TypeChecker tc(tctx); auto res = tc.check_module(expanded);
    if(res.success){ std::cerr << "[rustlite-trait-neg] expected failure but succeeded\n"; return 1; }
    bool saw=false; for(auto &e: res.errors){ if(e.code=="E1325") saw=true; }
    if(!saw){
    std::cerr << "[rustlite-trait-neg] expected E1325 diagnostic not found; dumping diagnostics (code|message) =>\n";
        for(auto &e: res.errors){ std::cerr << "  (" << e.code << ") " << e.message << "\n"; for(auto &n: e.notes){ std::cerr << "    note: " << n.message << "\n"; } }
        return 2;
    }
    std::cout << "[rustlite-trait-neg] expected E1325 arg-count failure observed\n"; return 0;
}
