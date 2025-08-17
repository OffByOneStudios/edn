#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/type_check.hpp"

using namespace edn;

// Verifies that EDN_PANIC=unwind lowers (panic) to a call to RaiseException under SEH when EH is enabled.
void run_phase4_eh_panic_unwind_seh_test(){
    std::cout << "[phase4] EH panic=unwind lowering test (SEH)...\n";
    _putenv("EDN_EH_MODEL=seh");
    _putenv("EDN_ENABLE_EH=1");
    _putenv("EDN_PANIC=unwind");
    _putenv("EDN_TARGET_TRIPLE=x86_64-pc-windows-msvc");

    auto ast = parse("(module :header {:kw 1} (fn :name \"boom\" :ret Void :params [] :body [ (panic) ]))");
    TypeContext tctx; TypeCheckResult tcres; IREmitter E(tctx);
    auto *M = E.emit(ast, tcres);
    assert(M && tcres.success);

    std::string ir; {
        std::string s; llvm::raw_string_ostream rso(s); M->print(rso, nullptr); rso.flush(); ir = std::move(s);
    }
    // Shape assertions
    assert(ir.find("__C_specific_handler") != std::string::npos);
    {
        bool hasRaise = (ir.find("call void @RaiseException") != std::string::npos) ||
                        (ir.find("invoke void @RaiseException") != std::string::npos);
        assert(hasRaise);
    }
    assert(ir.find("unreachable") != std::string::npos);

    _putenv("EDN_EH_MODEL=");
    _putenv("EDN_ENABLE_EH=");
    _putenv("EDN_PANIC=");
    _putenv("EDN_TARGET_TRIPLE=");
    std::cout << "[phase4] EH panic=unwind lowering test (SEH) passed\n";
}
