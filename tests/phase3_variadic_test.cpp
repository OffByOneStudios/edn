#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.inl"

using namespace edn;
static void ok(const char* s){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); if(!res.success){ std::cerr<<"Unexpected variadic test failure:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(res.success); }
static void err(const char* s, const std::string& code){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); assert(!res.success); bool f=false; for(auto &e: res.errors) if(e.code==code) f=true; if(!f){ std::cerr<<"Expected code "<<code<<" not found. Errors:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(f); }

int run_phase3_variadic_tests(){
    std::cout << "[phase3] variadic ok...\n";
    ok("(module (fn :name \"sum\" :ret i32 :params [ (param i32 %a) (param i32 %b) ] :vararg true :body [ (add %t i32 %a %b) (ret i32 %t) ]) )");
    // E1360: missing required fixed args (declares 2 fixed, provides 1)
    err("(module (fn :name \"sum\" :ret i32 :params [ (param i32 %a) (param i32 %b) ] :vararg true :body [ (const %x i32 1) (call %r i32 sum %x) (ret i32 %x) ]) )","E1360");
    // E1361: variadic extra arg not %var symbol
    err("(module (fn :name \"sum\" :ret i32 :params [ (param i32 %a) ] :vararg true :body [ (const %x i32 1) (call %r i32 sum %x 123) (ret i32 %x) ]) )","E1361");
    // E1362: variadic extra arg undefined var
    err("(module (fn :name \"sum\" :ret i32 :params [ (param i32 %a) ] :vararg true :body [ (call %r i32 sum %a %b) (ret i32 %a) ]) )","E1362");
    // Intrinsic errors: va-start outside variadic
    err("(module (fn :name \"foo\" :ret void :params [] :body [ (va-start %ap) (ret void %ap) ]) )","E1364");
    // Intrinsic errors: va-arg outside variadic
    err("(module (fn :name \"foo\" :ret void :params [] :body [ (va-arg %x i32 %ap) (ret void %x) ]) )","E1366");
    // Intrinsic errors: va-end outside variadic
    err("(module (fn :name \"foo\" :ret void :params [] :body [ (va-end %ap) (ret void %ap) ]) )","E1369");
    // Successful intrinsic usage pattern (synthetic; va-arg returns undef currently)
    ok("(module (fn :name \"sum\" :ret i32 :params [ (param i32 %count) ] :vararg true :body [ (va-start %ap) (const %acc i32 0) (ret i32 %acc) ]) )");
    std::cout << "Phase 3 variadic tests passed (restored)\n";
    return 0;
}
