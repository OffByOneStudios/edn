#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/type_check.hpp"

using namespace edn;

// Verifies that EDN_PANIC=unwind lowers (panic) to a call to __cxa_throw with Itanium EH enabled.
void run_phase4_eh_panic_unwind_test(){
    std::cout << "[phase4] EH panic=unwind lowering test...\n";
    // Configure Itanium EH and panic=unwind; use a Darwin triple to be explicit cross-target.
    _putenv("EDN_EH_MODEL=itanium");
    _putenv("EDN_ENABLE_EH=1");
    _putenv("EDN_PANIC=unwind");
    _putenv("EDN_TARGET_TRIPLE=x86_64-apple-darwin");

    auto ast = parse("(module :header {:kw 1} (fn :name \"boom\" :ret Void :params [] :body [ (panic) ]))");
    TypeContext tctx; TypeCheckResult tcres; IREmitter E(tctx);
    auto *M = E.emit(ast, tcres);
    assert(M && tcres.success);

    // Dump IR to a string and look for __cxa_throw and unreachable.
    std::string ir; {
        std::string s; llvm::raw_string_ostream rso(s); M->print(rso, nullptr); rso.flush(); ir = std::move(s);
    }
    // Basic shape assertions
    assert(ir.find("__gxx_personality_v0") != std::string::npos); // personality attached
    // Accept either direct call or invoke depending on context
    bool hasThrow = (ir.find("call void @__cxa_throw") != std::string::npos) ||
                    (ir.find("invoke void @__cxa_throw") != std::string::npos);
    assert(hasThrow);
    assert(ir.find("unreachable") != std::string::npos);

    // Reset env to avoid affecting later tests
    _putenv("EDN_EH_MODEL=");
    _putenv("EDN_ENABLE_EH=");
    _putenv("EDN_PANIC=");
    _putenv("EDN_TARGET_TRIPLE=");
    std::cout << "[phase4] EH panic=unwind lowering test passed\n";
}
