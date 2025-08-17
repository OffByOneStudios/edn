// Verifies SEH try/catch lowering with catchswitch/catchpad/catchret
#include <cassert>
#include <iostream>
#include <string>
#include <cstdlib>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/type_check.hpp"

using namespace edn;

void run_phase4_eh_seh_try_catch_smoke_test(){
    std::cout << "[phase4] EH SEH try/catch smoke test...\n";
    _putenv("EDN_EH_MODEL=seh");
    _putenv("EDN_ENABLE_EH=1");
    _putenv("EDN_TARGET_TRIPLE=x86_64-pc-windows-msvc");

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
    assert(tcres.success && mod);

    std::string ir; {
        std::string s; llvm::raw_string_ostream os(s); mod->print(os, nullptr); os.flush(); ir = std::move(s);
    }
    // Personality and SEH catch constructs
    assert(ir.find("__C_specific_handler") != std::string::npos);
    assert(ir.find("catchswitch") != std::string::npos);
    assert(ir.find("catchpad") != std::string::npos);
    assert(ir.find("catchret") != std::string::npos);
    // The call in try body must be emitted as invoke
    assert(ir.find("invoke ") != std::string::npos);

    _putenv("EDN_EH_MODEL=");
    _putenv("EDN_ENABLE_EH=");
    _putenv("EDN_TARGET_TRIPLE=");
    std::cout << "[phase4] EH SEH try/catch smoke test passed\n";
}
