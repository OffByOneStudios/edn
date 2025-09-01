// Negative driver: tuple pattern against non-tuple target -> E1455
#include <iostream>
#include "edn/edn.hpp"
#include "rustlite/expand.hpp"
int main(){
    const char* src = R"((module :id "tuple_pattern_non_tuple_neg"
        (rfn :name "main" :ret i32 :params [] :body [
            (const %x i32 42)
            (tget %a i32 %x 0) (tget %b i32 %x 1)
            (add %r i32 %a %b) (ret i32 %r)
        ])
    ))";
    auto ast = edn::parse(src); auto expanded = rustlite::expand_rustlite(ast);
    bool saw=false; if(expanded && std::holds_alternative<edn::list>(expanded->data)){
        auto &ML = std::get<edn::list>(expanded->data).elems; if(!ML.empty() && ML[0])
            for(auto &kv: ML[0]->metadata)
                if(kv.first.rfind("generic-error-",0)==0 && kv.second && std::holds_alternative<edn::symbol>(kv.second->data))
                    if(std::get<edn::symbol>(kv.second->data).name.rfind("E1455:",0)==0){ saw=true; break; }
    }
    if(!saw){ std::cerr << "[tuple-destructure-non-tuple-neg] expected E1455 not found\n"; return 1; }
    std::cout << "[tuple-destructure-non-tuple-neg] ok\n"; return 0; }
