#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

// Test closure record + call-closure path
static const char* SRC = R"((module
  (fn :name "adder" :ret i32 :params [ (param i32 %env) (param i32 %x) ] :body [
     (add %sum i32 %env %x)
     (ret i32 %sum)
  ])
  (fn :name "main" :ret i32 :params [ ] :body [
     (const %ten i32 10)
     (make-closure %c adder [ %ten ])
     (const %five i32 5)
     (call-closure %res i32 %c %five)
     (ret i32 %res)
  ])
))";

static void build_module(){
    auto ast = parse(SRC);
    TypeContext ctx; TypeChecker tc(ctx); TypeCheckResult tcr = tc.check_module(ast);
    if(!tcr.success){
        std::cerr << "Record closure test type check failed:\n";
        for(auto &e: tcr.errors) std::cerr<<e.code<<" "<<e.message<<"\n";
        assert(false);
    }
    IREmitter emitter(ctx); TypeCheckResult res; auto *mod = emitter.emit(ast, res); assert(mod); assert(res.success);
    // Ensure closure struct type was created
    bool foundStruct=false; for(auto &S : mod->getIdentifiedStructTypes()){ if(S->hasName() && S->getName().starts_with("struct.__edn.closure.adder")){ foundStruct=true; break; } }
    assert(foundStruct);
}

void run_phase4_closures_record_test(){
    std::cout << "[phase4] closures record test...\n";
    build_module();
    std::cout << "[phase4] closures record test passed\n";
}
