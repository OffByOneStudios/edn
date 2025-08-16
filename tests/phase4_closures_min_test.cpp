#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

// Minimal closure: function takes env first, then params. Closure produces a thunk fnptr that applies captured env at call time.
static const char* CLOSURE_SRC = R"((module
  (fn :name "add_env" :ret i32 :params [ (param i32 %env) (param i32 %x) ] :body [
     (add %sum i32 %env %x)
     (ret i32 %sum)
  ])
  (fn :name "main" :ret i32 :params [ ] :body [
     (const %ten i32 10)
     ; closure of add_env with env=10, type is pointer to (fn-type (params [ i32 ]) :ret i32)
     (closure %f (ptr (fn-type :params [ i32 ] :ret i32)) add_env [ %ten ])
    (const %five i32 5)
    ; no execution here; just ensure IR built and thunk synthesized
    (ret i32 %five)
  ])
))";

// For unit test we don't execute; we ensure type check & IR build succeeds and symbol types look right.
static void build_module(){
    auto ast = parse(CLOSURE_SRC);
    TypeContext ctx; TypeChecker tc(ctx); TypeCheckResult tcr = tc.check_module(ast);
    if(!tcr.success){
        std::cerr << "Closure test type check failed:\n"; for(auto &e: tcr.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; assert(false);
    }
    IREmitter emitter(ctx); TypeCheckResult res; auto *mod = emitter.emit(ast, res); assert(mod); assert(res.success);
    // verify thunk exists by scanning for a private function with prefix __edn.closure.thunk.
    bool foundThunk=false; for(auto &F : mod->functions()){ if(!F.isDeclaration()){
            auto name = F.getName().str(); if(name.rfind("__edn.closure.thunk.",0)==0){ foundThunk=true; break; }
        } }
    assert(foundThunk);
}

void run_phase4_closures_min_test(){
    std::cout << "[phase4] closures minimal test...\n";
    build_module();
    std::cout << "[phase4] closures minimal test passed\n";
}
