#include <cassert>
#include <iostream>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>

using namespace edn;

void run_phase4_debug_info_locals_test(){
    std::cout << "[phase4] debug info locals test...\n";
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_DEBUG", "1");
#else
    setenv("EDN_ENABLE_DEBUG", "1", 1);
#endif

    const char* SRC = R"EDN((module
      (fn :name "foo" :ret i32 :params [ (param i32 %a) (param i32 %b) ] :body [
         (alloca %p i32)
         (add %c i32 %a %b)
         (store i32 %p %c)
         (load %v i32 %p)
         (ret i32 %v)
      ])
    ))EDN";

    auto ast = parse(SRC);
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
    assert(tcres.success && mod);
    auto *F = mod->getFunction("foo");
    assert(F && F->getSubprogram());

    // Check we emitted at least one dbg.declare for the alloca and dbg.value for params
    bool hasDeclare=false, hasParamDbg=false;
    for(auto &BB : *F){
        for(auto &I : BB){
            if(auto *ci = llvm::dyn_cast<llvm::CallInst>(&I)){
                if(auto *called = ci->getCalledFunction()){
                    auto name = called->getName();
                    if(name == "llvm.dbg.declare") hasDeclare = true;
                    if(name == "llvm.dbg.value") hasParamDbg = true;
                }
            }
        }
    }
    assert(hasDeclare && "expected dbg.declare for local alloca");
    assert(hasParamDbg && "expected at least one dbg.value for params");

    std::cout << "[phase4] debug info locals test passed\n";
}
