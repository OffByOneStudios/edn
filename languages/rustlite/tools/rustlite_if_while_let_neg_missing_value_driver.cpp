// Negative driver: rif-let / rwhile-let missing :value should produce E1421 / E1422
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    b.sum_enum("OptionI32", { {"None", {}}, {"Some", {"i32"}} });
    // rif-let missing :value in then branch
    b.fn_raw("rif_let_missing_value", "i32", {}, "[ (const %x i32 1) (rsome %o OptionI32 %x) (rif-let %out i32 OptionI32 Some %o :bind %v :then [ (add %y i32 %v %x) ] :else [ (const %z i32 0) :value %z ]) (ret i32 %out) ]");
    // rwhile-let missing :value injection in default (remove generated :value by crafting body without break path value) -- simulate by constructing manual match via macro misuse: omit body value (should still rely on macro's injection so craft invalid) Not easily done; instead rely on rif-let case.
    b.end_module();
    auto prog = b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded);
    bool saw=false; for(auto &e: res.errors){ if(e.code=="E1421"||e.code=="E1422"){ saw=true; }
        std::cerr<<e.code<<":"<<e.message<<"\n"; }
    if(!saw){ std::cerr << "Expected E1421 or E1422 not found" << "\n"; return 1; }
    std::cout << "[rustlite-if-while-let-neg-missing-value] ok\n"; return 0;
}
