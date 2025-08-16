#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

static void test_positive_casts(){
    TypeContext ctx; IREmitter em(ctx);
    // integer extensions and truncation with non-constant source
    auto ast = parse("(module (fn :name \"casts\" :ret i32 :params [] :body [ (alloca %pa i8) (const %one i8 1) (store i8 %pa %one) (load %a i8 %pa) (zext %b i32 %a) (sext %c i32 %a) (trunc %d i8 %b) (ret i32 %b) ]))");
    TypeCheckResult r; auto *m=em.emit(ast,r); assert(r.success && m);
    auto fn = m->getFunction("casts"); assert(fn);
    size_t zc=0, sc=0, tc=0; for(auto &bb:*fn) for(auto &ins:bb){ auto name=std::string(ins.getOpcodeName()); if(name=="zext") ++zc; else if(name=="sext") ++sc; else if(name=="trunc") ++tc; }
    assert(zc==1 && sc==1 && tc==1);
    // float/int casts (avoid constant folding via load)
    auto ast2 = parse("(module (fn :name \"fc\" :ret f32 :params [] :body [ (alloca %pi i32) (const %ci i32 42) (store i32 %pi %ci) (load %i i32 %pi) (sitofp %f f32 %i) (fptosi %ri i32 %f) (ret f32 %f) ]))");
    TypeCheckResult r2; auto *m2=em.emit(ast2,r2); assert(r2.success && m2);
    auto fn2=m2->getFunction("fc"); assert(fn2); bool sawSIToFP=false,sawFPToSI=false; for(auto &bb:*fn2) for(auto &ins:bb){ if(std::string(ins.getOpcodeName())=="sitofp") sawSIToFP=true; if(std::string(ins.getOpcodeName())=="fptosi") sawFPToSI=true; } assert(sawSIToFP && sawFPToSI);
    // ptr/int casts
    auto ast3 = parse("(module (fn :name \"pc\" :ret i32 :params [] :body [ (alloca %p i32) (ptrtoint %pi i64 %p) (inttoptr %p2 (ptr i32) %pi) (const %z i32 0) (ret i32 %z) ]))");
    TypeCheckResult r3; auto *m3=em.emit(ast3,r3); assert(r3.success && m3);
    auto fn3=m3->getFunction("pc"); assert(fn3); bool sawP2I=false,sawI2P=false; for(auto &bb:*fn3) for(auto &ins:bb){ auto op=std::string(ins.getOpcodeName()); if(op=="ptrtoint") sawP2I=true; else if(op=="inttoptr") sawI2P=true; } assert(sawP2I && sawI2P);
}

static void test_negative_casts(){
    TypeContext ctx; TypeChecker tc(ctx);
    // invalid sext from unsigned
    auto bad1 = parse("(module (fn :name \"b1\" :ret i32 :params [] :body [ (const %a u8 1) (sext %b i32 %a) (ret i32 %b) ]))");
    auto r1=tc.check_module(bad1); assert(!r1.success);
    // invalid trunc direction
    auto bad2 = parse("(module (fn :name \"b2\" :ret i32 :params [] :body [ (const %a i32 1) (trunc %b i64 %a) (ret i32 %a) ]))");
    auto r2=tc.check_module(bad2); assert(!r2.success);
    // bitcast different widths
    auto bad3 = parse("(module (fn :name \"b3\" :ret i32 :params [] :body [ (const %a i32 1) (bitcast %b i64 %a) (ret i32 %a) ]))");
    auto r3=tc.check_module(bad3); assert(!r3.success);
    // ptrtoint wrong dest type (float)
    auto bad4 = parse("(module (fn :name \"b4\" :ret i32 :params [] :body [ (alloca %p i32) (ptrtoint %pi f32 %p) (ret i32 %pi) ]))");
    auto r4=tc.check_module(bad4); assert(!r4.success);
}

void run_cast_tests(){
    test_positive_casts();
    test_negative_casts();
    std::cout << "Cast tests passed\n";
}
