#include <cassert>
#include <iostream>
#include <string>
#include <cstdlib>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Error.h>

using namespace edn;

void run_phase4_coro_lowering_smoke_test(){
    std::cout << "[phase4] coroutines lowering smoke test...\n";
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_PASSES", "0");
    _putenv_s("EDN_ENABLE_CORO", "1");
#else
    setenv("EDN_ENABLE_PASSES", "0", 1);
    setenv("EDN_ENABLE_CORO", "1", 1);
#endif

    const char* SRC = R"EDN((module
      (fn :name "co" :ret i32 :params [ ] :body [
         (coro-begin %h)
         (coro-id %cid)
         (coro-save %tok %h)
         (coro-promise %p %h)
         (coro-final-suspend %st %tok)
         (coro-end %h)
         (const %z i32 0)
         (ret i32 %z)
      ])
    ))EDN";

    auto ast = parse(SRC);
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
    assert(tcres.success && mod);

    // Sanity: function should carry the presplitcoroutine attribute
    if (auto *F = mod->getFunction("co")) {
        bool hasAttr = F->hasFnAttribute("presplitcoroutine");
        if(!hasAttr){
            std::string ir; llvm::raw_string_ostream os(ir); mod->print(os, nullptr); os.flush();
            std::cerr << "[coro] missing presplitcoroutine attribute on co()\n" << ir << std::endl;
        }
        assert(hasAttr && "co() must be marked presplitcoroutine");
    }

    // Run the minimal coroutine lowering pipeline: early -> split -> cleanup
    llvm::PassBuilder PB;
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    llvm::ModulePassManager MPM;
    if (auto Err = PB.parsePassPipeline(MPM, "coro-early,coro-split,coro-cleanup")) {
        std::string emsg = llvm::toString(std::move(Err));
        std::cerr << "Failed to parse coroutine pipeline: " << emsg << "\n";
        assert(false && "failed to parse coroutine lowering pipeline");
    }
    MPM.run(*mod, MAM);

    // Verify module after lowering
    std::string verr;
    llvm::raw_string_ostream os(verr);
    bool bad = llvm::verifyModule(*mod, &os);
    if(bad){ std::cerr << "[coro] verifier errors after lowering:\n" << os.str() << std::endl; }
    assert(!bad && "module should verify after coroutine lowering passes");
    std::cout << "[phase4] coroutines lowering smoke test passed\n";
}
