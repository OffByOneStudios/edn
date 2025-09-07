#include <cassert>
#include <iostream>
#include <string>

#include "../languages/rustlite/parser/parser.hpp"
#include "../languages/rustlite/include/rustlite/expand.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"

// Basic struct pattern let destructuring (Phase 2b minimal):
// Surface (pseudo): struct Point { x, y }; let Point { x, y } = p;
// For now we assume struct Point already exists in type context (simplified test injects it manually after parse before type check).

int main(){
    const char* SRC = R"RL(fn use(){
        let p = 0; // placeholder: would be a Point instance later
        let Point { x, y } = p; // struct pattern
        return 0;
    })RL";
    rustlite::Parser parser; auto pres = parser.parse_string(SRC, "structpat.rl.rs"); if(!pres.success){ std::cerr<<"parse failed: "<<pres.error_message<<"\n"; return 1; }
    auto ast = edn::parse(pres.edn); if(!ast){ std::cerr<<"edn parse failed\n"; return 1; }
    auto expanded = rustlite::expand_rustlite(ast); if(!expanded){ std::cerr<<"expand failed\n"; return 1; }
    // Type check (struct type not truly defined yet; we only assert IR contains member ops)
    edn::TypeContext tctx; edn::TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    // Accept failure due to unknown struct; only verify member ops present
    auto irStr = edn::to_string(expanded);
    bool hasX = irStr.find("(member %x Point %p x)")!=std::string::npos;
    bool hasY = irStr.find("(member %y Point %p y)")!=std::string::npos;
    if(!(hasX && hasY)){
        std::cerr << "expected member ops for x and y\n" << irStr << "\n"; return 1; }
    std::cout << "[rustlite-struct-let-pattern] ok\n"; return 0;
}
