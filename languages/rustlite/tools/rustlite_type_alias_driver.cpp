// Driver: rustlite type alias via rtypedef
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){ using namespace edn; 
    // Define typedef at module scope then function using alias.
    const char* mod = R"EDN((module (typedef :name MyI32 :type i32) (fn :name "use_alias" :ret i32 :params [ ] :body [ (const %x MyI32 5) (ret i32 %x) ])))EDN";
    auto ast=parse(mod); auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res = tc.check_module(expanded);
    if(!res.success){ for(auto &e: res.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; } return 1; }
    std::cout << "[rustlite-type-alias] ok\n"; return 0; }
