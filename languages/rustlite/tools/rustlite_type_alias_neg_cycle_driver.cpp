// Negative typedef: self-referential cycle -> expect E1333 (invalid underlying form) or future cycle-specific code
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
int main(){ using namespace edn; 
    // Self-referential: underlying refers to alias symbol (currently causes generic parse/lookup failure)
    std::string mod = "(module (typedef :name Self :type Self))"; // self-reference -> E1333 (cycle)
    auto ast=parse(mod); auto &expanded=ast; TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded); bool ok=false; for(auto &e: res.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; if(e.code=="E1333") ok=true; }
    if(!ok){ std::cerr<<"Expected E1333 invalid type form"<<"\n"; return 1; }
    std::cout<<"[rustlite-type-alias-neg-cycle] ok\n"; return 0; }
