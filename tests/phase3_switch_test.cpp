#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.hpp"

using namespace edn;

static void ok(const char* src){ auto ast=parse(src); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); if(!res.success){ std::cerr<<"Unexpected switch success failure:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(res.success); }
static void err(const char* src, const std::string& code){ auto ast=parse(src); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); assert(!res.success); bool found=false; for(auto &e: res.errors) if(e.code==code) found=true; if(!found){ std::cerr<<"Expected code "<<code<<" not found. Errors:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(found); }

void run_phase3_switch_tests(){
    std::cout << "[phase3] switch ok...\n";
    ok("(module (fn :name \"m\" :ret i32 :params [ (param i32 %x) ] :body [ (switch %x :cases [ (case 0 [ (const %z i32 0) (ret i32 %z) ]) (case 1 [ (const %o i32 1) (ret i32 %o) ]) ] :default [ (const %d i32 2) (ret i32 %d) ]) ]) )");
    std::cout << "[phase3] switch missing expr...\n"; err("(module (fn :name \"m\" :ret i32 :params [] :body [ (switch :cases [] :default []) ]) )","E1390");
    std::cout << "[phase3] switch expr not %...\n"; err("(module (fn :name \"m\" :ret i32 :params [] :body [ (const %x i32 1) (switch x :cases [] :default []) ]) )","E1391");
    std::cout << "[phase3] switch expr not int...\n"; err("(module (fn :name \"m\" :ret i32 :params [ (param f32 %f) ] :body [ (switch %f :cases [] :default []) ]) )","E1392");
    std::cout << "[phase3] switch missing cases...\n"; err("(module (fn :name \"m\" :ret i32 :params [ (param i32 %x) ] :body [ (switch %x :default []) ]) )","E1393");
    std::cout << "[phase3] switch cases not vector...\n"; err("(module (fn :name \"m\" :ret i32 :params [ (param i32 %x) ] :body [ (switch %x :cases 7 :default []) ]) )","E1394");
    std::cout << "[phase3] switch case malformed...\n"; err("(module (fn :name \"m\" :ret i32 :params [ (param i32 %x) ] :body [ (switch %x :cases [ (bad 0 []) ] :default []) ]) )","E1395");
    std::cout << "[phase3] switch duplicate case...\n"; err("(module (fn :name \"m\" :ret i32 :params [ (param i32 %x) ] :body [ (switch %x :cases [ (case 0 [ ]) (case 0 [ ]) ] :default []) ]) )","E1396");
    std::cout << "[phase3] switch missing default...\n"; err("(module (fn :name \"m\" :ret i32 :params [ (param i32 %x) ] :body [ (switch %x :cases [ (case 0 [ ]) ]) ]) )","E1397");
    std::cout << "Phase 3 switch tests passed\n";
}
