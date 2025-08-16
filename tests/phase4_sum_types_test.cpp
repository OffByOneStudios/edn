#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"

using namespace edn;

static void ok(const char* s){ auto ast=parse(s); TypeContext ctx; TypeChecker tc(ctx); auto res=tc.check_module(ast); if(!res.success){ std::cerr<<"Unexpected sum test failure:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; } assert(res.success); }
static void err(const char* s, const std::string& code){
    std::cerr << "[err] parsing...\n" << std::flush;
    auto ast=parse(s);
    std::cerr << "[err] parsed. checking...\n" << std::flush;
    TypeContext ctx; TypeChecker tc(ctx);
    auto res=tc.check_module(ast);
    std::cerr << "[err] checked. success=" << (res.success?"true":"false") << "\n" << std::flush;
    if(res.success){ std::cerr<<"ERR helper: got success unexpectedly for program:\n"<<s<<"\n"<< std::flush; }
    assert(!res.success);
    bool f=false; for(auto &e: res.errors){ std::cerr << "[err] code="<<e.code<<" msg="<<e.message<<"\n"; if(e.code==code) f=true; }
    if(!f){ std::cerr<<"Expected code "<<code<<" not found.\n"<< std::flush; }
    assert(f);
}

void run_phase4_sum_types_tests(){
    std::cout << "[phase4] sum ok...\n";
    ok("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) (variant :name B :fields [ i64 f64 ]) ]) (fn :name \"main\" :ret void :params [] :body [ ]) )");
    std::cout << "[phase4] sum missing name...\n";
    err("(module (sum :variants [ (variant :name A :fields [ i32 ]) ]) )","E1400");
    std::cout << "[phase4] sum missing variants...\n";
    err("(module (sum :name T) )","E1402");
    std::cout << "[phase4] variant malformed...\n";
    err("(module (sum :name T :variants [ x ]) )","E1403");
    std::cout << "[phase4] variant missing name...\n";
    err("(module (sum :name T :variants [ (variant :fields [ i32 ]) ]) )","E1404");
    std::cout << "[phase4] sum-new shape/type checks...\n";
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"main\" :ret void :params [ (param i32 %x) ] :body [ (sum-new %s T A [ %y ]) ]) )","E1409");
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"main\" :ret void :params [ (param i32 %x) ] :body [ (sum-new %s T B [ %x ]) ]) )","E1405");
    std::cout << "[phase4] sum-is shape checks...\n";
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"main\" :ret void :params [ (param i32 %x) ] :body [ (sum-is %ok T %x A) ]) )","E1409");
    std::cout << "[phase4] sum-get shape/type checks...\n";
    // arity and index validation
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (sum-get %v T %p A) ]) )","E1409");
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (sum-get %v T %p B 0) ]) )","E1405");
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (sum-get %v T %p A -1) ]) )","E1409");
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (sum-get %v T %x A 0) ]) )","E1409");
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (sum-get %v T %p A 1) ]) )","E1409");
    // happy path compile-only check
    ok("(module (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) ]) (fn :name \"main\" :ret void :params [ (param i32 %x) (param i64 %y) ] :body [ (sum-new %s T A [ %x %y ]) (sum-get %g0 T %s A 0) (sum-get %g1 T %s A 1) ]) )");
    std::cout << "[phase4] match helper checks...\n";
    // bad shapes
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match X %p :cases [ ] :default [ ]) ]) )","E1400");
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T x :cases [ ] :default [ ]) ]) )","E1410");
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :cases x :default [ ]) ]) )","E1412");
    // case unknown variant
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :cases [ (case B [ ]) ] :default [ ]) ]) )","E1405");
    // duplicate variant in cases
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) (variant :name B :fields [ i64 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :cases [ (case A [ ]) (case A [ ]) ] :default [ ]) ]) )","E1414");
    // missing :cases entirely
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :default [ ]) ]) )","E1411");
    // missing :default
    // Non-exhaustive (two variants but only one case) without :default should error E1415
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) (variant :name B :fields [ i64 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :cases [ (case A [ ]) ]) ]) )","E1415");
    // default not a vector
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :cases [ (case A [ ]) ] :default x) ]) )","E1416");
    // value type mismatch: param is pointer to U, match expects T
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (sum :name U :variants [ (variant :name C :fields [ i64 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref U)) %p) ] :body [ (match T %p :cases [ (case A [ ]) ] :default [ ]) ]) )","E1410");
    // good shape
    ok("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) (variant :name B :fields [ i64 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :cases [ (case A [ ]) (case B [ ]) ] :default [ ]) ]) )");
    // exhaustive without default is allowed
    ok("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) (variant :name B :fields [ i64 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :cases [ (case A [ ]) (case B [ ]) ]) ]) )");
    // good with non-empty bodies
    ok("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) (variant :name B :fields [ i64 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :cases [ (case A [ (const %za i32 0) ]) (case B [ (const %zb i64 42) ]) ] :default [ (const %zd i32 1) ]) ]) )");
    // binds shape errors
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :cases [ (case A :binds x :body [ ]) ] :default [ ]) ]) )","E1417");
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :cases [ (case A :binds [ (bind x 0) ] :body [ ]) ] :default [ ]) ]) )","E1420");
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :cases [ (case A :binds [ (bind %x -1) ] :body [ ]) ] :default [ ]) ]) )","E1418");
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :cases [ (case A :binds [ (bind %x 1) ] :body [ ]) ] :default [ ]) ]) )","E1419");
    // binds positive: extract fields and use them
    ok("(module (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) (variant :name B :fields [ i32 ]) ]) (fn :name \"main\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match T %p :cases [ (case A :binds [ (bind %x 0) (bind %y 1) ] :body [ (add %sx i32 %x %x) (add %sy i64 %y %y) ]) (case B :body [ (const %one i32 1) ]) ] :default [ ]) ]) )");
    std::cout << "[phase4] match result-as-value negative/positive...\n";
    // Missing :value in a case when in result mode
    std::cout << "  -> E1421 (case missing :value)\n";
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) (variant :name B :fields [ i32 ]) ]) (fn :name \"m\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match %r i32 T %p :cases [ (case A :body [ (const %x i32 1) ]) (case B :body [ (const %y i32 2) :value %y ]) ] :default (default :body [ (const %z i32 3) :value %z ]) ) ]) )","E1421");
    // Default missing :value
    std::cout << "  -> E1422 (default missing :value)\n";
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"m\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match %r i32 T %p :cases [ (case A :body [ (const %x i32 1) :value %x ]) ] :default [ (const %z i32 3) ] ) ]) )","E1422");
    // Type mismatch between case value and declared result type
    std::cout << "  -> E1423 (result type mismatch)\n";
    err("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) (variant :name B :fields [ i32 ]) ]) (fn :name \"m\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match %r i64 T %p :cases [ (case A :body [ (const %x i32 1) :value %x ]) (case B :body [ (const %y i32 2) :value %y ]) ] :default (default :body [ (const %z i32 3) :value %z ]) ) ]) )","E1423");
    // Positive: produce i32 result and assign to %r
    std::cout << "  -> OK (result mode)\n";
    std::cout << "  -> [dbg] entering positive result-mode ok()\n" << std::flush;
    ok("(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) (variant :name B :fields [ i32 ]) ]) (fn :name \"m\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match %r i32 T %p :cases [ (case A :body [ (const %x i32 1) :value %x ]) (case B :body [ (const %y i32 2) :value %y ]) ] :default (default :body [ (const %z i32 3) :value %z ]) ) ]) )");
    std::cout << "  -> [dbg] positive result-mode ok() returned\n" << std::flush;
    std::cout << "Phase 4 sum tests passed\n";
}

