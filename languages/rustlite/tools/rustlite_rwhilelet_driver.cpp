#include <iostream>
#include <cassert>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite;
    using namespace edn;
    std::cout << "[rustlite-rwhilelet] building demo...\n";
    Builder b; b.begin_module();
    b.sum_enum("OptionI32", { {"None", {}}, {"Some", {"i32"}} });
    b.fn_raw("rwl_demo", "i32", {},
        "[ "
    "  (const %zero i32 0) (const %three i32 3) "
    "  (rsome %opt OptionI32 %three) "
    "  (const %acc i32 0) "
    "  (rwhile-let OptionI32 Some %opt :bind %x :body [ "
    "     (add %acc2 i32 %acc %x) (rassign %acc %acc2) (break) "
    "  ]) "
        "  (ret i32 %acc) "
        "]");
    b.end_module();

    auto prog = b.build();
    auto ast = parse(prog.edn_text);
    auto expanded = rustlite::expand_rustlite(ast);

    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-rwhilelet] type check failed\n";
        for(const auto& e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }

    IREmitter em(tctx); TypeCheckResult ir;
    auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
    std::cout << "[rustlite-rwhilelet] ok\n";
    return 0;
}
