// Negative generic call: unknown generic callee E1700
#include <iostream>
#include "edn/edn.hpp"
#include "rustlite/expand.hpp"
using namespace edn;
int main(){
    const char* mod = R"EDN((module
        (fn :name "use" :ret i32 :params [ ] :body [
            (const %a i32 1)
            (rcall-g %r i32 not_a_generic [ i32 ] %a)
            (ret i32 %a)
        ])
    ))EDN";
    auto ast=parse(mod); auto expanded=rustlite::expand_rustlite(ast);
    bool saw=false; if(expanded && std::holds_alternative<list>(expanded->data)){ auto &ML=std::get<list>(expanded->data).elems; if(!ML.empty() && ML[0] && std::holds_alternative<symbol>(ML[0]->data)){ for(auto &kv: ML[0]->metadata){ if(kv.first.rfind("generic-error-",0)==0 && kv.second && std::holds_alternative<symbol>(kv.second->data)){ auto name=std::get<symbol>(kv.second->data).name; if(name.rfind("E1700:",0)==0){ saw=true; break; } } } } }
    if(!saw){ std::cerr<<"Expected E1700 unknown generic diagnostic"<<"\n"; return 1; }
    std::cout<<"[rustlite-generics-neg-unknown] ok\n"; return 0; }
