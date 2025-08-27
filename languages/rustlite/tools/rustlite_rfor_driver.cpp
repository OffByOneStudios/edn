#include <iostream>
#include <cassert>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

// Driver to exercise rfor macro: counts i from 0 to 5 (exclusive limit)
// Loop shape:
//   init:  i=0, limit=5, one=1, cond = i < limit
//   cond:  %cond symbol
//   step:  i = i + 1; cond = i < limit
//   body:  (empty)
// Post-loop: i should equal 5
int main(){
    using namespace rustlite; using namespace edn;
    std::cout << "[rustlite-rfor] building demo...\n";
    Builder b; b.begin_module();
    b.fn_raw("rfor_demo", "i32", {},
        "[ "
        "  (rfor :init [ (const %i i32 0) (const %limit i32 5) (const %one i32 1) (lt %cond i32 %i %limit) ] "
        "        :cond %cond "
        "        :step [ (add %next i32 %i %one) (assign %i %next) (lt %tmpcond i32 %i %limit) (assign %cond %tmpcond) ] "
        "        :body [ ] ) "
        "  (const %five i32 5) (eq %cmp i32 %i %five) (rassert %cmp) "
        "  (ret i32 %i) "
        "]");
    b.end_module();

    auto prog = b.build();
    auto ast = parse(prog.edn_text);
    auto expanded = rustlite::expand_rustlite(ast);

    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-rfor] type check failed\n";
        for(const auto &e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }
    IREmitter em(tctx); TypeCheckResult ir;
    auto *mod = em.emit(expanded, ir); (void)mod; assert(ir.success);
    std::cout << "[rustlite-rfor] ok\n";
    return 0;
}
