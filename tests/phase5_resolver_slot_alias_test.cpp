#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"

// Minimal regression test for slot alias normalization (initializer suffix stripping)
// Ensures an increment uses the variable's slot load instead of the constant initializer load.
// EDN pattern:
// (as %z i32 %z_init)
// (const %two i32 2)
// (add %tmp i32 %z %two)
// (assign %z %tmp)
// We expect the generated IR for 'add' to use a load from %z.slot.

static const char* SRC = R"((module
    (fn :name "slot_alias" :ret i32 :params [ ] :body [
         (const %z_init i32 0)
         (as %z i32 %z_init)
         (const %two i32 2)
         (add %tmp1 i32 %z %two) ; first add may legally use initializer constant
         (assign %z %tmp1)
         (add %tmp2 i32 %z %two) ; second add must use slot load (post-mutation)
         (ret i32 %tmp2)
    ])
))";

int run_phase5_resolver_slot_alias_test(){
    std::cout << "[phase5] resolver slot alias test...\n";
    // Force optimization pipeline off so we can inspect raw emitted IR before DCE/instcombine
#ifdef _WIN32
    _putenv_s("EDN_ENABLE_PASSES", "0");
    _putenv_s("EDN_OPT_LEVEL", "0");
    _putenv_s("EDN_PASS_PIPELINE", "");
#else
    setenv("EDN_ENABLE_PASSES", "0", 1);
    setenv("EDN_OPT_LEVEL", "0", 1);
    unsetenv("EDN_PASS_PIPELINE");
#endif
    auto ast = edn::parse(SRC);
    edn::TypeContext ctx; edn::TypeCheckResult tcr; edn::TypeChecker tc(ctx); tcr = tc.check_module(ast); assert(tcr.success);
    edn::IREmitter emitter(ctx); edn::TypeCheckResult res; auto *mod = emitter.emit(ast, res); assert(mod && res.success);
    auto *F = mod->getFunction("slot_alias"); assert(F);
    bool sawLoadFromSlot=false; bool sawSecondAdd=false; bool secondAddOperandFromSlot=false; 
    for(auto &BB : *F){
        for(auto &I : BB){
            if(auto *LI = llvm::dyn_cast<llvm::LoadInst>(&I)){
                if(LI->getPointerOperand()->getName().ends_with("z.slot")) sawLoadFromSlot=true;
            }
            if(auto *BI = llvm::dyn_cast<llvm::BinaryOperator>(&I)){
                if(BI->getOpcode()==llvm::Instruction::Add && BI->getName()=="tmp2"){
                    sawSecondAdd=true;
                    if(auto *LI = llvm::dyn_cast<llvm::LoadInst>(BI->getOperand(0))){
                        if(LI->getPointerOperand()->getName().ends_with("z.slot")) secondAddOperandFromSlot=true;
                    }
                }
            }
        }
    }
    assert(sawSecondAdd && "expected second add instruction tmp2");
    assert(sawLoadFromSlot && "expected some load from %z.slot in function");
    assert(secondAddOperandFromSlot && "expected second add operand 0 to be a load from %z.slot");
    std::cout << "[phase5] resolver slot alias test passed\n";
    return 0;
}
