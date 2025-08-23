#include "edn/ir/exceptions.hpp"

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>

namespace edn::ir::exceptions {

static llvm::Constant* get_personality_decl(llvm::Module& M, bool itanium){
    auto& C = M.getContext();
    auto* i32 = llvm::Type::getInt32Ty(C);
    auto* fty = llvm::FunctionType::get(i32, /*isVarArg*/ true);
    auto name = itanium ? "__gxx_personality_v0" : "__C_specific_handler";
    auto callee = M.getOrInsertFunction(name, fty);
    return llvm::cast<llvm::Constant>(callee.getCallee());
}

llvm::Constant* select_personality(llvm::Module& M, const edn::EmitEnv& env){
    // If neither requested, return null
    if(!(env.personalityItanium || env.personalitySEH)) return nullptr;
#ifdef _WIN32
    // On Windows prefer SEH unless Itanium is explicitly selected
    if(env.personalityItanium) return get_personality_decl(M, /*itanium*/true);
    return get_personality_decl(M, /*itanium*/false);
#else
    // On non-Windows, prefer Itanium unless SEH is the only selected
    if(env.personalitySEH && !env.personalityItanium) return get_personality_decl(M, /*itanium*/false);
    return get_personality_decl(M, /*itanium*/true);
#endif
}

llvm::BasicBlock* create_panic_cleanup_landingpad(llvm::Function* F, llvm::IRBuilder<>& refBuilder){
    auto& C = F->getContext();
    auto* llctx = &C;
    auto* lpadBB = llvm::BasicBlock::Create(C, "panic.lpad", F);
    llvm::IRBuilder<> eb(lpadBB);
    // Preserve current debug location if any
    eb.SetCurrentDebugLocation(refBuilder.getCurrentDebugLocation());
    auto* i8 = llvm::Type::getInt8Ty(C);
    auto* i8p = llvm::PointerType::getUnqual(i8);
    auto* i32 = llvm::Type::getInt32Ty(C);
    auto* lpadTy = llvm::StructType::get(C, {i8p, i32});
    auto* lp = eb.CreateLandingPad(lpadTy, 0, "lpad");
    lp->setCleanup(true);
    eb.CreateResume(lp);
    (void)llctx; // silence unused in some builds
    return lpadBB;
}

llvm::BasicBlock* create_catch_all_landingpad(llvm::Function* F,
                                              llvm::IRBuilder<>& refBuilder,
                                              llvm::BasicBlock* handlerBB){
    auto& C = F->getContext();
    auto* lpadBB = llvm::BasicBlock::Create(C, "try.lpad", F);
    llvm::IRBuilder<> eb(lpadBB);
    eb.SetCurrentDebugLocation(refBuilder.getCurrentDebugLocation());
    auto* i8 = llvm::Type::getInt8Ty(C);
    auto* i8p = llvm::PointerType::getUnqual(i8);
    auto* i32 = llvm::Type::getInt32Ty(C);
    auto* lpadTy = llvm::StructType::get(C, {i8p, i32});
    auto* lp = eb.CreateLandingPad(lpadTy, 1, "lpad");
    lp->addClause(llvm::ConstantPointerNull::get(i8p)); // catch-all
    eb.CreateBr(handlerBB);
    return lpadBB;
}

llvm::BasicBlock* ensure_seh_cleanup(llvm::Function* F,
                                     llvm::IRBuilder<>& refBuilder,
                                     llvm::BasicBlock* existingCleanupBB){
    if(existingCleanupBB) return existingCleanupBB;
    auto& C = F->getContext();
    auto* bb = llvm::BasicBlock::Create(C, "seh.cleanup", F);
    llvm::IRBuilder<> eb(bb);
    eb.SetCurrentDebugLocation(refBuilder.getCurrentDebugLocation());
    auto* tokNone = llvm::ConstantTokenNone::get(C);
    auto* cp = eb.CreateCleanupPad(tokNone, {}, "cp");
    (void)cp;
    eb.CreateCleanupRet(cp, nullptr);
    return bb;
}

SEHCatchScaffold create_seh_catch_scaffold(llvm::Function* F,
                                           llvm::IRBuilder<>& refBuilder,
                                           llvm::BasicBlock* contBB){
    SEHCatchScaffold out{nullptr, nullptr, nullptr};
    auto& C = F->getContext();
    out.dispatchBB = llvm::BasicBlock::Create(C, "try.catch.dispatch", F);
    out.catchPadBB = llvm::BasicBlock::Create(C, "try.catch.pad", F);
    // Build catchswitch in dispatchBB
    {
        llvm::IRBuilder<> eb(out.dispatchBB);
        eb.SetCurrentDebugLocation(refBuilder.getCurrentDebugLocation());
        auto* tokNone = llvm::ConstantTokenNone::get(C);
        auto* cs = eb.CreateCatchSwitch(tokNone, contBB, 1, "cs");
        auto* csi = llvm::cast<llvm::CatchSwitchInst>(cs);
        csi->addHandler(out.catchPadBB);
    }
    // Place a catchpad in catchPadBB with three i8* typeinfo arguments (catch-all per our tests)
    {
        llvm::IRBuilder<> eb(out.catchPadBB);
        eb.SetCurrentDebugLocation(refBuilder.getCurrentDebugLocation());
        auto* i8 = llvm::Type::getInt8Ty(C);
        auto* i8p = llvm::PointerType::getUnqual(i8);
        llvm::Value* ti0 = llvm::ConstantPointerNull::get(i8p);
        llvm::Value* ti1 = llvm::ConstantPointerNull::get(i8p);
        llvm::Value* ti2 = llvm::ConstantPointerNull::get(i8p);
        // We need the catchswitch token; fetch it by taking the terminator of dispatchBB
        auto* csTerm = out.dispatchBB->getTerminator();
        if(!csTerm || !llvm::isa<llvm::CatchSwitchInst>(csTerm)) {
            // Defensive: log and create an unreachable if structure malformed mid-emission
            if(csTerm){
                fprintf(stderr, "[guard][exceptions] expected CatchSwitchInst terminator but got opcode=%u\n", (unsigned)csTerm->getOpcode());
            } else {
                fprintf(stderr, "[guard][exceptions] missing catchswitch terminator in dispatchBB\n");
            }
            auto* unr = eb.CreateUnreachable(); (void)unr;
        } else {
            auto* cs = llvm::cast<llvm::CatchSwitchInst>(csTerm);
            out.catchPad = eb.CreateCatchPad(cs, {ti0, ti1, ti2}, "cpad");
        }
    }
    return out;
}

} // namespace edn::ir::exceptions
