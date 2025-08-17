#include <cassert>
#include <iostream>
#include <string>
#include <cstdlib>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/type_check.hpp"

using namespace edn;

// Ensures panic=unwind in a try body routes to landingpad catch-all on Itanium
void run_phase4_eh_panic_inside_try_itanium_test(){
    std::cout << "[phase4] EH panic inside try (Itanium) test...\n";
    _putenv("EDN_EH_MODEL=itanium");
    _putenv("EDN_ENABLE_EH=1");
    _putenv("EDN_PANIC=unwind");
    _putenv("EDN_TARGET_TRIPLE=x86_64-apple-darwin");

    const char* SRC = R"((module
      (fn :name "main" :ret i32 :params [] :body [
         (try :body [ (panic) ] :catch [ (const %h i32 42) ])
         (const %z i32 0)
         (ret i32 %z)
      ])
    ))";

    auto ast = parse(SRC);
    TypeContext tctx; TypeCheckResult tcres; IREmitter emitter(tctx);
    auto *mod = emitter.emit(ast, tcres);
    assert(tcres.success && mod);

    std::string ir; { std::string s; llvm::raw_string_ostream os(s); mod->print(os, nullptr); os.flush(); ir = std::move(s);}    
    // Itanium personality, invoke in try, landingpad with catch clause, and __cxa_throw present
    assert(ir.find("__gxx_personality_v0") != std::string::npos);
    assert(ir.find("invoke ") != std::string::npos);
    assert(ir.find("landingpad ") != std::string::npos);
    assert(ir.find("catch ") != std::string::npos);
    assert(ir.find("__cxa_throw") != std::string::npos);

    _putenv("EDN_EH_MODEL=");
    _putenv("EDN_ENABLE_EH=");
    _putenv("EDN_PANIC=");
    _putenv("EDN_TARGET_TRIPLE=");
    std::cout << "[phase4] EH panic inside try (Itanium) test passed\n";
}
