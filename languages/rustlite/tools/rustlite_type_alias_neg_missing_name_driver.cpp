// Negative typedef: missing :name -> E1330
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
int main(){ using namespace edn; 
    // Direct module text with missing :name in typedef
    std::string mod = "(module (fn :name \"dummy\" :ret void :params [ ] :body [ ]) (typedef :type i32))"; 
    auto ast=parse(mod); 
    auto &expanded=ast; // no rustlite macros used
    TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded); bool saw=false; for(auto &e: res.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; if(e.code=="E1330") saw=true; }
    if(!saw){ std::cerr<<"Expected E1330 missing :name"<<"\n"; return 1; }
    std::cout<<"[rustlite-type-alias-neg-missing-name] ok\n"; return 0; }
