#include <cassert>
#include <iostream>
#include <string>
#include <cstdlib>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

// Ensure SEH cleanup funclet is created once and reused by multiple invokes.
void run_phase4_eh_seh_cleanup_consolidation_test(){
    std::cout << "[phase4] EH SEH cleanup consolidation test...\n";
    const char* SRC = R"((module
      (fn :name "f" :ret i32 :params [] :body [
        (const %a i32 1)
        (const %b i32 2)
        (call %x i32 callee)
        (call %y i32 callee)
        (add %z i32 %a %b)
        (ret i32 %z)
      ])
      (fn :name "callee" :ret i32 :params [] :body [ (const %z i32 1) (ret i32 %z) ])
    ))";
    auto ast = parse(SRC);
    _putenv("EDN_EH_MODEL=seh");
    _putenv("EDN_ENABLE_EH=1");
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
    assert(tcres.success && mod);
    std::string ir; { std::string s; llvm::raw_string_ostream rso(s); mod->print(rso, nullptr); rso.flush(); ir = std::move(s);}    
    // Expect exactly one cleanuppad occurrence and two invokes
    size_t cp1 = ir.find("cleanuppad ");
    assert(cp1 != std::string::npos);
    size_t cp2 = ir.find("cleanuppad ", cp1+1);
    assert(cp2 == std::string::npos);
    size_t inv1 = ir.find("invoke ");
    assert(inv1 != std::string::npos);
    size_t inv2 = ir.find("invoke ", inv1+1);
    assert(inv2 != std::string::npos);
    _putenv("EDN_EH_MODEL=");
    _putenv("EDN_ENABLE_EH=");
    std::cout << "[phase4] EH SEH cleanup consolidation test passed\n";
}
