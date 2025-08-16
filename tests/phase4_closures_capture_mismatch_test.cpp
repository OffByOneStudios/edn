#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"

using namespace edn;

static bool has_code(const TypeCheckResult& r, const std::string& code){
    for(const auto& e : r.errors){ if(e.code == code) return true; }
    return false;
}

void run_phase4_closures_capture_mismatch_test(){
    // Env type mismatch: function expects i32 env, we pass f32
    const char* SRC = R"((module
      (fn :name "adder" :ret i32 :params [ (param i32 %env) (param i32 %x) ] :body [ (add %s i32 %env %x) (ret i32 %s) ])
      (fn :name "main" :ret i32 :params [ ] :body [
         (const %tenf f32 10.0)
         (make-closure %c adder [ %tenf ])
         (ret i32 0)
      ])
    ))";
    auto ast = parse(SRC);
    TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
    assert(!res.success);
    assert(has_code(res, "E1434"));
    std::cout << "[phase4] closures capture mismatch negative test passed\n";
}
