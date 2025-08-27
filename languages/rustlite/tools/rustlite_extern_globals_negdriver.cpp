/** Extern globals negative driver: redefining an extern global with an initializer should fail. */
#include <iostream>
#include <cassert>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace edn;
    const char* edn_src = R"EDN((module :id "rl_extern_globs_neg"
    (rextern-global :name EXT_BAD :type i32)
    (global :name EXT_BAD :type i32 :init (const i32 5)) ; invalid redefinition with init
))EDN";

    auto ast = parse(edn_src);
    auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res = tc.check_module(expanded);
    if(res.success){ std::cerr << "[rustlite-extern-globals-neg] expected failure but succeeded\n"; return 1; }
    bool saw=false; for(auto &e: res.errors){ if(e.code=="EGEN" && e.message.find("duplicate global")!=std::string::npos) saw=true; }
    if(!saw){
    std::cerr << "[rustlite-extern-globals-neg] expected duplicate-global diagnostic not found; dumping diagnostics (code|message) =>\n";
        for(auto &e: res.errors){ std::cerr << "  (" << e.code << ") " << e.message << "\n"; for(auto &n: e.notes){ std::cerr << "    note: " << n.message << "\n"; } }
        return 2;
    }
    std::cout << "[rustlite-extern-globals-neg] expected EGEN duplicate-global failure observed\n"; return 0;
}
