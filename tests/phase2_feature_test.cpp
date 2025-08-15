#include <cassert>
#include <string>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.inl"

using namespace edn;

// Embedded Phase 2 sample modules (mirrors files under edn/phase2 for self-contained test)
static const char* sample_floats = R"((module :id "floats"
    (fn :name "mix" :ret f32 :params [ (param f32 %a) (param f32 %b) (param f32 %t) ]
            :body [ (fsub %one f32 %b %a) (fmul %scaled f32 %one %t) (fadd %out f32 %a %scaled) (ret f32 %out) ])
    (fn :name "main" :ret f32 :params []
            :body [ (const %a f32 1.0) (const %b f32 3.0) (const %t f32 0.25) (call %r f32 mix %a %b %t) (ret f32 %r) ])
))";
static const char* sample_unsigned = R"((module :id "unsigned"
    (fn :name "wrap" :ret u32 :params [ (param u32 %x) ]
            :body [ (const %m u32 4294967295) (add %s u32 %x %m) (ret u32 %s) ])
    (fn :name "main" :ret u32 :params [] :body [ (const %z u32 5) (call %r u32 wrap %z) (ret u32 %r) ])
))";
static const char* sample_casts = R"((module :id "casts"
    (fn :name "do" :ret i32 :params [ (param i8 %a) (param u8 %b) ]
            :body [ (zext %az i32 %a) (zext %bz i32 %b) (add %s i32 %az %bz) (trunc %t i8 %s) (sext %sx i32 %t) (ret i32 %sx) ])
    (fn :name "main" :ret i32 :params []
            :body [ (const %c1 i8 5) (const %c2 u8 9) (call %r i32 do %c1 %c2) (ret i32 %r) ])
))";
static const char* sample_phi = R"((module :id "phi"
    (fn :name "abs" :ret i32 :params [ (param i32 %x) ]
            :body [ (const %zero i32 0) (lt %neg i32 %x %zero)
                            (if %neg [ (sub %n i32 %zero %x) (ret i32 %n) ])
                            (ret i32 %x) ])
))";
static const char* sample_suggestions_invalid = R"((module :id "suggestions-invalid"
    (global :name GLOB1 :type i32 :init 1)
    (fn :name "bad" :ret i32 :params [] :body [ (gload %v i32 GLOB ) (ret i32 %v) ])
))";

static void check_success(const std::string &src){
    auto ast = parse(src);
    TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
    if(!res.success){
        std::cerr << "Unexpected failure. Errors:\n";
        for(auto &e: res.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
    }
    assert(res.success);
}

static void check_failure_code(const std::string &src, const std::string &code){
    auto ast = parse(src);
    TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
    assert(!res.success);
    bool found=false; for(auto &e: res.errors){ if(e.code==code) found=true; }
    if(!found){
        std::cerr << "Did not find expected error code " << code << "\n";
        for(auto &e: res.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
    }
    assert(found);
}

void run_phase2_feature_tests(){
    std::cout << "[phase2] floats...\n";
    check_success(sample_floats);
    std::cout << "[phase2] unsigned...\n";
    check_success(sample_unsigned);
    std::cout << "[phase2] casts...\n";
    check_success(sample_casts);
    std::cout << "[phase2] phi...\n";
    check_success(sample_phi);
    std::cout << "[phase2] suggestions invalid...\n";
    check_failure_code(sample_suggestions_invalid, "E0902");
    std::cout << "Phase 2 feature tests passed\n";
}
