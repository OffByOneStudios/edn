// Negative driver: over-arity tuple pattern -> E1454
#include <iostream>
#include "edn/edn.hpp"
#include "rustlite/expand.hpp"
int main(){
    const char* src = R"((module :id "tuple_pattern_over_arity"
        (rfn :name "main" :ret i32 :params [] :body [
            (const %x i32 1) (const %y i32 2) (const %z i32 3)
            (tuple %t [ %x %y %z ])
            (tget %a i32 %t 0) (tget %b i32 %t 1) (tget %c i32 %t 2) (tget %d i32 %t 3)
            (add %ab i32 %a %b) (add %abc i32 %ab %c) (ret i32 %abc)
        ])
    ))";
    auto ast = edn::parse(src); auto expanded = rustlite::expand_rustlite(ast);
    bool found=false; if(expanded && std::holds_alternative<edn::list>(expanded->data)){
        auto &ML = std::get<edn::list>(expanded->data).elems; if(!ML.empty() && ML[0])
            for(auto &kv: ML[0]->metadata)
                if(kv.first.rfind("generic-error-",0)==0 && kv.second && std::holds_alternative<edn::symbol>(kv.second->data))
                    if(std::get<edn::symbol>(kv.second->data).name.rfind("E1454:",0)==0){ found=true; break; }
    }
    if(!found){ std::cerr << "[tuple-destructure-over-arity-neg] expected E1454 not found\n"; return 1; }
    std::cout << "[tuple-destructure-over-arity-neg] ok\n"; return 0; }
