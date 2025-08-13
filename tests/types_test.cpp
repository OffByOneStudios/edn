// Tests for type system scaffolding
#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

void run_type_tests(){
    TypeContext ctx;
    auto t_i32 = ctx.parse_type(parse("i32"));
    auto t_i32_b = ctx.parse_type(parse("i32"));
    assert(t_i32 == t_i32_b);
    (void)t_i32; (void)t_i32_b;
    auto p1 = ctx.parse_type(parse("(ptr i32)"));
    auto p2 = ctx.parse_type(parse("(ptr :to i32)"));
    assert(p1 == p2);
    (void)p1; (void)p2; // silence unused warnings in Release
    auto s_point = ctx.parse_type(parse("(struct-ref Point)"));
    auto s_point2 = ctx.parse_type(parse("Point"));
    assert(s_point == s_point2);
    (void)s_point; (void)s_point2;
    auto fn = ctx.parse_type(parse("(fn-type :params [i32 (ptr i32)] :ret i64)"));
    auto fn2 = ctx.parse_type(parse("(fn-type :params [i32 (ptr i32)] :ret i64)"));
    assert(fn == fn2);
    (void)fn; (void)fn2;
    auto fnv = ctx.parse_type(parse("(fn-type :params [i32] :ret void :variadic true)"));
    assert(fn != fnv);
    (void)fnv;
    auto arr = ctx.parse_type(parse("(array :elem i32 :size 4)"));
    auto arr2 = ctx.parse_type(parse("(array :elem i32 :size 4)"));
    assert(arr == arr2);
    (void)arr; (void)arr2;
    std::cout << "Type tests passed\n";

    // Simple IR emitter smoke: empty function module
    auto ast = parse("(module :id \"m\" (fn :name \"f\" :ret i32 :params [] :body [ (const %c i32 0) (ret i32 %c) ]))");
    edn::TypeChecker tc(ctx); auto res = tc.check_module(ast); assert(res.success);
    edn::IREmitter emitter(ctx); edn::TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres); assert(tcres.success); (void)mod;
}
