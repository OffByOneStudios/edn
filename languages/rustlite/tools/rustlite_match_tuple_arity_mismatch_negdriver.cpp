// Negative driver: match arm tuple pattern arity mismatch -> E1454
#include <iostream>
#include "edn/edn.hpp"
#include "rustlite/expand.hpp"
int main(){
    const char* src = R"((module :id "match_tuple_arity_neg"
        (rfn :name "main" :ret i32 :params [] :body [
            (const %x i32 1) (const %y i32 2)
            (tuple %t [ %x %y ])
            (tget %a i32 %t 0) (tget %b i32 %t 1) (tget %c i32 %t 2)
            (tuple-pattern-meta %t 3)
            (add %r i32 %a %b) (ret i32 %r)
        ])
    ))";
    auto ast = edn::parse(src); auto expanded = rustlite::expand_rustlite(ast);
    bool saw=false; if(expanded && std::holds_alternative<edn::list>(expanded->data)){
        auto &ML = std::get<edn::list>(expanded->data).elems; if(!ML.empty() && ML[0])
            for(auto &kv: ML[0]->metadata)
                if(kv.first.rfind("generic-error-",0)==0 && kv.second && std::holds_alternative<edn::symbol>(kv.second->data))
                    if(std::get<edn::symbol>(kv.second->data).name.rfind("E1454:",0)==0){ saw=true; break; }
    }
    if(!saw){ std::cerr << "[match-tuple-arity-neg] expected E1454 not found\n"; return 1; }
    std::cout << "[match-tuple-arity-neg] ok\n"; return 0; }