// TEMP: focused repro for first result-mode negative case to observe diagnostics
void run_phase4_sum_resultmode_repro(){
    using namespace edn;
    const char* prog = "(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) (variant :name B :fields [ i32 ]) ]) (fn :name \"m\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match %r i32 T %p :cases [ (case A :body [ (const %x i32 1) ]) (case B :body [ (const %y i32 2) :value %y ]) ] :default (default :body [ (const %z i32 3) :value %z ]) ) ]) )";
    std::cout << "[phase4] repro: starting...\n";
    try {
        auto ast = parse(prog);
        TypeContext ctx; TypeChecker tc(ctx);
        auto res = tc.check_module(ast);
        std::cout << "[phase4] repro: success=" << (res.success?"true":"false") << "\n";
        for(auto &e: res.errors){ std::cout << "  error: " << e.code << " - " << e.message << "\n"; }
    } catch(const std::exception& ex) {
        std::cout << "[phase4] repro: parse threw: " << ex.what() << "\n";
    }
    // Now try the E1422 string to see if parsing fails
    const char* prog2 = "(module (sum :name T :variants [ (variant :name A :fields [ i32 ]) ]) (fn :name \"m\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [ (match %r i32 T %p :cases [ (case A :body [ (const %x i32 1) :value %x ]) ] :default [ (const %z i32 3) ] ) ]) )";
    std::cout << "[phase4] repro E1422 parse test...\n";
    try {
        auto ast2 = parse(prog2);
        std::cout << "[phase4] repro E1422 parsed OK\n";
    } catch(const std::exception& ex) {
        std::cout << "[phase4] repro E1422 parse threw: " << ex.what() << "\n";
    }
}
