// Tuple/Array macro smoke driver exercising arr + rindex.
#include <iostream>
#include <cassert>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"
using namespace edn; using namespace rustlite;

int main(){
    // Basic module: build an array via (arr ...) then index it with (rindex ...)
    const char* src = R"((module :id "tuple_array_smoke"
        (rfn :name "main" :ret i32 :params [] :body [
            (const %i0 i32 0) (const %i1 i32 1) (const %i2 i32 2) (const %i3 i32 3)
            (arr %arr i32 [ %i0 %i1 %i2 %i3 ])
            (rindex %v i32 %arr %i2) ; load arr[2] == 2
            (ret i32 %v)
        ])
    ))";

    auto ast = parse(src);
    auto expanded = expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "(tuple/array smoke) typecheck failed\n";
        for(auto &e: tcres.errors) std::cerr << e.code << " " << e.message << "\n";
        return 1;
    }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); (void)mod; assert(ir.success);
    std::cout << "rustlite tuple/array smoke OK\n";
    return 0;
}