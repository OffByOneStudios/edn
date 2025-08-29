// Negative driver: malformed rfor-range syntax should leave raw (rfor-range ...) which type checker should ignore
// We assert we still get some generic EGEN errors (unknown instruction) rather than crashing, and specifically no unexpected codes.
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    // Malformed: missing :body keyword
    b.fn_raw("bad1", "i32", {}, "[ (rfor-range %i i32 0 5) (ret i32 0) ]");
    // Malformed: tuple mode but missing body vector
    b.fn_raw("bad2", "i32", {}, "[ (rrange %r i32 0 3 :inclusive true) (rfor-range %j i32 %r :bod [ ]) (ret i32 0) ]");
    b.end_module();
    auto prog = b.build(); auto ast = parse(prog.edn_text); auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res = tc.check_module(expanded);
    bool sawSomething=false; for(auto &e: res.errors){ if(e.code=="EGEN") sawSomething=true; }
    if(!sawSomething){ std::cerr << "[rustlite-range-neg] expected generic errors for malformed rfor-range not found\n"; for(auto &e: res.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; return 1; }
    std::cout << "[rustlite-range-neg] ok\n"; return 0;
}
