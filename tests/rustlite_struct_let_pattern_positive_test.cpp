#include <cassert>
#include <iostream>
#include <string>

#include "../languages/rustlite/parser/parser.hpp"
#include "../languages/rustlite/include/rustlite/expand.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"

// Positive struct pattern test: no unknown/duplicate field diagnostics after expansion + type check.
int main(){
    const char* SRC = R"RL(fn use(){
        let p = 0;
        let Point { x, y } = p; // both fields valid, no duplicates
        return 0;
    })RL";
    rustlite::Parser parser; auto pres = parser.parse_string(SRC, "structpatpos.rl.rs"); if(!pres.success){ std::cerr<<"parse failed: "<<pres.error_message<<"\n"; return 1; }
    auto ast = edn::parse(pres.edn); if(!ast){ std::cerr<<"edn parse failed\n"; return 1; }
    auto expanded = rustlite::expand_rustlite(ast); if(!expanded){ std::cerr<<"expand failed\n"; return 1; }
    // Run type checker to harvest any expansion diagnostics into unified errors channel.
    edn::TypeContext tctx; edn::TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    // Fail if any struct pattern related errors surfaced.
    for(auto &e : tcres.errors){ if(e.code=="E1454"||e.code=="E1455"||e.code=="E1456"||e.code=="E1457"){ std::cerr<<"unexpected struct/tuple pattern diagnostic: "<<e.code<<" "<<e.message<<"\n"; return 1; } }
    std::cout << "[rustlite-struct-let-pattern-positive] ok\n"; return 0; }
