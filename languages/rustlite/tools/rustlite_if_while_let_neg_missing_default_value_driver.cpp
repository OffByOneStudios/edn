// Negative driver: rif-let default branch missing :value should produce E1422
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    b.sum_enum("OptionI32", { {"None", {}}, {"Some", {"i32"}} });
    // rif-let with :then having :value but :else missing :value (default branch)
    b.fn_raw("rif_let_missing_default_value", "i32", {}, "[ (const %x i32 2) (const %tmp i32 3) (rsome %opt OptionI32 %tmp) (rif-let %out i32 OptionI32 Some %opt :bind %v :then [ (add %y i32 %v %x) :value %y ] :else [ (const %z i32 0) ]) (ret i32 %out) ]");
    b.end_module();
    auto prog = b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded);
    bool saw=false; for(auto &e: res.errors){ if(e.code=="E1422"){ saw=true; } std::cerr<<e.code<<":"<<e.message<<"\n"; }
    if(!saw){ std::cerr << "Expected E1422 not found" << "\n"; return 1; }
    std::cout << "[rustlite-if-while-let-neg-missing-default-value] ok\n"; return 0;
}
