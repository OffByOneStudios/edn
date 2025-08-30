// Negative typedef: redefinition -> E1332
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
int main(){ using namespace edn; std::string mod = "(module (fn :name \"dummy\" :ret void :params [ ] :body [ ]) (typedef :name X :type i32) (typedef :name X :type i32))"; auto ast=parse(mod); auto &expanded=ast; 
    TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded); bool saw=false; for(auto &e: res.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; if(e.code=="E1332") saw=true; }
    if(!saw){ std::cerr<<"Expected E1332 redefinition"<<"\n"; return 1; }
    std::cout<<"[rustlite-type-alias-neg-redefinition] ok\n"; return 0; }
