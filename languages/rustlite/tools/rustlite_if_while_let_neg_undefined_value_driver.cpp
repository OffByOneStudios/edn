// Negative driver: rif-let with :then referencing undefined :value symbol -> expect case :value undefined (E1421)
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"
// rif-let then branch references %missing -> expect E1421
int main(){ using namespace edn; 
    // NOTE: function :name must be a string literal per parser expectations
    std::string mod = "(module (sum :name OptionI32 :variants [ (variant :name Some :fields [ i32 ]) (variant :name None :fields [ ]) ]) (fn :name \"undef_value\" :ret i32 :params [ ] :body [ (const %x i32 5) (rsome %opt OptionI32 %x) (rif-let %out i32 OptionI32 Some %opt :bind %v :then [ (add %y i32 %v %x) :value %missing ] :else [ (const %z i32 0) :value %z ]) (ret i32 %out) ]))";
    auto ast=parse(mod); auto expanded=rustlite::expand_rustlite(ast); TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded);
    bool saw=false; for(auto &e: res.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; if(e.code=="E1421") saw=true; }
    if(!saw){ std::cerr<<"Expected E1421 case :value undefined not found\n"; return 1; }
    std::cout<<"[rustlite-if-while-let-neg-undef-value] ok\n"; return 0; }
