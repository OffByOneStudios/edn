// Positive driver: rloop-val with plain rbreak (no :value) should leave zero-initialized destination intact
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    // rloop-val initializes %out to 0; plain rbreak should not assign, so %out remains 0
    b.fn_raw("loop_val_plain_break", "i32", {}, "[ (rloop-val %out i32 :body [ (const %tmp i32 7) (rbreak) ]) (ret i32 %out) ]");
    b.end_module();
    auto prog=b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded);
    if(!res.success){ for(auto &e: res.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; return 1; }
    std::cout << "[rustlite-rloop-val-plain-break] ok\n"; return 0; }
