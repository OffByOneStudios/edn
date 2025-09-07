#include <cassert>
#include <iostream>
#include <string>

#include "../languages/rustlite/parser/parser.hpp"
#include "../languages/rustlite/include/rustlite/expand.hpp"
#include "edn/edn.hpp"

// Placeholder '_' fields should be skipped (no member op emitted for them) but meta should still list retained fields.
int main(){
    const char* SRC = R"RL(fn use(){
        let p = 0;
        let Point { x, _, y } = p; // placeholder in middle
        return 0;
    })RL";
    rustlite::Parser parser; auto pres = parser.parse_string(SRC, "structpatph.rl.rs"); if(!pres.success){ std::cerr<<"parse failed: "<<pres.error_message<<"\n"; return 1; }
    auto ast = edn::parse(pres.edn); if(!ast){ std::cerr<<"edn parse failed\n"; return 1; }
    auto expanded = rustlite::expand_rustlite(ast); if(!expanded){ std::cerr<<"expand failed\n"; return 1; }
    auto ir = edn::to_string(expanded);
    bool hasX = ir.find("(member %x Point %p x)")!=std::string::npos;
    bool hasY = ir.find("(member %y Point %p y)")!=std::string::npos;
    bool hasUnderscoreMember = ir.find("(member %_ Point %p")!=std::string::npos;
    bool hasMeta = ir.find("(struct-pattern-meta %p Point 2 x y)")!=std::string::npos; // only x and y captured
    if(!(hasX && hasY && !hasUnderscoreMember && hasMeta)){
        std::cerr << "placeholder test failed IR=\n" << ir << "\n"; return 1; }
    std::cout << "[rustlite-struct-let-pattern-placeholder] ok\n"; return 0;
}
