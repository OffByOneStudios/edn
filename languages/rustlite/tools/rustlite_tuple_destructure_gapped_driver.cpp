// Gapped indices pattern should not emit E1454/E1455
#include <iostream>
#include "edn/edn.hpp"
#include "rustlite/expand.hpp"
int main(){
    const char* src = R"((module :id "tuple_pattern_gapped"
        (rfn :name "main" :ret i32 :params [] :body [
            (const %x i32 1) (const %y i32 2) (const %z i32 3)
            (tuple %t [ %x %y %z ])
            (tget %a i32 %t 0) (tget %c i32 %t 2)
            (add %ac i32 %a %c) (ret i32 %ac)
        ])
    ))";
    auto ast = edn::parse(src); auto expanded = rustlite::expand_rustlite(ast);
    bool bad=false; if(expanded && std::holds_alternative<edn::list>(expanded->data)){
        auto &ML = std::get<edn::list>(expanded->data).elems; if(!ML.empty() && ML[0])
            for(auto &kv: ML[0]->metadata)
                if(kv.first.rfind("generic-error-",0)==0 && kv.second && std::holds_alternative<edn::symbol>(kv.second->data)){
                    auto n = std::get<edn::symbol>(kv.second->data).name; if(n.rfind("E1454:",0)==0 || n.rfind("E1455:",0)==0){ bad=true; break; }
                }
    }
    if(bad){ std::cerr << "[tuple-destructure-gapped] unexpected tuple pattern error emitted\n"; return 1; }
    std::cout << "[tuple-destructure-gapped] ok\n"; return 0; }
