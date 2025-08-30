// Driver: exercise rif-let (if let) and rwhile-let macros
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    // Create OptionI32 sum with Some/None
    b.sum_enum("OptionI32", { {"None", {}}, {"Some", {"i32"}} });
    // Function using rif-let: if let Some %v = %opt { add } else { ret 0 }
    b.fn_raw("ifle", "i32", {}, "[ (const %x i32 5) (const %tmp i32 5) (rsome %opt OptionI32 %tmp) (rif-let %out i32 OptionI32 Some %opt :bind %v :then [ (add %y i32 %v %x) :value %y ] :else [ :value %x ]) (ret i32 %out) ]");
    // Function using rwhile-let to sum while successive Options are Some until None encountered
    b.fn_raw("wle", "i32", {}, "[ (const %acc i32 0) (const %one i32 1) (rsome %cur OptionI32 %one) (rwhile-let OptionI32 Some %cur :bind %v :body [ (add %acc2 i32 %acc %v) (assign %acc %acc2) (rnone %cur_next OptionI32) (assign %cur %cur_next) ]) (ret i32 %acc) ]");
    b.end_module();
    auto prog = b.build();
    std::cerr << "[raw-edn]\n" << prog.edn_text << "\n";
    auto ast = parse(prog.edn_text); auto expanded = rustlite::expand_rustlite(ast);
    // (debug dump removed)
    TypeContext tctx; TypeChecker tc(tctx); auto res = tc.check_module(expanded);
    if(!res.success){ for(auto &e: res.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; return 1; }
    std::cout << "[rustlite-if-while-let] ok\n"; return 0;
}
