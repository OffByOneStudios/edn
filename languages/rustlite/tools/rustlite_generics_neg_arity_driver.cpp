// Negative generic call: arity mismatch should produce diagnostic E1701
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"
using namespace edn;
int main(){
    const char* mod = R"EDN((module
        (fn :name "id2" :generics [ T U ] :ret T :params [ (param T %x) (param U %y) ] :body [ (ret T %x) ])
        (fn :name "use" :ret i32 :params [ ] :body [
            (const %a i32 1)
            (rcall-g %r i32 id2 [ i32 ] %a %a) ; missing second type arg
            (ret i32 %a)
        ])
    ))EDN";
    auto ast=parse(mod); auto expanded=rustlite::expand_rustlite(ast);
    bool saw=false; // check module head metadata
    if(expanded && std::holds_alternative<list>(expanded->data)){
        auto &ML = std::get<list>(expanded->data).elems; if(!ML.empty() && ML[0] && std::holds_alternative<symbol>(ML[0]->data)){
            for(auto &kv : ML[0]->metadata){ if(kv.first.rfind("generic-error-",0)==0 && kv.second && std::holds_alternative<symbol>(kv.second->data)){ auto name=std::get<symbol>(kv.second->data).name; if(name.rfind("E1701:",0)==0) { saw=true; break; } } }
        }
    }
    if(!saw){ std::cerr<<"Expected E1701 arity mismatch diagnostic"<<"\n"; return 1; }
    std::cout<<"[rustlite-generics-neg-arity] ok\n"; return 0; }
