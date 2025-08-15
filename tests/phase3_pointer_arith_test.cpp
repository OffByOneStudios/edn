#include <cassert>
#include <iostream>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.inl"

using namespace edn;

static const char* mod_ptr_add = R"((module
    (fn :name "padd" :ret i32 :params [ (param (ptr i32) %p) (param i32 %i) ]
        :body [ (ptr-add %q (ptr i32) %p %i) (const %zero i32 0) (ret i32 %zero) ])
))";

static const char* mod_ptr_sub = R"((module
    (fn :name "psub" :ret i32 :params [ (param (ptr i32) %p) (param i32 %i) ]
        :body [ (ptr-sub %q (ptr i32) %p %i) (const %zero i32 0) (ret i32 %zero) ])
))";

static const char* mod_ptr_diff = R"((module
    (fn :name "diff" :ret i64 :params [ (param (ptr i32) %a) (param (ptr i32) %b) ]
        :body [ (ptr-diff %d i64 %a %b) (ret i64 %d) ])
))";

static const char* mod_ptr_add_bad_base = R"((module
    (fn :name "bad" :ret i32 :params [ (param (ptr i32) %p) (param i32 %i) ]
        :body [ (ptr-add %q (ptr i32) p %i) (ret i32 %i) ])
))"; // missing % on base triggers E1302

static const char* mod_ptr_diff_mismatch = R"((module
    (fn :name "bad2" :ret i32 :params [ (param (ptr i32) %a) (param (ptr i64) %b) ]
        :body [ (ptr-diff %d i32 %a %b) (ret i32 %d) ])
))"; // mismatch triggers E1309

static void expect_success(const char* src){ auto ast=parse(src); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); if(!res.success){ std::cerr<<"Unexpected failure in success case\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(res.success); }
static void expect_error(const char* src, const std::string& code){ auto ast=parse(src); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); assert(!res.success); bool found=false; for(auto &e: res.errors){ if(e.code==code) found=true; } if(!found){ std::cerr<<"Expected code "<<code<<" not found. Got:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(found); }

void run_phase3_pointer_arith_tests(){
    std::cout << "[phase3] ptr-add...\n"; expect_success(mod_ptr_add);
    std::cout << "[phase3] ptr-sub...\n"; expect_success(mod_ptr_sub);
    std::cout << "[phase3] ptr-diff...\n"; expect_success(mod_ptr_diff);
    std::cout << "[phase3] ptr-add bad base...\n"; expect_error(mod_ptr_add_bad_base, "E1302");
    std::cout << "[phase3] ptr-diff mismatch...\n"; expect_error(mod_ptr_diff_mismatch, "E1309");
    std::cout << "Phase 3 pointer arithmetic tests passed\n"; }
