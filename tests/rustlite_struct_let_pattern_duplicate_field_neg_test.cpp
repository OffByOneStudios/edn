#include <cassert>
#include <iostream>
#include <string>

#include "../languages/rustlite/parser/parser.hpp"
#include "../languages/rustlite/include/rustlite/expand.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"

// Negative test: duplicate struct field in pattern should produce E1457 diagnostic harvested by TypeChecker
int main(){
    const char* SRC = R"RL(fn use(){
        let p = 0;
        let Point { x, x } = p; // duplicate field
        return 0;
    })RL";
    rustlite::Parser parser; auto pres = parser.parse_string(SRC, "structpatdup.rl.rs"); if(!pres.success){ std::cerr<<"parse failed: "<<pres.error_message<<"\n"; return 1; }
    auto ast = edn::parse(pres.edn); if(!ast){ std::cerr<<"edn parse failed\n"; return 1; }
    auto expanded = rustlite::expand_rustlite(ast); if(!expanded){ std::cerr<<"expand failed\n"; return 1; }
    edn::TypeContext tctx; edn::TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    bool saw=false; for(auto &e : tcres.errors){ if(e.code=="E1457"){ saw=true; break; } }
    if(!saw){ std::cerr<<"expected E1457 duplicate fields diagnostic; errors=\n"; for(auto &e: tcres.errors){ std::cerr<<e.code<<": "<<e.message<<"\n"; } std::cerr<<edn::to_string(expanded)<<"\n"; return 1; }
    std::cout << "[rustlite-struct-let-pattern-dup-neg] ok\n"; return 0;
}
