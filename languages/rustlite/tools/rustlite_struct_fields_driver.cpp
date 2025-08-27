#include <iostream>
#include <cassert>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    std::cout << "[rustlite-struct-fields] building demo...\n";
    Builder b; b.begin_module()
        .rstruct("Pair", { {"a","i32"}, {"b","i32"} })
        .fn_raw("pair_demo", "i32", {},
            "[ (alloca %p Pair) "
            "  (const %zero i32 0) (rset i32 Pair %p a %zero) (rset i32 Pair %p b %zero) "
            "  (const %ten i32 10) (rset i32 Pair %p a %ten) "
            "  (const %twenty i32 20) (rset i32 Pair %p b %twenty) "
            "  (rget %va Pair %p a) (rget %vb Pair %p b) (add %sum i32 %va %vb) "
            "  (const %expected i32 30) (eq %cmp i32 %sum %expected) (rassert %cmp) "
            "  (ret i32 %sum) ]")
        .end_module();

    auto prog = b.build();
    auto ast = parse(prog.edn_text);
    auto expanded = rustlite::expand_rustlite(ast);

    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-struct-fields] type check failed\n";
        for(const auto &e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); (void)mod; assert(ir.success);
    std::cout << "[rustlite-struct-fields] ok\n";
    return 0;
}
