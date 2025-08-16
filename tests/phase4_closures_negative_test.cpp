#include <cassert>
#include <iostream>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"

using namespace edn;

static bool has_code(const TypeCheckResult& r, const std::string& code){
    for(const auto& e : r.errors){ if(e.code == code) return true; }
    return false;
}

// Negative tests for make-closure and call-closure validation
void run_phase4_closures_negative_tests(){
    // E1436: make-closure arity (missing captures vector)
    {
        const char* SRC = R"((module
          (fn :name "adder" :ret i32 :params [ (param i32 %env) (param i32 %x) ] :body [ (add %s i32 %env %x) (ret i32 %s) ])
          (fn :name "main" :ret i32 :params [ ] :body [
             (const %ten i32 10)
             (make-closure %c adder) ; missing [ %env ] vector
             (ret i32 %ten)
          ])
        ))";
        auto ast = parse(SRC);
        TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
        assert(!res.success);
        assert(has_code(res, "E1436"));
    }

    // E1437: call-closure arg count mismatch (no user args provided)
    {
        const char* SRC = R"((module
          (fn :name "adder" :ret i32 :params [ (param i32 %env) (param i32 %x) ] :body [ (add %s i32 %env %x) (ret i32 %s) ])
          (fn :name "main" :ret i32 :params [ ] :body [
             (const %ten i32 10)
             (make-closure %c adder [ %ten ])
             (call-closure %res i32 %c) ; missing one user arg
             (ret i32 %ten)
          ])
        ))";
        auto ast = parse(SRC);
        TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
        assert(!res.success);
        assert(has_code(res, "E1437"));
    }

    // E1437: call-closure return type mismatch
    {
        const char* SRC = R"((module
          (fn :name "adder" :ret i32 :params [ (param i32 %env) (param i32 %x) ] :body [ (add %s i32 %env %x) (ret i32 %s) ])
          (fn :name "main" :ret i32 :params [ ] :body [
             (const %ten i32 10)
             (const %five i32 5)
             (make-closure %c adder [ %ten ])
             (call-closure %res i64 %c %five) ; wrong annotated return type
             (ret i32 %ten)
          ])
        ))";
        auto ast = parse(SRC);
        TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
        assert(!res.success);
        assert(has_code(res, "E1437"));
    }

    std::cout << "[phase4] closures negative tests passed\n";
}
