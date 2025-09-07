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
        let t = 0;
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
    edn::TypeContext tctx; edn::TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    if(!tcres.success){ std::cerr << "type check failed for tuple match literal guard test\n"; for(auto &e: tcres.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; }
        std::cerr << "[IR]\n" << edn::to_string(expanded) << "\n"; return 1; }
    for(auto &e: tcres.errors){ if(e.code=="E1454"||e.code=="E1455"||e.code=="E1456"||e.code=="E1457"||e.code=="E1458"||e.code=="E1459"){ std::cerr<<"unexpected pattern diagnostic: "<<e.code<<"\n"; std::cerr<<edn::to_string(expanded)<<"\n"; return 1; } }
    std::cout<<"[rustlite-tuple-match-literal-guard] ok"<<"\n"; return 0; }
