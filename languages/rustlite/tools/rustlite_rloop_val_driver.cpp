// Positive driver: rloop-val should capture break value into destination
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    // fn that uses rloop-val to produce 42 immediately
    b.fn_raw("loop_val", "i32", {}, "[ (rloop-val %out i32 :body [ (const %answer i32 42) (rbreak :value %answer) ]) (ret i32 %out) ]");
    b.end_module();
    auto prog=b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res=tc.check_module(expanded);
    if(!res.success){ for(auto &e: res.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; return 1; }
    std::cout << "[rustlite-rloop-val] ok\n"; return 0; }

