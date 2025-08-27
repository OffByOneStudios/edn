// Negative test: const external global without :init should trigger E1227 (const global requires :init)
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace edn;
    // rextern-const adds :external true; we explicitly supply :const true w/out :init to force E1227.
    const char* src = R"EDN((module :id "rl_extern_const_missing_init"
  (rextern-const :name EXT_CONST_MISS :type i32 :const true)
))EDN";
    auto ast = parse(src);
    auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res = tc.check_module(expanded);
    if(res.success){ std::cerr << "[rustlite-extern-global-init-neg] expected failure but succeeded\n"; return 1; }
    bool saw=false; for(auto &e: res.errors){ if(e.code=="E1227") saw=true; }
    if(!saw){
        std::cerr << "[rustlite-extern-global-init-neg] expected E1227 not found; diagnostics:\n";
        for(auto &e: res.errors){ std::cerr << "  ("<<e.code<<") "<<e.message<<"\n"; for(auto &n: e.notes){ std::cerr << "    note: "<<n.message<<"\n"; } }
        return 2;
    }
    std::cout << "[rustlite-extern-global-init-neg] expected E1227 const-without-init failure observed\n";
    return 0;
}
