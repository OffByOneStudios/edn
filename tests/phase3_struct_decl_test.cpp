#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"

using namespace edn;
static void ok(const char* s){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); if(!res.success){ std::cerr<<"Unexpected struct test failure:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(res.success); }
static void err(const char* s, const std::string& code){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); assert(!res.success); bool f=false; for(auto &e: res.errors) if(e.code==code) f=true; if(!f){ std::cerr<<"Expected code "<<code<<" not found. Errors:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(f); }

int run_phase3_struct_decl_tests(){
    std::cout << "[phase3] struct ok...\n";
    ok("(module (struct :name S :fields [ (field :name a :type i32) (field :name b :type f32) ]) )");
    std::cout << "[phase3] struct missing name...\n"; err("(module (struct :fields [ (field :name a :type i32) ]) )","E1400");
    std::cout << "[phase3] struct missing fields...\n"; err("(module (struct :name S) )","E1401");
    std::cout << "[phase3] struct malformed field...\n"; err("(module (struct :name S :fields [ x ]) )","E1402");
    std::cout << "[phase3] struct field missing name...\n"; err("(module (struct :name S :fields [ (field :type i32) ]) )","E1403");
    std::cout << "[phase3] struct field missing type...\n"; err("(module (struct :name S :fields [ (field :name a) ]) )","E1404");
    std::cout << "[phase3] struct duplicate field...\n"; err("(module (struct :name S :fields [ (field :name a :type i32) (field :name a :type i32) ]) )","E1405");
    std::cout << "[phase3] struct redefinition...\n"; err("(module (struct :name S :fields [ (field :name a :type i32) ]) (struct :name S :fields [ (field :name b :type i32) ]) )","E1406");
    std::cout << "[phase3] struct empty fields...\n"; err("(module (struct :name S :fields [ ]) )","E1407");
    std::cout << "Phase 3 struct decl tests passed\n";
    return 0;
}
