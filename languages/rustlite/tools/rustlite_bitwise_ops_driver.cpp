#include <iostream>
#include <sstream>
#include "edn/edn.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

int main(){
    std::cout << "[rustlite-bitwise-ops] running...\n";
    const char* src =
        "(module :id \"bitwise\" "
        "  (rfn :name \"ops\" :ret i32 :params [ ] :body [ "
        "     (const %a i32 5) (const %b i32 3) (const %c i32 1) "
        "     (rcall %and i32 and %a %b) "
        "     (rcall %or i32 or %a %b) "
        "     (rcall %xor i32 xor %a %b) "
        "     (rcall %shl i32 shl %a %c) "
        "     (rcall %lshr i32 lshr %a %c) "
        "     (rcall %ashr i32 ashr %a %c) "
        "     (rcall %sdiv i32 div %a %b) "
        "     (rcall %u u32 udiv %a %b) "
        "     (rcall %sr i32 srem %a %b) "
        "     (rcall %ur u32 urem %a %b) "
        "     (add %tmp i32 %and %xor) "
        "     (ret i32 %tmp) "
        "  ]) )";
    auto ast = parse(src);
    auto expanded = rustlite::expand_rustlite(ast);
    std::ostringstream oss;
    std::function<void(const node_ptr&)> dump=[&](const node_ptr& n){
        if(!n) return; if(std::holds_alternative<list>(n->data)){
            auto &L=std::get<list>(n->data).elems; if(!L.empty() && std::holds_alternative<symbol>(L[0]->data)){
                auto op=std::get<symbol>(L[0]->data).name; if(op=="and"||op=="or"||op=="xor"||op=="shl"||op=="lshr"||op=="ashr"||op=="sdiv"||op=="udiv"||op=="srem"||op=="urem") oss<<op<<" ";
            }
            for(auto &e:L) dump(e);
        } else if(std::holds_alternative<vector_t>(n->data)){
            for(auto &e: std::get<vector_t>(n->data).elems) dump(e);
        }
    };
    dump(expanded);
    auto found = oss.str();
    const char* required[] = {"and","or","xor","shl","lshr","ashr","sdiv","udiv","srem","urem"};
    for(auto r: required){ if(found.find(r)==std::string::npos){ std::cerr<<"[rustlite-bitwise-ops] missing op="<<r<<"\n"; return 1; } }
    std::cout<<"[rustlite-bitwise-ops] ok\n";
    return 0;
}
