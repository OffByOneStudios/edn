// Positive driver: rloop-val with multiple rbreak :value statements of same type.
// Expect type check success; semantics: first break ends loop; later break unreachable but not analyzed.
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    b.fn_raw("loop_val_multi_break", "i32", {}, "[ (rloop-val %out i32 :body [ (const %a i32 42) (rbreak :value %a) (const %b i32 43) (rbreak :value %b) ]) (ret i32 %out) ]");
    b.end_module();
    auto prog=b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded);
    if(!res.success){ for(auto &e: res.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; return 1; }
    std::cout << "[rustlite-rloop-val-multi-break] ok\n"; return 0; }
