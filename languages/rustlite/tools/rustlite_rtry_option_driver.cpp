#include <iostream>
#include <cassert>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

// Driver for rtry macro with Option type.
int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    b.sum_enum("OptionI32", { {"Some", {"i32"}}, {"None", {}} });
    // some_path: unwrap Some and add 2 -> Some(42)
    b.fn_raw("some_path", "OptionI32", {},
        "[ (const %forty i32 40) (rsome %o OptionI32 %forty) (rtry %x OptionI32 %o) (const %two i32 2) (add %sum i32 %x %two) (rsome %out OptionI32 %sum) (rderef %rv OptionI32 %out) (ret OptionI32 %rv) ]");
    // none_short: early return None
    b.fn_raw("none_short", "OptionI32", {},
        "[ (rnone %n OptionI32) (rtry %x OptionI32 %n) (const %one i32 1) (rsome %out OptionI32 %one) (rderef %rv OptionI32 %out) (ret OptionI32 %rv) ]");
    b.end_module();
    auto prog = b.build();
    auto ast = parse(prog.edn_text);
    auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    if(!tcres.success){ for(const auto &e: tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; } return 1; }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
    std::cout << "[rustlite-rtry-option] ok" << std::endl; return 0;
}
