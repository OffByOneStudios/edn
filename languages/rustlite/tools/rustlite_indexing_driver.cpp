#include <iostream>
#include <cassert>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    std::cout << "[rustlite-indexing] building demo...\n";
    Builder b; b.begin_module()
        .fn_raw("index_demo", "i32", {},
            "[ (alloca %arr (array :elem i32 :size 4)) "
            "  (const %i0 i32 0) (const %i1 i32 1) (const %i2 i32 2) (const %i3 i32 3) "
            "  (const %v10 i32 10) (const %v20 i32 20) (const %v30 i32 30) (const %v40 i32 40) "
            "  (rindex-store i32 %arr %i0 %v10) (rindex-store i32 %arr %i1 %v20) (rindex-store i32 %arr %i2 %v30) (rindex-store i32 %arr %i3 %v40) "
            "  (rindex-load %x i32 %arr %i2) (const %v30b i32 30) (eq %ok i32 %x %v30b) (rassert %ok) "
            "  (ret i32 %x) ]")
        .end_module();

    auto prog = b.build();
    auto ast = parse(prog.edn_text);
    auto expanded = rustlite::expand_rustlite(ast);

    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-indexing] type check failed\n";
        for(const auto &e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); (void)mod; assert(ir.success);
    std::cout << "[rustlite-indexing] ok\n";
    return 0;
}
