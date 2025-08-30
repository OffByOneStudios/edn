// Negative driver: rwhile-let with variant mismatch -> expect E1405
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"
int main(){ using namespace edn; 
    std::string mod = "(module (sum :name OptI32 :variants [ (variant :name Some :fields [ i32 ]) (variant :name None :fields [ ]) ]) (fn :name \"wle_variant_bad\" :ret void :params [ ] :body [ (const %one i32 1) (rsome %opt OptI32 %one) (rwhile-let OptI32 Bad %opt :bind %v :body [ (const %tmp i32 0) ]) (ret void ) ]))"; 
    auto ast=parse(mod); auto expanded=rustlite::expand_rustlite(ast); TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded); bool saw=false; for(auto &e: res.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; if(e.code=="E1405") saw=true; } if(!saw){ std::cerr<<"Expected E1405 unknown variant diagnostic not found\n"; return 1; } std::cout<<"[rustlite-rwhile-let-neg-variant-mismatch] ok\n"; return 0; }
