// Runtime-style test for variadic sum (placeholder; va-arg currently returns undef)
#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

static const char* VARIADIC_SUM_SRC = R"((module
  (fn :name "sumN" :ret i32 :params [ (param i32 %n) ] :vararg true :body [
     (va-start %ap)
     (const %acc i32 0)
     ; A future loop would iterate %n times doing (va-arg %tmp i32 %ap) and add.
     (ret i32 %acc)
  ])
))";

static void build_module(){
    auto ast = parse(VARIADIC_SUM_SRC);
    TypeContext ctx; TypeChecker tc(ctx); TypeCheckResult tcr = tc.check_module(ast); if(!tcr.success){
        std::cerr << "Variadic runtime test type check failed:\n"; for(auto &e: tcr.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; assert(false); }
    IREmitter emitter(ctx); TypeCheckResult res; auto *mod = emitter.emit(ast, res); assert(mod); assert(res.success);
    // Verify function signature is variadic in LLVM IR
    auto *F = mod->getFunction("sumN"); assert(F); assert(F->isVarArg());
    (void)F; // silence unused warning in release builds when asserts are stripped
}

int run_phase3_variadic_runtime_test(){
    std::cout << "[phase3] variadic runtime test...\n";
    build_module();
    std::cout << "[phase3] variadic runtime test passed\n";
    return 0;
}
