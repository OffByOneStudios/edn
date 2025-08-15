#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.inl"

using namespace edn;
static void ok(const char* s){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); if(!res.success){ std::cerr<<"Unexpected enum test failure:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(res.success); }
static void err(const char* s, const std::string& code){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); assert(!res.success); bool f=false; for(auto &e: res.errors) if(e.code==code) f=true; if(!f){ std::cerr<<"Expected code "<<code<<" not found. Errors:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(f); }

int run_phase3_enum_tests(){
    std::cout << "[phase3] enum ok...\n";
    ok("(module (enum :name Color :underlying u8 :values [ (eval :name RED :value 0) (eval :name BLUE :value 1) ]) )");
    // usage in const
    ok("(module (enum :name N :underlying i32 :values [ (eval :name A :value 7) ]) (fn :name \"main\" :ret void :params [] :body [ (const %c i32 A) ]) )");
    std::cout << "[phase3] enum missing name...\n";
    err("(module (enum :underlying i32 :values [ (eval :name A :value 0) ]) )","E1340");
    std::cout << "[phase3] enum missing underlying...\n";
    err("(module (enum :name E :values [ (eval :name A :value 0) ]) )","E1341");
    std::cout << "[phase3] enum bad underlying...\n";
    err("(module (enum :name E :underlying f32 :values [ (eval :name A :value 0) ]) )","E1342");
    std::cout << "[phase3] enum missing values...\n";
    err("(module (enum :name E :underlying i32))","E1343");
    std::cout << "[phase3] enum missing constant value...\n";
    err("(module (enum :name E :underlying i32 :values [ (eval :name A) ]) )","E1344");
    std::cout << "[phase3] enum duplicate constant...\n";
    err("(module (enum :name E :underlying i32 :values [ (eval :name A :value 0) (eval :name A :value 1) ]) )","E1345");
    std::cout << "[phase3] enum unknown constant usage...\n";
    err("(module (enum :name C :underlying i32 :values [ (eval :name A :value 1) ]) (fn :name \"main\" :ret void :params [] :body [ (const %x i32 B) ]) )","E1349");
    std::cout << "Phase 3 enum tests passed\n";
    return 0;
}
