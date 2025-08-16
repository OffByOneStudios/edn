// Linter warnings tests (M4.10)
#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"

using namespace edn;

static bool hasWarning(const TypeCheckResult& r, const std::string& code){
    for(const auto& w : r.warnings) if(w.code==code) return true; return false;
}

void run_phase4_lints_tests(){
    // Ensure lints run (unless user has explicitly disabled via EDN_LINT=0)
    if(const char* lintEnv = std::getenv("EDN_LINT")){ if(lintEnv[0]=='0'){ std::cout << "Lints disabled via EDN_LINT=0, skipping lint tests\n"; return; } }
    TypeContext ctx; TypeChecker tc(ctx);
    // W1400: unreachable after top-level ret
    auto m1 = parse("(module (fn :name \"f\" :ret i32 :params [] :body [ (const %z i32 0) (ret i32 %z) (const %dead i32 1) ]) )");
    auto r1 = tc.check_module(m1); assert(r1.success); assert(hasWarning(r1, "W1400"));

    // W1402: nested unreachable inside block after ret
    auto m2 = parse("(module (fn :name \"g\" :ret i32 :params [] :body [ (block :body [ (const %a i32 1) (ret i32 %a) (const %b i32 2) ]) (const %z i32 0) (ret i32 %z) ]) )");
    auto r2 = tc.check_module(m2); assert(r2.success); assert(hasWarning(r2, "W1402"));

    // W1404: unused parameter; W1403: unused local/def
    auto m3 = parse("(module (fn :name \"h\" :ret i32 :params [ (param i32 %p) ] :body [ (const %z i32 0) (const %u i32 42) (ret i32 %z) ]) )");
    auto r3 = tc.check_module(m3); assert(r3.success); assert(hasWarning(r3, "W1404")); assert(hasWarning(r3, "W1403"));

    std::cout << "Phase 4 lints tests passed\n";
}
