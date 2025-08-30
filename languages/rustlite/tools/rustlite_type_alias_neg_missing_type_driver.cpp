// Negative typedef: missing :type -> E1331
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
int main(){ using namespace edn; std::string mod = "(module (fn :name \"dummy\" :ret void :params [ ] :body [ ]) (typedef :name X))"; auto ast=parse(mod); auto &expanded=ast; 
    TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded); bool saw=false; for(auto &e: res.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; if(e.code=="E1331") saw=true; }
    if(!saw){ std::cerr<<"Expected E1331 missing :type"<<"\n"; return 1; }
    std::cout<<"[rustlite-type-alias-neg-missing-type] ok\n"; return 0; }
