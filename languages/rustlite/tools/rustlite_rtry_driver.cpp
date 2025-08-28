#include <iostream>
#include <cassert>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

// Driver for rtry macro: exercises Ok path and early Err short-circuit.
int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    b.sum_enum("ResultI32", { {"Ok", {"i32"}}, {"Err", {"i32"}} });
    // ok_path: should return 42
    b.fn_raw("ok_path", "ResultI32", {},
        "[ (const %forty i32 40) (rok %r1 ResultI32 %forty) (rtry %x ResultI32 %r1) (const %two i32 2) (add %sum i32 %x %two) (rok %out ResultI32 %sum) (rderef %rv ResultI32 %out) (ret ResultI32 %rv) ]");
    // err_short: should early return Err(5)
    b.fn_raw("err_short", "ResultI32", {},
        "[ (const %five i32 5) (rerr %e ResultI32 %five) (rtry %x ResultI32 %e) (const %one i32 1) (rok %out ResultI32 %one) (rderef %rv ResultI32 %out) (ret ResultI32 %rv) ]");
    b.end_module();
    auto prog = b.build();
    auto ast = parse(prog.edn_text);
    auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    if(!tcres.success){ for(const auto &e: tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; } return 1; }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
    std::cout << "[rustlite-rtry] ok" << std::endl; return 0; }
