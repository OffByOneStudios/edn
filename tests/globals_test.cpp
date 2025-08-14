// Tests for M5: const globals and initializer validation
#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.inl"
#include "edn/ir_emitter.hpp"

using namespace edn;

static void test_scalar_const_global(){
    TypeContext ctx; IREmitter em(ctx);
    auto ast = parse("(module (global :name G :type i32 :init 7 :const true) (fn :name \"use\" :ret i32 :params [] :body [ (const %z i32 0) (ret i32 %z) ]))");
    TypeCheckResult r; auto *m=em.emit(ast,r); bool ok = r.success; if(!ok){ for(auto &e: r.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; }
    assert(ok); if(m){ auto *gv=m->getGlobalVariable("G"); assert(gv && gv->isConstant()); }
}

static void test_array_const_global(){
    TypeContext ctx; IREmitter em(ctx);
    // Function does not attempt to load the array (no array -> scalar load supported yet)
    auto ast = parse("(module (global :name A :type (array :elem i32 :size 3) :init [1 2 3] :const true) (fn :name \"use\" :ret i32 :params [] :body [ (const %z i32 0) (ret i32 %z) ]))");
    TypeCheckResult r; auto *m=em.emit(ast,r); if(!r.success){ std::cerr << "array const global failed with "<< r.errors.size() << " errors\n"; for(auto &e: r.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; }
    assert(r.success); if(m){ auto *gv=m->getGlobalVariable("A"); assert(gv && gv->isConstant()); } }

static void test_struct_const_global(){
    TypeContext ctx; IREmitter em(ctx);
    // Function does not attempt to load the struct value (no struct aggregate load/return yet)
    auto ast = parse("(module (struct :name P :fields [ (field :name x :type i32) (field :name y :type f32) ]) (global :name S :type (struct-ref P) :init [1 2.0] :const true) (fn :name \"use\" :ret i32 :params [] :body [ (const %z i32 0) (ret i32 %z) ]))");
    TypeCheckResult r; auto *m=em.emit(ast,r); if(!r.success){ std::cerr << "struct const global failed with "<< r.errors.size() << " errors\n"; for(auto &e: r.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; }
    assert(r.success); if(m){ auto *gv=m->getGlobalVariable("S"); assert(gv && gv->isConstant()); } }

static void test_negative_const_store(){
    TypeContext ctx; TypeChecker tc(ctx);
    auto ast = parse("(module (global :name G :type i32 :init 1 :const true) (fn :name \"bad\" :ret i32 :params [] :body [ (const %c i32 2) (gstore i32 G %c) (ret i32 %c) ]))");
    auto res = tc.check_module(ast); assert(!res.success); }

static void test_negative_array_init_mismatch(){
    TypeContext ctx; TypeChecker tc(ctx);
    auto ast = parse("(module (global :name A :type (array :elem i32 :size 4) :init [1 2 3] :const true) (fn :name \"f\" :ret i32 :params [] :body []))");
    auto res = tc.check_module(ast); assert(!res.success); }

void run_globals_tests(){
    test_scalar_const_global();
    test_array_const_global();
    test_struct_const_global();
    test_negative_const_store();
    test_negative_array_init_mismatch();
    std::cout << "Globals tests passed\n";
}
