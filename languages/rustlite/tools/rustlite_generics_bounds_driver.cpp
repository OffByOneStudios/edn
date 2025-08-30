// Generic bounds syntax placeholder: ensure :bounds parsed and removed from specialization
#include <iostream>
#include "edn/edn.hpp"
#include "rustlite/expand.hpp"
using namespace edn;
int main(){
    const char* mod = R"EDN((module
        (fn :name "add_like" :generics [ T ] :bounds [ (bound T Addable) ] :ret T :params [ (param T %x) ] :body [ (ret T %x) ])
        (fn :name "use" :ret i32 :params [ ] :body [ (const %a i32 1) (rcall-g %r i32 add_like [ i32 ] %a) (ret i32 %a) ])
    ))EDN";
    auto ast=parse(mod);
    auto expanded=rustlite::expand_rustlite(ast);
    // Verify there is a specialized fn add_like__i32 and no residual :generics or :bounds keywords in it.
    bool foundSpec=false, hasResidual=false;
    if(expanded && std::holds_alternative<list>(expanded->data)){
        auto &ML=std::get<list>(expanded->data).elems; for(auto &n : ML){ if(!n||!std::holds_alternative<list>(n->data)) continue; auto &L=std::get<list>(n->data).elems; if(L.empty()||!std::holds_alternative<symbol>(L[0]->data)) continue; if(std::get<symbol>(L[0]->data).name=="fn"){
            std::string fname;  for(size_t i=1;i+1<L.size(); i+=2){ if(!std::holds_alternative<keyword>(L[i]->data)) break; std::string kw=std::get<keyword>(L[i]->data).name; if(kw=="name" && std::holds_alternative<std::string>(L[i+1]->data)) fname=std::get<std::string>(L[i+1]->data); if(kw=="generics"||kw=="bounds") hasResidual=true; }
            if(fname=="add_like__i32") { foundSpec=true; }
        } }
    }
    if(!foundSpec || hasResidual){ std::cerr<<"bounds specialization failure"<<"\n"; return 1; }
    std::cout<<"[rustlite-generics-bounds] ok\n"; return 0; }
