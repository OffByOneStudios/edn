// Negative typedef: alias name collides with existing sum/enum/struct -> expect E1332
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
int main(){ using namespace edn; 
    // Define typedef first, then conflicting sum so typedef collision path is exercised (typedef pass precedes sums)
    std::string mod = "(module (typedef :name Opt :type i32) (sum :name Opt :variants [ (variant :name Some :fields [ i32 ]) (variant :name None :fields [ ]) ]))"; 
    auto ast=parse(mod); auto &expanded=ast; TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded); bool saw=false; for(auto &e: res.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; if(e.code=="E1401") saw=true; }
    if(!saw){ std::cerr<<"Expected E1401 sum redefinition (typedef shadow)"<<"\n"; return 1; }
    std::cout<<"[rustlite-type-alias-neg-shadow] ok\n"; return 0; }
