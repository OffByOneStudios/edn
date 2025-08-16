#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.hpp"

using namespace edn;
static void ok(const char* s){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); if(!res.success){ std::cerr<<"Unexpected union test failure:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(res.success); }
static void err(const char* s, const std::string& code){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); assert(!res.success); bool f=false; for(auto &e: res.errors) if(e.code==code) f=true; if(!f){ std::cerr<<"Expected code "<<code<<" not found. Errors:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(f); }

int run_phase3_union_tests(){
    std::cout << "[phase3] union ok...\n";
    ok("(module (union :name U :fields [ (ufield :name a :type i32) (ufield :name b :type (ptr i8)) ]) (fn :name \"main\" :ret void :params [] :body [ ]) )");
    std::cout << "[phase3] union missing name...\n";
    err("(module (union :fields [ (ufield :name a :type i32) ]) )","E1350");
    std::cout << "[phase3] union missing fields...\n";
    err("(module (union :name U) )","E1351");
    std::cout << "[phase3] union malformed field entry...\n";
    err("(module (union :name U :fields [ x ]) )","E1352");
    std::cout << "[phase3] union field missing type...\n";
    err("(module (union :name U :fields [ (ufield :name a) ]) )","E1352");
    std::cout << "[phase3] union duplicate field...\n";
    err("(module (union :name U :fields [ (ufield :name a :type i32) (ufield :name a :type i32) ]) )","E1355");
    std::cout << "[phase3] union unsupported field type...\n";
    err("(module (union :name U :fields [ (ufield :name s :type (struct-ref S)) ]) )","E1354");
    std::cout << "[phase3] union redefinition...\n";
    err("(module (union :name U :fields [ (ufield :name a :type i32) ]) (union :name U :fields [ (ufield :name b :type i32) ]) )","E1356");
    std::cout << "[phase3] union-member unknown union/base...\n";
    err("(module (union :name U :fields [ (ufield :name a :type i32) ]) (fn :name \"main\" :ret void :params [] :body [ (union-member %x U %p a) ]) )","E1358");
    std::cout << "[phase3] union-member unknown field...\n";
    err("(module (union :name U :fields [ (ufield :name a :type i32) ]) (fn :name \"main\" :ret void :params [] :body [ (alloca %p U) (union-member %x U %p b) ]) )","E1359");
    std::cout << "Phase 3 union tests passed\n";
    return 0;
}
