// Negative typedef: invalid underlying type form -> E1333
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
int main(){ using namespace edn; std::string mod = "(module (fn :name \"dummy\" :ret void :params [ ] :body [ ]) (typedef :name Bad :type ()))"; auto ast=parse(mod); auto &expanded=ast; 
    TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded); bool saw=false; for(auto &e: res.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; if(e.code=="E1333") saw=true; }
    if(!saw){ std::cerr<<"Expected E1333 invalid type form"<<"\n"; return 1; }
    std::cout<<"[rustlite-type-alias-neg-invalid-underlying] ok\n"; return 0; }
