#include <iostream>
#include <cassert>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

// Negative: attempt to use an uncaptured symbol inside closure environment.
int main(){
    using namespace rustlite; using namespace edn;
    std::cout << "[rustlite-closure-neg] building demo...\n";
    Builder b; b.begin_module();
    b.fn_raw("adder", "i32", { {"env","i32"}, {"x","i32"} }, "[ (add %r i32 %env %x) (ret i32 %r) ]");
    // Intentionally reference %missing (never defined) inside closure capture list -> should surface undefined symbol diagnostic.
    b.fn_raw("make", "i32", {}, "[ (const %base i32 1) (rclosure %c adder :captures [ %missing ]) (ret i32 %base) ]");
    b.end_module();
    auto prog=b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto r=tc.check_module(expanded);
    if(r.success){ std::cerr<<"[rustlite-closure-neg] expected failure but type check passed\n"; return 1; }
    bool sawUndefined=false; for(auto &e: r.errors){ if(e.code=="E0102" || e.message.find("undefined")!=std::string::npos) sawUndefined=true; }
    if(!sawUndefined){ std::cerr<<"[rustlite-closure-neg] did not observe undefined symbol diagnostic\n"; return 2; }
    std::cout << "[rustlite-closure-neg] expected failure observed\n"; return 0;
}
