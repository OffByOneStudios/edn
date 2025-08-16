#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.hpp"

using namespace edn;

static void ok(const char* s){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); if(!res.success){ std::cerr<<"Unexpected failure:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(res.success); }
static void err(const char* s,const std::string& code){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); assert(!res.success); bool f=false; for(auto &e: res.errors) if(e.code==code) f=true; if(!f){ std::cerr<<"Expected "<<code<<" not found. Got:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(f); }

int run_phase3_typedef_tests(){
    // success: use alias in param and body
    ok("(module (typedef :name I :type i32) (fn :name \"f\" :ret I :params [ (param I %x) ] :body [ (ret I %x) ]) )");
    // error: missing name
    err("(module (typedef :type i32))","E1330");
    // error: missing type
    err("(module (typedef :name X))","E1331");
    // error: redefinition
    err("(module (typedef :name X :type i32) (typedef :name X :type i32))","E1332");
    // error: bad underlying type form (empty list)
    err("(module (typedef :name B :type ()))","E1333");
    return 0;
}
