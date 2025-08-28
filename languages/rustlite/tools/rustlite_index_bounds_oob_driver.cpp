#include <iostream>
#include <sstream>
#include <string>
#include "edn/edn.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

int main(){
    std::cout << "[rustlite-index-bounds-oob] running...\n";
    setenv("RUSTLITE_BOUNDS","1",1);
    const char* src = \
        "(module :id \"idx_bounds_oob\" "
        "  (rfn :name \"oob\" :ret i32 :params [ ] :body [ "
        "     (arr %a i32 [ (const %v0 i32 1) (const %v1 i32 2) ]) "
        "     (const %i2 i32 2) "
        "     (rindex-load %x i32 %a %i2) "
        "     (ret i32 %x) "
        "  ]) )";
    auto ast = parse(src);
    auto expanded = rustlite::expand_rustlite(ast);
    std::ostringstream oss;
    std::function<void(const node_ptr&, std::ostream&)> dump = [&](const node_ptr& n, std::ostream& os){
        if(!n){ os<<"nil"; return; }
        if(std::holds_alternative<symbol>(n->data)){ os<<std::get<symbol>(n->data).name; return; }
        if(std::holds_alternative<keyword>(n->data)){ os<<":"<<std::get<keyword>(n->data).name; return; }
        if(std::holds_alternative<int64_t>(n->data)){ os<<std::get<int64_t>(n->data); return; }
        if(std::holds_alternative<std::string>(n->data)){ os<<'"'<<std::get<std::string>(n->data)<<'"'; return; }
        if(std::holds_alternative<list>(n->data)){ os<<'('; bool first=true; for(auto &e: std::get<list>(n->data).elems){ if(!first) os<<' '; dump(e, os); first=false;} os<<')'; return; }
        if(std::holds_alternative<vector_t>(n->data)){ os<<'['; bool first=true; for(auto &e: std::get<vector_t>(n->data).elems){ if(!first) os<<' '; dump(e, os); first=false;} os<<']'; return; }
    };
    dump(expanded, oss);
    auto text = oss.str();
    // Expect panic form in expansion due to out-of-bounds idx == len
    if(text.find("panic") == std::string::npos){ std::cerr << "[rustlite-index-bounds-oob] missing panic"; return 1; }
    std::cout << "[rustlite-index-bounds-oob] ok\n";
    return 0;
}
