#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../languages/rustlite/parser/parser.hpp"
#include "../languages/rustlite/include/rustlite/expand.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"

// Test: tuple match with purely variable patterns (no literals) produces one cluster per arm
// and assigns the first arm that matches (which will be the first arm because all are unconditional).
// Surface snippet:
//   let t = 0; // placeholder until real tuple values land
//   let r = match t { (a,b){ a } (_,_){ 0 } };
// We assert structural properties: >=2 tuple-pattern-meta, rif/if chain, tget ops for variable names.

int main(){
    const char* SRC = R"RL(fn use(){
        let t = 0; // placeholder tuple value
        let r = match t { (a,b){ a } (_ , _){ 0 } };
        return r;
    })RL";
    rustlite::Parser parser;
    auto pres = parser.parse_string(SRC, "varpat.rl.rs");
    if(!pres.success){ std::cerr<<"parse failed: "<<pres.error_message<<"\n"; return 1; }
    auto ast = edn::parse(pres.edn); if(!ast){ std::cerr<<"edn parse failed"<<"\n"; return 1; }
    auto expanded = rustlite::expand_rustlite(ast); if(!expanded){ std::cerr<<"expand failed"<<"\n"; return 1; }
    edn::TypeContext tctx; edn::TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    if(!tcres.success){ std::cerr<<"type check failed for variable tuple pattern test\n"; for(auto &e: tcres.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; } return 1; }
    for(auto &e: tcres.errors){ if(e.code=="E1454"||e.code=="E1455"){ std::cerr<<"unexpected tuple pattern diagnostic: "<<e.code<<" -> "<<e.message<<"\n"; return 1; } }
    auto irStr = edn::to_string(expanded);
    size_t metaCount=0; for(size_t pos=0; ;){ pos = irStr.find("tuple-pattern-meta", pos); if(pos==std::string::npos) break; ++metaCount; pos+=18; }
    bool metaOk = metaCount>=2;
    size_t rifCount=0; for(size_t pos=0; ;){ pos = irStr.find("(rif ", pos); if(pos==std::string::npos) break; ++rifCount; pos+=5; }
    size_t ifCount=0; for(size_t pos=0; ;){ pos = irStr.find("(if ", pos); if(pos==std::string::npos) break; ++ifCount; pos+=4; }
    bool condChainOk = (rifCount>=1) || (ifCount>=1); // only two arms, single rif or if sufficient
    bool hasVarTgets = irStr.find("(tget %a")!=std::string::npos && irStr.find("(tget %b")!=std::string::npos;
    if(!(metaOk && condChainOk && hasVarTgets)){
        std::cerr << "[debug-ir-dump]\n" << irStr << "\n";
    }
    assert(metaOk && "expected tuple-pattern-meta per arm");
    assert(condChainOk && "expected rif or if chain");
    assert(hasVarTgets && "expected tget ops for %a and %b");
    std::cout << "[rustlite-tuple-match-variable-pattern] ok\n"; return 0;
}
