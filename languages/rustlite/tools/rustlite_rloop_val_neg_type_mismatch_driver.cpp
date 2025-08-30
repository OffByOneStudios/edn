// Negative driver: rloop-val break value type mismatch should surface assign type mismatch (E1107)
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    // Expect the assign of i1 into i32 destination to trip E1107
    b.fn_raw("loop_val_bad", "i32", {}, "[ (rloop-val %out i32 :body [ (const %flag i1 1) (rbreak :value %flag) ]) (ret i32 %out) ]");
    b.end_module();
    auto prog=b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded);
    bool saw=false; for(auto &e: res.errors){ if(e.code=="E1107") saw=true; std::cerr<<e.code<<":"<<e.message<<"\n"; }
    if(!saw){ std::cerr << "Expected E1107 not found" << "\n"; return 1; }
    std::cout << "[rustlite-rloop-val-neg-type-mismatch] ok\n"; return 0; }
