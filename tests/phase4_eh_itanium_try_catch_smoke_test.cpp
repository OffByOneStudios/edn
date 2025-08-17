#include <cassert>
#include <iostream>
#include <string>
#include <cstdlib>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/type_check.hpp"
// Optional verifier to catch malformed IR early during debugging
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

using namespace edn;

// Verifies Itanium try/catch lowering with landingpad (catch-all)
void run_phase4_eh_itanium_try_catch_smoke_test(){
    std::cout << "[phase4] EH Itanium try/catch smoke test...\n";
    _putenv("EDN_EH_MODEL=itanium");
    _putenv("EDN_ENABLE_EH=1");
    _putenv("EDN_TARGET_TRIPLE=x86_64-apple-darwin");

    const char* SRC = R"((module
      (fn :name "may_throw" :ret i32 :params [] :external true)
      (fn :name "main" :ret i32 :params [] :body [
         (try :body [
                (call %x i32 may_throw)
              ]
              :catch [
                (const %h i32 1)
              ])
         (const %z i32 0)
         (ret i32 %z)
      ])
    ))";

    auto ast = parse(SRC);
    TypeContext tctx; TypeCheckResult tcres; IREmitter emitter(tctx);
  auto *mod = emitter.emit(ast, tcres);
  std::cout << "[dbg] emitted module ptr=" << (void*)mod << " success=" << tcres.success << "\n";
    assert(tcres.success && mod);
  // Verify module well-formedness (prints to llvm errs on failure)
  bool bad = llvm::verifyModule(*mod, &llvm::errs());
  std::cout << "[dbg] verifyModule bad=" << bad << "\n";

    std::string ir; {
        std::string s; llvm::raw_string_ostream os(s); mod->print(os, nullptr); os.flush(); ir = std::move(s);
    }
  std::cout << "[dbg] got IR text, size=" << ir.size() << "\n";
    // Personality and Itanium catch constructs
    assert(ir.find("__gxx_personality_v0") != std::string::npos);
    assert(ir.find("invoke ") != std::string::npos);
    assert(ir.find("landingpad ") != std::string::npos);
    assert(ir.find("catch ") != std::string::npos);

    _putenv("EDN_EH_MODEL=");
    _putenv("EDN_ENABLE_EH=");
    _putenv("EDN_TARGET_TRIPLE=");
    std::cout << "[phase4] EH Itanium try/catch smoke test passed\n";
}
