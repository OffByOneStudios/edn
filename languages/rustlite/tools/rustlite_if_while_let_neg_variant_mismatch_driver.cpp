// Negative driver: rif-let with variant name mismatch vs sum value -> expect variant mismatch diagnostic
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"
// Direct EDN module; rif-let macro will expand; variant B does not exist in S -> E1405
int main(){ using namespace edn; 
    // NOTE: function :name expects a string literal in current parser
    std::string mod = "(module (sum :name S :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"bad_variant\" :ret i32 :params [ (param (ptr (struct-ref S)) %p) ] :body [ (rif-let %res i32 S B %p :bind %v :then [ (const %x i32 1) :value %x ] :else [ (const %y i32 2) :value %y ]) (ret i32 %res) ]))";
    auto ast=parse(mod); auto expanded=rustlite::expand_rustlite(ast); TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded);
    bool saw=false; for(auto &e: res.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; if(e.code=="E1405") saw=true; }
    if(!saw){ std::cerr<<"Expected E1405 unknown variant diagnostic not found\n"; return 1; }
    std::cout<<"[rustlite-if-while-let-neg-variant-mismatch] ok\n"; return 0; }
