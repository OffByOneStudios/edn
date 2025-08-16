#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.hpp"

using namespace edn;

static const char* mod_fnptr_ok = R"((module
  (fn :name "add" :ret i32 :params [ (param i32 %a) (param i32 %b) ] :body [ (add %s i32 %a %b) (ret i32 %s) ])
  (fn :name "use" :ret i32 :params [ (param i32 %x) (param i32 %y) ]
       :body [ (fnptr %fp (ptr (fn-type :params [ i32 i32 ] :ret i32)) add)
               (call-indirect %r i32 %fp %x %y)
               (ret i32 %r) ])
))";

static const char* mod_fnptr_sig_mismatch = R"((module
  (fn :name "add" :ret i32 :params [ (param i32 %a) (param i32 %b) ] :body [ (add %s i32 %a %b) (ret i32 %s) ])
  (fn :name "use" :ret i32 :params [ (param i32 %x) (param i32 %y) ]
       :body [ (fnptr %fp (ptr (fn-type :params [ i32 ] :ret i32)) add) (ret i32 %x) ])
))"; // E1324

static const char* mod_call_indirect_bad_count = R"((module
  (fn :name "add" :ret i32 :params [ (param i32 %a) (param i32 %b) ] :body [ (add %s i32 %a %b) (ret i32 %s) ])
  (fn :name "use" :ret i32 :params [ (param i32 %x) ]
       :body [ (fnptr %fp (ptr (fn-type :params [ i32 i32 ] :ret i32)) add)
               (call-indirect %r i32 %fp %x) (ret i32 %x) ])
))"; // E1325

static void ok(const char* s){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); if(!res.success){ for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(res.success); }
static void err(const char* s, const std::string& code){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); assert(!res.success); bool f=false; for(auto &e: res.errors) if(e.code==code) f=true; if(!f){ for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(f); }

void run_phase3_fnptr_tests(){
  std::cout<<"[phase3] fnptr ok...\n"; ok(mod_fnptr_ok);
  std::cout<<"[phase3] fnptr signature mismatch...\n"; err(mod_fnptr_sig_mismatch, "E1324");
  std::cout<<"[phase3] call-indirect arg count mismatch...\n"; err(mod_call_indirect_bad_count, "E1325");
  std::cout<<"Phase 3 function pointer tests passed\n";
}
