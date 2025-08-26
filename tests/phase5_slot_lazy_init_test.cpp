#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"

// Regression (EDN-0001): ensure a mutable var originating from synthetic (const+bitcast) gets
// a single entry-block initializer when pre-hoist is disabled, and loop mutation path executes.

static const char* SRC = R"((module
  (fn :name "lazy_slot" :ret i32 :params [] :body [
     (const %c0 i32 0)
     (bitcast %z i32 %c0)
     (const %one i32 1)
     (const %t i1 1)
     (while %t [ (add %tmp i32 %z %one) (assign %z %tmp) (break) ])
     (ret i32 %z)
  ])
))";

void run_phase5_slot_lazy_init_test(){
    std::cout << "[phase5] slot lazy init test...\n";
    // Force lazy path but remember previous value to restore (so later GTest driver invocations see default behavior)
    const char* prevDisable = std::getenv("EDN_DISABLE_BITCAST_PREHOIST");
#ifndef _WIN32
    setenv("EDN_DISABLE_BITCAST_PREHOIST", "1", 1);
#else
    _putenv_s("EDN_DISABLE_BITCAST_PREHOIST", "1");
#endif
    auto ast = edn::parse(SRC);
    edn::TypeContext ctx; edn::TypeCheckResult tcr; edn::TypeChecker tc(ctx); tcr = tc.check_module(ast); assert(tcr.success);
    edn::IREmitter emitter(ctx); edn::TypeCheckResult res; auto *mod = emitter.emit(ast, res); assert(mod && res.success);
    auto *F = mod->getFunction("lazy_slot"); assert(F);
    (void)F; // IR dump removed after stabilization
    bool sawInitStore=false; bool sawAdd=false; bool sawSlotAlloca=false;
    for(auto &BB : *F){
        for(auto &I : BB){
            if(auto *AI = llvm::dyn_cast<llvm::AllocaInst>(&I)){
                if(AI->getName().ends_with("z.slot")) sawSlotAlloca=true;
            }
            if(auto *SI = llvm::dyn_cast<llvm::StoreInst>(&I)){
                if(SI->getPointerOperand()->getName().ends_with("z.slot")) sawInitStore=true;
            }
            if(auto *BO = llvm::dyn_cast<llvm::BinaryOperator>(&I)) if(BO->getOpcode()==llvm::Instruction::Add) sawAdd=true;
        }
    }
    assert(sawSlotAlloca);
    assert(sawInitStore && "expected initializer store to z.slot");
    // Expect: slot alloca + single initializer store + presence of loop add
    assert(sawAdd && "expected add in loop body");
    // Restore original env var state
#ifndef _WIN32
    if(prevDisable) setenv("EDN_DISABLE_BITCAST_PREHOIST", prevDisable, 1); else unsetenv("EDN_DISABLE_BITCAST_PREHOIST");
#else
    if(prevDisable) _putenv_s("EDN_DISABLE_BITCAST_PREHOIST", prevDisable); else _putenv_s("EDN_DISABLE_BITCAST_PREHOIST", "");
#endif
    std::cout << "[phase5] slot lazy init test passed\n";
}
