// Negative driver: rwhile-let body references undefined symbol in place of expected value -> E1421
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"
int main(){ using namespace edn; 
    // Use nested rif-let inside rwhile-let body and reference undefined %missing in its :then :value to trigger E1421
    std::string mod = "(module (sum :name OptI32 :variants [ (variant :name Some :fields [ i32 ]) (variant :name None :fields [ ]) ]) (fn :name \"wle_undef_body\" :ret void :params [ ] :body [ (const %one i32 1) (rsome %opt OptI32 %one) (rwhile-let OptI32 Some %opt :bind %v :body [ (rif-let %out i32 OptI32 Some %opt :bind %inner :then [ (add %tmp i32 %inner %one) :value %missing ] :else [ (const %z i32 0) :value %z ]) ]) (ret void ) ]))"; 
    auto ast=parse(mod); auto expanded=rustlite::expand_rustlite(ast); TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded); bool saw=false; for(auto &e: res.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; if(e.code=="E1421") saw=true; } if(!saw){ std::cerr<<"Expected E1421 undefined symbol diagnostic not found\n"; return 1; } std::cout<<"[rustlite-rwhile-let-neg-undef-body] ok\n"; return 0; }
