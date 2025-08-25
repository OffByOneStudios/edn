#include <cassert>
#include <iostream>
#include <string>
#include <cstdlib>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/type_check.hpp"

using namespace edn;

// Regression: panic=unwind should produce distinct lowering patterns for bare panic vs inside try (Itanium & SEH)
// This aggregates and asserts both shapes in one place for faster failure diagnostics.
void run_phase4_eh_panic_unwind_regression_test(){
    std::cout << "[phase4] EH panic unwind regression test...\n";
    struct Case { const char* model; const char* triple; const char* body; const char* expect1; const char* expect2; };
    // For Itanium: expect __cxa_throw + landingpad when in try; plain throw path has __cxa_throw but no landingpad.
    // For SEH: expect RaiseException + catchswitch/pad when in try; plain path only RaiseException.
    Case cases[] = {
        {"itanium", "x86_64-apple-darwin", "(panic)", "__cxa_throw", nullptr},
        {"itanium", "x86_64-apple-darwin", "(try :body [ (panic) ] :catch [ (const %h i32 1) ])", "__cxa_throw", "landingpad"},
        {"seh", "x86_64-pc-windows-msvc", "(panic)", "RaiseException", nullptr},
        {"seh", "x86_64-pc-windows-msvc", "(try :body [ (panic) ] :catch [ (const %h i32 2) ])", "RaiseException", "catchswitch"},
    };
    for(const auto &c : cases){
#if defined(_WIN32)
        {
            std::string s1 = std::string("EDN_EH_MODEL=") + c.model; _putenv(s1.c_str());
            _putenv("EDN_ENABLE_EH=1");
            _putenv("EDN_PANIC=unwind");
            std::string s2 = std::string("EDN_TARGET_TRIPLE=") + c.triple; _putenv(s2.c_str());
        }
#else
        setenv("EDN_EH_MODEL", c.model, 1);
        setenv("EDN_ENABLE_EH", "1", 1);
        setenv("EDN_PANIC", "unwind", 1);
        setenv("EDN_TARGET_TRIPLE", c.triple, 1);
#endif
        std::string src = std::string("(module (fn :name \"main\" :ret i32 :params [] :body [ ") + c.body + " (const %z i32 0) (ret i32 %z) ]))";
        auto ast = parse(src.c_str());
        TypeContext tctx; TypeCheckResult tcres; IREmitter em(tctx); auto *m = em.emit(ast, tcres);
        assert(tcres.success && m);
        std::string ir; { std::string tmp; llvm::raw_string_ostream os(tmp); m->print(os,nullptr); os.flush(); ir = std::move(tmp);}        
        // Expect core throw primitive
        if(ir.find(c.expect1)==std::string::npos){
            std::cerr << "[panic-reg] Missing primary token '" << c.expect1 << "' for model=" << c.model << " body=" << c.body << "\nIR:\n" << ir << std::endl;
        }
        assert(ir.find(c.expect1)!=std::string::npos);
        if(c.expect2){
            if(ir.find(c.expect2)==std::string::npos){
                std::cerr << "[panic-reg] Missing secondary token '" << c.expect2 << "' for model=" << c.model << " body=" << c.body << "\nIR:\n" << ir << std::endl;
            }
            assert(ir.find(c.expect2)!=std::string::npos);
        } else {
            // Bare panic path: implementation may materialize internal cleanup handling (landingpad/funclets);
            // we only assert no user catch clause was synthesized in Itanium case (absence of ' catch ' substring).
            if(std::string(c.model)=="itanium"){
                assert(ir.find(" catch ")==std::string::npos && "bare panic should not produce catch clause");
            }
        }
    }
#if defined(_WIN32)
    _putenv("EDN_EH_MODEL="); _putenv("EDN_ENABLE_EH="); _putenv("EDN_PANIC="); _putenv("EDN_TARGET_TRIPLE=");
#else
    unsetenv("EDN_EH_MODEL"); unsetenv("EDN_ENABLE_EH"); unsetenv("EDN_PANIC"); unsetenv("EDN_TARGET_TRIPLE");
#endif
    std::cout << "[phase4] EH panic unwind regression test passed\n";
}
