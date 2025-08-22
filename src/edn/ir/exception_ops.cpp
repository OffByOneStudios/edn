#include "edn/ir/exception_ops.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>

namespace edn::ir::exception_ops {

bool handle_panic(Context& C, const std::vector<edn::node_ptr>& il){
    if(il.size()!=1) return false;
    if(!il[0] || !std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="panic") return false;
    auto &B = C.builder;
    // Unwinding path (Itanium)
    if(C.panicUnwind && C.enableEHItanium && C.selectedPersonality){
        auto *i8Ty = llvm::Type::getInt8Ty(C.llctx);
        auto *i8Ptr = llvm::PointerType::getUnqual(i8Ty);
        auto *throwFTy = llvm::FunctionType::get(llvm::Type::getVoidTy(C.llctx), {i8Ptr, i8Ptr, i8Ptr}, false);
        auto throwCallee = C.module.getOrInsertFunction("__cxa_throw", throwFTy);
        llvm::Value *nullp = llvm::ConstantPointerNull::get(i8Ptr);
        llvm::BasicBlock *exTarget = C.itnExceptTargetStack.empty()? edn::ir::exceptions::create_panic_cleanup_landingpad(C.F, B) : C.itnExceptTargetStack.back();
        auto *unwCont = llvm::BasicBlock::Create(C.llctx, "panic.cont", C.F);
        B.CreateInvoke(throwFTy, throwCallee.getCallee(), unwCont, exTarget, {nullp, nullp, nullp});
        llvm::IRBuilder<> nb(unwCont);
        if(C.enableDebugInfo && C.F->getSubprogram()) nb.SetCurrentDebugLocation(B.getCurrentDebugLocation());
        nb.CreateUnreachable();
        return true;
    }
    // Unwinding path (SEH)
    if(C.panicUnwind && C.enableEHSEH && C.selectedPersonality){
        auto *i32 = llvm::Type::getInt32Ty(C.llctx);
        auto *i8 = llvm::Type::getInt8Ty(C.llctx);
        auto *i8ptr = llvm::PointerType::getUnqual(i8);
        auto *raiseFTy = llvm::FunctionType::get(llvm::Type::getVoidTy(C.llctx), {i32,i32,i32,i8ptr}, false);
        auto raiseCallee = C.module.getOrInsertFunction("RaiseException", raiseFTy);
        auto *code = llvm::ConstantInt::get(i32, 0xE0ED0001);
        auto *flags = llvm::ConstantInt::get(i32, 1);
        auto *nargs = llvm::ConstantInt::get(i32, 0);
        llvm::Value *argsPtr = llvm::ConstantPointerNull::get(i8ptr);
        llvm::BasicBlock *exTarget = C.sehExceptTargetStack.empty() ? nullptr : C.sehExceptTargetStack.back();
        if(!exTarget){ C.sehCleanupBB = edn::ir::exceptions::ensure_seh_cleanup(C.F, C.builder, C.sehCleanupBB); exTarget = C.sehCleanupBB; }
        auto *unwCont = llvm::BasicBlock::Create(C.llctx, "panic.cont", C.F);
        B.CreateInvoke(raiseFTy, raiseCallee.getCallee(), unwCont, exTarget, {code, flags, nargs, argsPtr});
        llvm::IRBuilder<> nb(unwCont);
        if(C.enableDebugInfo && C.F->getSubprogram()) nb.SetCurrentDebugLocation(B.getCurrentDebugLocation());
        nb.CreateUnreachable();
        return true;
    }
    // Default: trap
    auto callee = llvm::Intrinsic::getDeclaration(&C.module, llvm::Intrinsic::trap);
    B.CreateCall(callee);
    B.CreateUnreachable();
    return true;
}

bool handle_try(Context& C, const std::vector<edn::node_ptr>& il){
    if(il.empty() || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="try") return false;
    // (try :body [ ... ] :catch [ ... ])
    node_ptr bodyNode=nullptr, catchNode=nullptr;
    for(size_t i=1;i<il.size();++i){
        if(!il[i] || !std::holds_alternative<edn::keyword>(il[i]->data)) break;
        std::string kw = std::get<edn::keyword>(il[i]->data).name;
        if(++i>=il.size()) break;
        auto val = il[i];
        if(kw=="body" && val && std::holds_alternative<edn::vector_t>(val->data)) bodyNode = val;
        else if(kw=="catch" && val && std::holds_alternative<edn::vector_t>(val->data)) catchNode = val;
    }
    if(!bodyNode || !catchNode) return true; // malformed; treat as handled (skip)
    auto &B = C.builder;
    auto &LL = C.llctx;
    auto *bodyBB = llvm::BasicBlock::Create(LL, "try.body." + std::to_string(C.cfCounter++), C.F);
    auto *contBB = llvm::BasicBlock::Create(LL, "try.end." + std::to_string(C.cfCounter++), C.F);
    if(C.enableEHSEH && C.selectedPersonality){
        auto *catchHandlerBB = llvm::BasicBlock::Create(LL, "try.handler." + std::to_string(C.cfCounter++), C.F);
        if(!B.GetInsertBlock()->getTerminator()) B.CreateBr(bodyBB);
        auto scf = edn::ir::exceptions::create_seh_catch_scaffold(C.F, B, contBB);
        B.SetInsertPoint(scf.catchPadBB);
        B.CreateCatchRet(scf.catchPad, catchHandlerBB);
        B.SetInsertPoint(bodyBB);
        C.sehExceptTargetStack.push_back(scf.dispatchBB);
        C.emit_nested(std::get<edn::vector_t>(bodyNode->data).elems);
        C.sehExceptTargetStack.pop_back();
        if(!B.GetInsertBlock()->getTerminator()) B.CreateBr(contBB);
        B.SetInsertPoint(catchHandlerBB);
        C.emit_nested(std::get<edn::vector_t>(catchNode->data).elems);
        if(!B.GetInsertBlock()->getTerminator()) B.CreateBr(contBB);
        B.SetInsertPoint(contBB);
        return true;
    } else if(C.enableEHItanium && C.selectedPersonality){
        auto *handlerBB = llvm::BasicBlock::Create(LL, "try.handler." + std::to_string(C.cfCounter++), C.F);
        if(!B.GetInsertBlock()->getTerminator()) B.CreateBr(bodyBB);
        auto *lpadBB = edn::ir::exceptions::create_catch_all_landingpad(C.F, B, handlerBB);
        B.SetInsertPoint(bodyBB);
        C.itnExceptTargetStack.push_back(lpadBB);
        C.emit_nested(std::get<edn::vector_t>(bodyNode->data).elems);
        C.itnExceptTargetStack.pop_back();
        if(!B.GetInsertBlock()->getTerminator()) B.CreateBr(contBB);
        B.SetInsertPoint(handlerBB);
        C.emit_nested(std::get<edn::vector_t>(catchNode->data).elems);
        if(!B.GetInsertBlock()->getTerminator()) B.CreateBr(contBB);
        B.SetInsertPoint(contBB);
        return true;
    } else {
        if(!B.GetInsertBlock()->getTerminator()) B.CreateBr(bodyBB);
        B.SetInsertPoint(bodyBB);
        C.emit_nested(std::get<edn::vector_t>(bodyNode->data).elems);
        if(!B.GetInsertBlock()->getTerminator()) B.CreateBr(contBB);
        B.SetInsertPoint(contBB);
        return true;
    }
}

} // namespace edn::ir::exception_ops
