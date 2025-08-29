// Negative driver: rtry with unsupported sum type (not Result*/Option*) should trigger E1603
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    // Define a sum that is not Option/Result
    b.sum_enum("WrapI32", { {"Wrap", {"i32"}} });
    b.fn_raw("bad", "WrapI32", {},
        "[ (const %v i32 5) (rwrap %w WrapI32 %v) (rtry %x WrapI32 %w) (rderef %rv WrapI32 %w) (ret WrapI32 %rv) ]");
    b.end_module();
    auto prog = b.build(); auto ast = parse(prog.edn_text); auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res = tc.check_module(expanded);
    bool saw=false; for(auto &e: res.errors){ if(e.code=="E1603") saw=true; }
    if(!saw){
        std::cerr << "[rustlite-rtry-neg] expected E1603 not found\n"; for(auto &e: res.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; return 1; }
    std::cout << "[rustlite-rtry-neg] ok\n"; return 0;
}
