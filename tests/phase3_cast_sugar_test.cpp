// Phase 3 cast sugar tests for (as ...)
#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.inl"
#include "edn/ir_emitter.hpp"

using namespace edn;

static void test_cast_sugar_positive(){
    auto ast = parse(R"((module (fn :name "csg" :ret i32 :params [] :body [
        (const %a u8 10)
        (as %w i32 %a)
        (as %n i8 %w)
        (as %f f32 %w)
        (ret i32 %w)
    ])))");
    TypeContext ctx; IREmitter em(ctx); TypeCheckResult r; auto *m=em.emit(ast,r); assert(r.success && m); auto *fn=m->getFunction("csg"); assert(fn);
    bool sawZext=false,sawTrunc=false,sawUIToFP=false,sawSIToFP=false; const char* dbgEnv = std::getenv("EDN_DEBUG_CAST");
    for(auto &bb:*fn){ for(auto &ins:bb){ std::string nm=ins.getOpcodeName(); if(dbgEnv && dbgEnv[0]=='1'){ std::cerr << "[cast-debug] op=" << nm << "\n"; }
        if(nm=="zext") sawZext=true; else if(nm=="trunc") sawTrunc=true; else if(nm=="uitofp") sawUIToFP=true; else if(nm=="sitofp") sawSIToFP=true; } }
    if(dbgEnv && dbgEnv[0]=='1'){ std::cerr << "[cast-debug] sawZext="<<sawZext<<" sawTrunc="<<sawTrunc<<" sawUIToFP="<<sawUIToFP<<" sawSIToFP="<<sawSIToFP<<"\n"; }
    assert(sawZext && sawTrunc && (sawUIToFP || sawSIToFP));
}

static void test_cast_sugar_negative(){
    TypeContext ctx; TypeChecker tc(ctx);
    auto bad = parse(R"((module (fn :name "bad" :ret f32 :params [] :body [ (const %a f32 1) (as %b f64 %a) (ret f32 %a) ])))");
    auto r=tc.check_module(bad); assert(!r.success); bool saw=false; for(auto &e: r.errors) if(e.code=="E13A4") saw=true; assert(saw);
}

int run_phase3_cast_sugar_test(){
    std::cout << "[phase3] cast sugar tests...\n";
    test_cast_sugar_positive();
    test_cast_sugar_negative();
    std::cout << "[phase3] cast sugar tests passed\n";
    return 0;
}
