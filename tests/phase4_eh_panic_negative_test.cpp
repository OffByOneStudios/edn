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

void run_phase4_eh_panic_negative_test(){
    std::cout << "[phase4] panic negative tests...\n";
    // E1440: panic arity (takes no operands)
    {
        const char* SRC = R"((module
          (fn :name "main" :ret i32 :params [] :body [
            (const %z i32 0)
            (panic %z) ; invalid: panic takes no operands
            (ret i32 %z)
          ])
        ))";
        auto ast = parse(SRC);
        TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
        assert(!res.success);
        assert(has_code(res, "E1440"));
    }
    // Also ensure extra operands are rejected
    {
        const char* SRC = R"((module
          (fn :name "main" :ret i32 :params [] :body [
            (const %a i32 1)
            (const %b i32 2)
            (panic %a %b)
            (ret i32 %a)
          ])
        ))";
        auto ast = parse(SRC);
        TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
        assert(!res.success);
        assert(has_code(res, "E1440"));
    }
    std::cout << "[phase4] panic negative tests passed\n";
}
