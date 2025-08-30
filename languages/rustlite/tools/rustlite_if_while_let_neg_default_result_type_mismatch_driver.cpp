// Negative driver: rif-let default branch :value type mismatch should produce E1423
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    b.sum_enum("OptionI32", { {"None", {}}, {"Some", {"i32"}} });
    // rif-let expecting i32 result, but :else default produces i1
    b.fn_raw("rif_let_default_result_mismatch", "i32", {}, "[ (const %x i32 4) (rnone %opt OptionI32) (const %flag i1 0) (rif-let %out i32 OptionI32 Some %opt :bind %v :then [ (add %y i32 %v %x) :value %y ] :else [ :value %flag ]) (ret i32 %out) ]");
    b.end_module();
    auto prog=b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded);
    bool saw=false; for(auto &e: res.errors){ if(e.code=="E1423") saw=true; std::cerr<<e.code<<":"<<e.message<<"\n"; }
    if(!saw){ std::cerr << "Expected E1423 not found" << "\n"; return 1; }
    std::cout << "[rustlite-if-while-let-neg-default-result-type-mismatch] ok\n"; return 0;
}
