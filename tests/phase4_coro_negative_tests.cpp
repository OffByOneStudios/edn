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

void run_phase4_coro_negative_tests(){
    // E1460: coro-begin arity
    {
        const char* SRC = R"((module
          (fn :name "co" :ret i32 :params [ ] :body [ (coro-begin %h %extra) ])
        ))";
        auto ast = parse(SRC); TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
        assert(!res.success); assert(has_code(res, "E1460"));
    }
    // E1461: coro-begin dst must be %var
    {
        const char* SRC = R"((module
          (fn :name "co" :ret i32 :params [ ] :body [ (coro-begin h) ])
        ))";
        auto ast = parse(SRC); TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
        assert(!res.success); assert(has_code(res, "E1461"));
    }
    // E1462/E1463: coro-suspend arity and handle must be %var
    {
        const char* SRC = R"((module
          (fn :name "co" :ret i32 :params [ ] :body [ (coro-begin %h) (coro-suspend %st h) ])
        ))";
        auto ast = parse(SRC); TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
        assert(!res.success); assert(has_code(res, "E1463"));
    }
    // E1463: handle undefined
    {
        const char* SRC = R"((module
          (fn :name "co" :ret i32 :params [ ] :body [ (coro-suspend %st %h) ])
        ))";
        auto ast = parse(SRC); TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
        assert(!res.success); assert(has_code(res, "E1463"));
    }
    std::cout << "[phase4] coroutines negative tests passed\n";
}
