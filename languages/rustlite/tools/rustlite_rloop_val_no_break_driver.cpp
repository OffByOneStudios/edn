// Positive driver: rloop-val with no rbreak in body; compile-only (loop is infinite at runtime) but type system should accept and %out remains zero-init.
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    // No rbreak; body just a const side-effect; ret after loop is theoretically unreachable but we don't model reachability.
    b.fn_raw("loop_val_no_break", "i32", {}, "[ (rloop-val %out i32 :body [ (const %tmp i32 7) ]) (ret i32 %out) ]");
    b.end_module();
    auto prog=b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded);
    if(!res.success){ for(auto &e: res.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; return 1; }
    std::cout << "[rustlite-rloop-val-no-break] ok\n"; return 0; }
