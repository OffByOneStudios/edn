#include <iostream>
#include <sstream>
#include "edn/edn.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

int main(){
    std::cout << "[rustlite-compound-assign] running...\n";
    const char* src =
        "(module :id \"compound\" "
        "  (rfn :name \"do\" :ret i32 :params [ ] :body [ "
        "     (const %two i32 2) (as %x i32 %two) "
        "     (const %three i32 3) "
        "     (rassign-op %x i32 add %three) "
        "     (const %four i32 4) "
        "     (rassign-op %x i32 mul %four) "
        "     (const %one i32 1) "
        "     (rassign-op %x i32 shl %one) "
        "     (ret i32 %x) "
        "  ]) )";
    auto ast = parse(src);
    auto expanded = rustlite::expand_rustlite(ast);
    // Debug dump of expanded module
    std::ostringstream dump;
    std::function<void(const node_ptr&)> dumpNode=[&](const node_ptr& n){
        if(!n){ dump<<"nil"; return; }
        if(std::holds_alternative<symbol>(n->data)){ dump<<std::get<symbol>(n->data).name; return; }
        if(std::holds_alternative<keyword>(n->data)){ dump<<":"<<std::get<keyword>(n->data).name; return; }
        if(std::holds_alternative<int64_t>(n->data)){ dump<<std::get<int64_t>(n->data); return; }
        if(std::holds_alternative<std::string>(n->data)){ dump<<'"'<<std::get<std::string>(n->data)<<'"'; return; }
        if(std::holds_alternative<list>(n->data)){ dump<<'('; bool first=true; for(auto &e: std::get<list>(n->data).elems){ if(!first) dump<<' '; dumpNode(e); first=false;} dump<<')'; return; }
        if(std::holds_alternative<vector_t>(n->data)){ dump<<'['; bool first=true; for(auto &e: std::get<vector_t>(n->data).elems){ if(!first) dump<<' '; dumpNode(e); first=false;} dump<<']'; return; }
    }; dumpNode(expanded);
    std::ostringstream ops;
    std::function<void(const node_ptr&)> walk=[&](const node_ptr& n){
        if(!n) return; if(std::holds_alternative<list>(n->data)){
            auto &L=std::get<list>(n->data).elems; if(!L.empty() && std::holds_alternative<symbol>(L[0]->data)){
                auto op=std::get<symbol>(L[0]->data).name; if(op=="add"||op=="mul"||op=="shl") ops<<op<<" ";
            }
            for(auto &e:L) walk(e);
        } else if(std::holds_alternative<vector_t>(n->data)){
            for(auto &e: std::get<vector_t>(n->data).elems) walk(e);
        }
    };
    walk(expanded);
    auto found=ops.str();
    if(found.find("add")==std::string::npos || found.find("mul")==std::string::npos || found.find("shl")==std::string::npos){
        std::cerr<<"[rustlite-compound-assign] missing expected ops\n";
        std::cerr<<dump.str()<<"\n";
        return 1;
    }
    std::cout<<"[rustlite-compound-assign] ok\n";
    return 0;
}
