// Driver: rustlite type alias via rtypedef
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    // Create alias: MyI32 = i32
    // Macro form: (rtypedef MyI32 i32) -> (typedef :name MyI32 :type i32)
    b.fn_raw("use_alias", "i32", {}, "[ (rtypedef MyI32 i32) (const %x MyI32 5) (ret i32 %x) ]");
    b.end_module();
    auto prog=b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded);
    if(!res.success){ for(auto &e: res.errors){ std::cerr<<e.code<<":"<<e.message<<"\n"; } return 1; }
    std::cout << "[rustlite-type-alias] ok\n"; return 0; }
