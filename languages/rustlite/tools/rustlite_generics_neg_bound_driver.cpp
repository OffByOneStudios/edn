// Negative bound: type does not satisfy Addable -> E1702
#include <iostream>
#include "edn/edn.hpp"
#include "rustlite/expand.hpp"
using namespace edn;
int main(){
    // bool (i1) not in Addable table; expect E1702 diagnostic
    const char* mod = R"EDN((module
        (fn :name "id_add" :generics [ T ] :bounds [ (bound T Addable) ] :ret T :params [ (param T %x) ] :body [ (ret T %x) ])
        (fn :name "use" :ret i32 :params [ ] :body [ (const %a i1 1) (rcall-g %r i1 id_add [ i1 ] %a) (ret i32 0) ])
    ))EDN";
    auto ast=parse(mod); auto expanded=rustlite::expand_rustlite(ast);
    bool saw=false; if(expanded && std::holds_alternative<list>(expanded->data)){ auto &ML=std::get<list>(expanded->data).elems; if(!ML.empty() && ML[0] && std::holds_alternative<symbol>(ML[0]->data)){ for(auto &kv: ML[0]->metadata){ if(kv.first.rfind("generic-error-",0)==0 && kv.second && std::holds_alternative<symbol>(kv.second->data)){ auto name=std::get<symbol>(kv.second->data).name; if(name.rfind("E1702:",0)==0){ saw=true; break; } } } } }
    if(!saw){ std::cerr<<"Expected E1702 unsatisfied bound diagnostic"<<"\n"; return 1; }
    std::cout<<"[rustlite-generics-neg-bound] ok\n"; return 0; }
