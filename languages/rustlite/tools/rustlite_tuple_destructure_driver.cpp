// Driver: tuple destructuring via manual tget sequence (placeholder until surface sugar added)
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    // Build a tuple of three ints and destructure into %a %b %c via tget
    b.fn_raw("tup", "i32", {}, "[ (const %x i32 1) (const %y i32 2) (const %z i32 3) (tuple %t [ %x %y %z ]) (tget %a i32 %t 0) (tget %b i32 %t 1) (tget %c i32 %t 2) (add %ab i32 %a %b) (add %res i32 %ab %c) (ret i32 %res) ]");
    b.end_module();
    auto prog = b.build(); auto ast = parse(prog.edn_text); auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res = tc.check_module(expanded);
    if(!res.success){ for(auto &e: res.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; return 1; }
    std::cout << "[rustlite-tuple-destructure] ok\n"; return 0;
}
