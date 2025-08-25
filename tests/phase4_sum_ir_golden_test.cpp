#include <cassert>
#include <iostream>
#include <string>
#include <cstdlib>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/Support/raw_ostream.h>

using namespace edn;

static std::string module_to_ir(llvm::Module* m){
    std::string buf; llvm::raw_string_ostream os(buf); m->print(os, nullptr); os.flush(); return buf;
}

static void expect_contains(const std::string& ir, const char* needle){
    if(ir.find(needle)==std::string::npos){
        std::cerr << "Golden IR missing snippet:\n" << needle << "\n----- IR dump -----\n" << ir << "\n-------------------\n";
    }
    assert(ir.find(needle)!=std::string::npos);
}

void run_phase4_sum_ir_golden_tests(){
    // Ensure optimization pipeline is disabled so we see raw emission shapes
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_PASSES", "0");
#else
    setenv("EDN_ENABLE_PASSES", "0", 1);
#endif

    std::cout << "[phase4] golden IR: sum-new/sum-get combined...\n";
    {
        TypeContext tctx; IREmitter em(tctx); TypeCheckResult r;
        // Build a module that constructs a sum, extracts payloads, and returns a value to keep code live
        auto ast = parse(
            "(module :id \"m_sn\""
            " (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) ])"
            " (fn :name \"f\" :ret i64 :params [ (param i32 %x) (param i64 %y) ] :body ["
            "   (sum-new %s T A [ %x %y ])"
            "   (sum-get %g0 T %s A 0)"
            "   (sum-get %g1 T %s A 1)"
            "   (zext %x64 i64 %g0)"
            "   (add %sum i64 %x64 %g1)"
            "   (ret i64 %sum)"
            " ]) )");
        auto *m = em.emit(ast, r); assert(r.success && m);
        auto ir = module_to_ir(m);
        // Type layout for T: { i32 tag, [12 x i8] payload }
        expect_contains(ir, "struct.T = type { i32, [12 x i8] }");
        // Function signature
        expect_contains(ir, "define i64 @f(i32 %x, i64 %y)");
        // Tag store sequence
        expect_contains(ir, "s.tag.addr = getelementptr inbounds %struct.T, ptr %s, i32 0, i32 0");
        expect_contains(ir, "store i32 0, ptr %s.tag.addr");
        // Payload address and raw pointer
        expect_contains(ir, "s.payload.addr = getelementptr inbounds %struct.T, ptr %s, i32 0, i32 1");
    // Note: with opaque pointers we no longer emit an explicit bitcast to a raw ptr
        // Field 0 at offset 0 (i32)
    expect_contains(ir, "s.payload.f0.raw = getelementptr inbounds i8, ptr %s.payload.addr, i64 0");
    expect_contains(ir, "store i32 %x, ptr %s.payload.f0.raw");
        // Field 1 at offset 4 (i64)
    expect_contains(ir, "s.payload.f1.raw = getelementptr inbounds i8, ptr %s.payload.addr, i64 4");
    expect_contains(ir, "store i64 %y, ptr %s.payload.f1.raw");
        // Extraction loads
        expect_contains(ir, "g0.payload.addr = getelementptr inbounds %struct.T, ptr %s, i32 0, i32 1");
    // No explicit bitcast; compute field addresses from payload.addr
    expect_contains(ir, "g0.payload.f0.raw = getelementptr inbounds i8, ptr %g0.payload.addr, i64 0");
    expect_contains(ir, "g0 = load i32, ptr %g0.payload.f0.raw");
    expect_contains(ir, "g1.payload.addr = getelementptr inbounds %struct.T, ptr %s, i32 0, i32 1");
    expect_contains(ir, "g1.payload.f1.raw = getelementptr inbounds i8, ptr %g1.payload.addr, i64 4");
    expect_contains(ir, "g1 = load i64, ptr %g1.payload.f1.raw");
    }

    std::cout << "[phase4] golden IR: sum-is...\n";
    {
        TypeContext tctx; IREmitter em(tctx); TypeCheckResult r;
        auto ast = parse(
            "(module :id \"m_si\""
            " (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) (variant :name B :fields [ i32 ]) ])"
            " (fn :name \"isA\" :ret i1 :params [ (param (ptr (struct-ref T)) %p) ] :body ["
            "   (sum-is %ok T %p A)"
            "   (ret i1 %ok)"
            " ]) )");
        auto *m = em.emit(ast, r); assert(r.success && m);
        auto ir = module_to_ir(m);
        expect_contains(ir, "define i1 @isA(ptr %p)");
        expect_contains(ir, "ok.tag.addr = getelementptr inbounds %struct.T, ptr %p, i32 0, i32 0");
        expect_contains(ir, "ok.tag = load i32, ptr %ok.tag.addr");
        expect_contains(ir, "ok = icmp eq i32 %ok.tag, 0");
    }

    std::cout << "[phase4] golden IR: match with binds...\n";
    {
        TypeContext tctx; IREmitter em(tctx); TypeCheckResult r;
        auto ast = parse(
            "(module :id \"m_match\""
            " (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) (variant :name B :fields [ i32 ]) ])"
            " (fn :name \"m\" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body ["
            "   (match T %p :cases ["
            "     (case A :binds [ (bind %x 0) (bind %y 1) ] :body [ (zext %x64 i64 %x) (add %z i64 %x64 %y) ])"
            "     (case B [ (const %one i32 1) ])"
            "   ] :default [ ])"
            " ]) )");
        auto *m = em.emit(ast, r); assert(r.success && m);
        auto ir = module_to_ir(m);
        // Tag load and compares for both variants
        expect_contains(ir, "match.tag.addr = getelementptr inbounds %struct.T, ptr %p, i32 0, i32 0");
        expect_contains(ir, "match.tag = load i32, ptr %match.tag.addr");
        expect_contains(ir, "match.cmp = icmp eq i32 %match.tag, 0");
        expect_contains(ir, "icmp eq i32 %match.tag, 1");
    // Payload extraction for binds (current naming uses x.raw / y.raw rather than x.payload.f0.raw)
    expect_contains(ir, "match.payload.addr = getelementptr inbounds %struct.T, ptr %p, i32 0, i32 1");
    expect_contains(ir, "x.raw = getelementptr inbounds i8, ptr %match.payload.addr, i64 0");
    expect_contains(ir, "x = load i32, ptr %x.raw");
    expect_contains(ir, "y.raw = getelementptr inbounds i8, ptr %match.payload.addr, i64 4");
    expect_contains(ir, "y = load i64, ptr %y.raw");
    }

    std::cout << "[phase4] golden IR: match result-as-value PHI...\n";
    {
        TypeContext tctx; IREmitter em(tctx); TypeCheckResult r;
        auto ast = parse(
            "(module :id \"m_mphi\""
            " (sum :name T :variants [ (variant :name A :fields [ i32 ]) (variant :name B :fields [ i32 ]) ])"
            " (fn :name \"f\" :ret i32 :params [ (param (ptr (struct-ref T)) %p) ] :body ["
            "   (match %r i32 T %p :cases ["
            "     (case A :body [ (const %x i32 1) :value %x ])"
            "     (case B :body [ (const %y i32 2) :value %y ])"
            "   ] :default (default :body [ (const %z i32 3) :value %z ]) )"
            "   (ret i32 %r)"
            " ]) )");
        auto *m = em.emit(ast, r); assert(r.success && m);
        auto ir = module_to_ir(m);
    // Check that a PHI for result mode exists; incoming names may vary
    expect_contains(ir, "%r = phi i32");
    }

    std::cout << "Phase 4 golden IR tests passed\n";
}
