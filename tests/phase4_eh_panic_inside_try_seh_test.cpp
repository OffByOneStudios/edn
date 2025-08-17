#include <cassert>
#include <iostream>
#include <string>
#include <cstdlib>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/type_check.hpp"

using namespace edn;

// Ensures panic=unwind in a try body routes to SEH catch funclet
void run_phase4_eh_panic_inside_try_seh_test(){
    std::cout << "[phase4] EH panic inside try (SEH) test...\n";
    _putenv("EDN_EH_MODEL=seh");
    _putenv("EDN_ENABLE_EH=1");
    _putenv("EDN_PANIC=unwind");
    _putenv("EDN_TARGET_TRIPLE=x86_64-pc-windows-msvc");

    const char* SRC = R"((module
      (fn :name "main" :ret i32 :params [] :body [
         (try :body [ (panic) ] :catch [ (const %h i32 7) ])
         (const %z i32 0)
         (ret i32 %z)
      ])
    ))";

    auto ast = parse(SRC);
    TypeContext tctx; TypeCheckResult tcres; IREmitter emitter(tctx);
    auto *mod = emitter.emit(ast, tcres);
    assert(tcres.success && mod);

    std::string ir; { std::string s; llvm::raw_string_ostream os(s); mod->print(os, nullptr); os.flush(); ir = std::move(s);}    
    // SEH personality, catchswitch/pad/ret present, invoke, and RaiseException present
    assert(ir.find("__C_specific_handler") != std::string::npos);
    assert(ir.find("catchswitch") != std::string::npos);
    assert(ir.find("catchpad") != std::string::npos);
    assert(ir.find("catchret") != std::string::npos);
    assert(ir.find("invoke ") != std::string::npos);
    assert(ir.find("RaiseException") != std::string::npos);

    _putenv("EDN_EH_MODEL=");
    _putenv("EDN_ENABLE_EH=");
    _putenv("EDN_PANIC=");
    _putenv("EDN_TARGET_TRIPLE=");
    std::cout << "[phase4] EH panic inside try (SEH) test passed\n";
}
