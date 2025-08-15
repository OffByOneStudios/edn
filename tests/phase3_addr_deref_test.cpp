#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.inl"

using namespace edn;

static const char* mod_addr = R"((module
  (fn :name "take" :ret i32 :params [ (param i32 %v) ]
       :body [ (addr %p (ptr i32) %v) (deref %x i32 %p) (ret i32 %x) ])
))";

static const char* mod_addr_type_mismatch = R"((module
  (fn :name "bad" :ret i32 :params [ (param i32 %v) ]
       :body [ (addr %p (ptr i64) %v) (ret i32 %v) ])
))"; // E1315

static const char* mod_deref_bad_ptr = R"((module
  (fn :name "bad2" :ret i32 :params [ (param (ptr i32) %p) ]
       :body [ (deref %x i64 %p) (ret i32 %x) ])
))"; // E1319 mismatch

static void ok(const char* s){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); if(!res.success){ for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(res.success); }
static void fail(const char* s, const std::string& code){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); assert(!res.success); bool f=false; for(auto &e: res.errors) if(e.code==code) f=true; if(!f){ for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(f); }

void run_phase3_addr_deref_tests(){
  std::cout<<"[phase3] addr/deref success...\n"; ok(mod_addr);
  std::cout<<"[phase3] addr mismatch...\n"; fail(mod_addr_type_mismatch, "E1315");
  std::cout<<"[phase3] deref mismatch...\n"; fail(mod_deref_bad_ptr, "E1319");
  std::cout<<"Phase 3 addr/deref tests passed\n";
}
