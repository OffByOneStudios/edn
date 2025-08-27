#include <iostream>
#include <cassert>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    std::cout << "[rustlite-closure] building demo...\n";
    Builder b; b.begin_module();
    // Callee taking env + arg
    b.fn_raw("adder", "i32", { {"env","i32"}, {"x","i32"} }, "[ (add %r i32 %env %x) (ret i32 %r) ]");
    b.fn_raw("make_and_call", "i32", {},
        "[ (const %capt i32 7) (const %arg i32 5) (rclosure %c adder :captures [ %capt ]) (rcall-closure %res i32 %c %arg) (const %twelve i32 12) (eq %ok i32 %res %twelve) (rassert %ok) (ret i32 %res) ]");
    b.end_module();

    auto prog=b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto r=tc.check_module(expanded);
    if(!r.success){
        std::cerr << "[rustlite-closure] type check failed\n"; for(auto &e:r.errors){ std::cerr<<e.code<<": "<<e.message<<"\n"; } return 1; }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod=em.emit(expanded, ir); (void)mod; assert(ir.success);
    std::cout << "[rustlite-closure] ok\n"; return 0;
}
