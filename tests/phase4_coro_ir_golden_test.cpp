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

void run_phase4_coro_ir_golden_test(){
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_PASSES", "0");
    _putenv_s("EDN_ENABLE_CORO", "1");
#else
    setenv("EDN_ENABLE_PASSES", "0", 1);
    setenv("EDN_ENABLE_CORO", "1", 1);
#endif

    std::cout << "[phase4] golden IR: minimal coroutines...\n";
    {
        TypeContext tctx; IREmitter em(tctx); TypeCheckResult r;
        // Simple function that begins, suspends, and ends a coroutine
        auto ast = parse(
            "(module :id \"m_coro\""
            " (fn :name \"c\" :ret i32 :params [ ] :body ["
            "   (coro-begin %h)"
            "   (coro-suspend %st %h)"
            "   (coro-end %h)"
            "   (const %z i32 0)"
            "   (ret i32 %z)"
            " ]) )");
        auto *m = em.emit(ast, r);
        if(!r.success || !m){
            std::cerr << "[coro] type check failed: errors=" << r.errors.size() << "\n";
            for(const auto& e : r.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        }
        assert(r.success && m);
        auto ir = module_to_ir(m);
    // Check presence of key coro intrinsics when flag is on
    expect_contains(ir, "call token @llvm.coro.id(i32 0, ptr null, ptr null, ptr null)");
    expect_contains(ir, "call ptr @llvm.coro.begin(token %coro.id, ptr null)");
    expect_contains(ir, "call i8 @llvm.coro.suspend(token none, i1 false)");
    expect_contains(ir, "call i1 @llvm.coro.end(ptr %h, i1 false, token none)");
    }

    std::cout << "[phase4] golden IR: coroutines save and final-suspend...\n";
    {
        TypeContext tctx; IREmitter em(tctx); TypeCheckResult r;
        auto ast = parse(
            "(module :id \"m_coro2\""
            " (fn :name \"c2\" :ret i32 :params [ ] :body ["
            "   (coro-begin %h)"
            "   (coro-id %cid)"
            "   (coro-save %tok %h)"
            "   (coro-promise %p %h)"
            "   (coro-size %sz)"
            "   (coro-alloc %need %cid)"
            "   (coro-final-suspend %st %tok)"
            "   (coro-resume %h)"
            "   (coro-destroy %h)"
            "   (coro-free %mem %cid %h)"
            "   (coro-done %dn %h)"
            "   (coro-end %h)"
            "   (const %z i32 0)"
            "   (ret i32 %z)"
            " ]) )");
        auto *m = em.emit(ast, r);
        if(!r.success || !m){
            std::cerr << "[coro] type check failed: errors=" << r.errors.size() << "\n";
            for(const auto& e : r.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        }
        assert(r.success && m);
        auto ir = module_to_ir(m);
        expect_contains(ir, "call token @llvm.coro.id(i32 0, ptr null, ptr null, ptr null)");
        expect_contains(ir, "call ptr @llvm.coro.begin(token %coro.id, ptr null)");
        expect_contains(ir, "call token @llvm.coro.save(ptr %h)");
    // promise(ptr, i32, i1) returns ptr
    expect_contains(ir, "call ptr @llvm.coro.promise(ptr %h");
    expect_contains(ir, "call i64 @llvm.coro.size.i64()");
    expect_contains(ir, "call i1 @llvm.coro.alloc(token %coro.id)");
        expect_contains(ir, "call i8 @llvm.coro.suspend(token %tok, i1 true)");
    expect_contains(ir, "call void @llvm.coro.resume(ptr %h)");
    expect_contains(ir, "call void @llvm.coro.destroy(ptr %h)");
    expect_contains(ir, "call ptr @llvm.coro.free(token %coro.id, ptr %h)");
    expect_contains(ir, "call i1 @llvm.coro.done(ptr %h)");
        expect_contains(ir, "call i1 @llvm.coro.end(ptr %h, i1 false, token none)");
    }
}
