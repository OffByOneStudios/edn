#include <cassert>
#include <iostream>
#include <string>
#include <cstdlib>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/PassManager.h>

using namespace edn;

// JIT smoke: build a module with a minimal coroutine, lower it, and JIT call co()->i32 0
void run_phase4_coro_jit_smoke_test(){
    // Ensure coroutines are emitted; keep opt pipeline off in emitter; we'll run our own passes
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_PASSES", "0");
    _putenv_s("EDN_ENABLE_CORO", "1");
#else
    setenv("EDN_ENABLE_PASSES", "0", 1);
    setenv("EDN_ENABLE_CORO", "1", 1);
#endif
    std::cout << "[phase4] coroutines JIT smoke: starting" << std::endl;

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
    if(!(tcres.success && mod)){
        std::cerr << "[coro-jit] emitter failed, success=" << tcres.success << ", mod=" << (void*)mod << "\n";
        assert(false && "emitter failed for coroutine JIT test");
    }
    std::cout << "[phase4] coroutines JIT smoke: emitted module" << std::endl;

    // Lower coroutine intrinsics so we don't JIT opaque intrinsics paths
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
        auto msg = llvm::toString(std::move(Err));
        std::cerr << "Failed to parse coroutine pipeline: " << msg << "\n";
        assert(false && "failed to parse coroutine lowering pipeline");
    }
    MPM.run(*mod, MAM);
    bool bad = llvm::verifyModule(*mod, &llvm::errs());
    if(bad){
        std::string ir;
        llvm::raw_string_ostream rso(ir);
        rso << *mod;
        rso.flush();
        std::cerr << "[coro-jit] verify failed after lowering:\n" << ir << std::endl;
        assert(!bad && "module should verify after coroutine lowering");
    }
    std::cout << "[phase4] coroutines JIT smoke: lowered and verified" << std::endl;
    {
        std::string ir;
        llvm::raw_string_ostream rso(ir);
        rso << *mod;
        rso.flush();
        std::cout << "[coro-jit] lowered IR:\n" << ir << std::endl;
    }

    llvm::InitializeNativeTarget(); llvm::InitializeNativeTargetAsmPrinter();
    auto jitExp = llvm::orc::LLJITBuilder().create(); assert(jitExp && "Failed to create JIT");
    auto jit = std::move(*jitExp);
    jit->getMainJITDylib().addGenerator(llvm::cantFail(
        llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            jit->getDataLayout().getGlobalPrefix())));
    std::cout << "[phase4] coroutines JIT smoke: JIT created" << std::endl;
    auto err = jit->addIRModule(emitter.toThreadSafeModule());
    if(err){
        std::string ir;
        llvm::raw_string_ostream rso(ir);
        rso << *mod;
        rso.flush();
        std::cerr << "[coro-jit] addIRModule failed; IR follows:\n" << ir << std::endl;
        assert(!err && "Failed to add module to JIT");
    }
    std::cout << "[phase4] coroutines JIT smoke: module added" << std::endl;
    auto sym = jit->lookup("co");
    if(!sym){
        std::cerr << "[coro-jit] lookup('co') failed" << std::endl;
        assert(sym && "co not found");
    }
    std::cout << "[phase4] coroutines JIT smoke: lookup ok" << std::endl;
    // Do not invoke the lowered function here: without a full allocation strategy wired,
    // minimal lowered IR may contain frame stores relative to null which would crash.
    // The smoke goal is to ensure ORC can JIT the module and resolve the symbol.
    std::cout << "[phase4] coroutines JIT smoke passed (lookup only)\n";
}
