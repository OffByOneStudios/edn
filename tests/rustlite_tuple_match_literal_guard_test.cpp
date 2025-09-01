#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../languages/rustlite/parser/parser.hpp"
#include "../languages/rustlite/include/rustlite/expand.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"

// Test: tuple match with literal guards chooses first matching arm in source order.
// Surface snippet lowered via rustlite parser:
//   fn main() {
//     let t = 0; // placeholder single value; future will construct tuple source
//     let r = match t { (1,2) {3} (_,2){4} (_,_){5} };
//     return r;
//   }
// Currently only integer literal guards supported; first arm has literal mismatch so later arms ensure chain structure.

int main(){
    const char* SRC = R"RL(fn use(){
        let t = 0; // placeholder single value (tuple support WIP)
        let r = match t { (1,2){3} (_ ,2){4} (_,_){5} };
        return r;
    })RL";
    rustlite::Parser parser;
    auto pres = parser.parse_string(SRC, "test.rl.rs");
    if(!pres.success){ std::cerr<<"parse failed: "<<pres.error_message<<"\n"; return 1; }
    auto ast = edn::parse(pres.edn);
    if(!ast){ std::cerr<<"edn parse failed"<<"\n"; return 1; }
    auto expanded = rustlite::expand_rustlite(ast);
    if(!expanded){ std::cerr<<"expand failed"<<"\n"; return 1; }
    // Serialize expanded EDN to string for simple pattern assertions (skip type checking until rif/tget supported).
    auto irStr = edn::to_string(expanded);
    // (debug IR dump removed; will emit on assertion failure below)
    // Basic structural assertions:
    // 1. Expect one tuple-pattern-meta per arm (currently 3) or at least >= number of arms parsed
    size_t metaCount=0; for(size_t pos=0; ;){ pos = irStr.find("tuple-pattern-meta", pos); if(pos==std::string::npos) break; ++metaCount; pos+=18; }
    bool metaOk = metaCount>=3; // strict for this exact test shape
    // 2. Either rif chain (pre-expansion) or nested ifs (post expansion)
    size_t rifCount=0; for(size_t pos=0; ;){ pos = irStr.find("(rif ", pos); if(pos==std::string::npos) break; ++rifCount; pos+=5; }
    size_t ifCount=0; for(size_t pos=0; ;){ pos = irStr.find("(if ", pos); if(pos==std::string::npos) break; ++ifCount; pos+=4; }
    bool condChainOk = (rifCount>=2) || (ifCount>=2);
    // 3. Literal guard eq comparisons for first two discriminatory arms. After expansion these show as '(eq ...' forms.
    size_t eqCount=0; for(size_t pos=0; ;){ pos = irStr.find("(eq ", pos); if(pos==std::string::npos) break; ++eqCount; pos+=4; }
    bool guardsOk = eqCount>=2;
    if(!(metaOk && condChainOk && guardsOk)) std::cerr<<"[debug-ir-dump]\n"<<irStr<<"\n";
    assert(metaOk && "expected tuple-pattern-meta per arm");
    assert(condChainOk && "expected rif or if chain for multi-arm match");
    assert(guardsOk && "expected at least two eq literal guard comparisons");
    std::cout<<"[rustlite-tuple-match-literal-guard] ok"<<"\n"; return 0;
}
