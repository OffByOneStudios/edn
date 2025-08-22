#include "edn/ir/coro_ops.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>

namespace edn::ir::coro_ops {

static std::string symName(const edn::node_ptr &n){ if(!n) return {}; if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name; if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; }
static std::string trimPct(const std::string& s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }

bool handle(builder::State& S,
            const std::vector<edn::node_ptr>& il,
            bool enableCoro,
            llvm::Value*& lastCoroIdTok){
    if(il.empty() || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    const std::string op = std::get<edn::symbol>(il[0]->data).name;
    auto &B = S.builder;
    auto &M = S.module;
    auto &LL = S.llctx;
    auto &tctx = S.tctx;
    auto getName = [&](size_t idx){ return idx<il.size()? trimPct(symName(il[idx])): std::string(); };
    if(op=="coro-begin" && il.size()==2){
        std::string dst = getName(1); if(dst.empty()) return true; // nothing emitted
        auto *i8 = llvm::Type::getInt8Ty(LL);
        auto *i8p = llvm::PointerType::getUnqual(i8);
        llvm::Value* hdl=nullptr;
        if(enableCoro){
            auto idDecl = llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::coro_id);
            auto beginDecl = llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::coro_begin);
            auto *i32 = llvm::Type::getInt32Ty(LL);
            llvm::Value *alignV = llvm::ConstantInt::get(i32, 0);
            llvm::Value *nullp = llvm::ConstantPointerNull::get(i8p);
            auto *idTok = B.CreateCall(idDecl, {alignV, nullp, nullp, nullp}, "coro.id");
            lastCoroIdTok = idTok;
            hdl = B.CreateCall(beginDecl, {idTok, nullp}, dst);
        } else {
            hdl = llvm::ConstantPointerNull::get(i8p);
        }
        S.vmap[dst]=hdl;
        S.vtypes[dst]= tctx.get_pointer(tctx.get_base(edn::BaseType::I8));
        return true;
    }
    if(op=="coro-suspend" && il.size()==3){
        std::string dst=getName(1); std::string h=getName(2); if(dst.empty()||h.empty()||!S.vmap.count(h)) return true;
        llvm::Value* st=nullptr; if(enableCoro){
            auto suspendDecl = llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::coro_suspend);
            auto *i1 = llvm::Type::getInt1Ty(LL);
            auto *tokArg = S.vmap[h]->getType()->isTokenTy()? S.vmap[h] : (llvm::Value*)llvm::ConstantTokenNone::get(LL);
            llvm::Value *isFinal = llvm::ConstantInt::getFalse(i1);
            st = B.CreateCall(suspendDecl, {tokArg, isFinal}, dst);
        } else { st = llvm::ConstantInt::get(llvm::Type::getInt8Ty(LL), 0); }
        S.vmap[dst]=st; S.vtypes[dst]= tctx.get_base(edn::BaseType::I8); return true;
    }
    if(op=="coro-final-suspend" && il.size()==3){
        std::string dst=getName(1); std::string h=getName(2); if(dst.empty()||h.empty()||!S.vmap.count(h)) return true;
        llvm::Value* st=nullptr; if(enableCoro){
            auto suspendDecl = llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::coro_suspend);
            auto *i1 = llvm::Type::getInt1Ty(LL);
            auto *tokArg = S.vmap[h]->getType()->isTokenTy()? S.vmap[h] : (llvm::Value*)llvm::ConstantTokenNone::get(LL);
            llvm::Value *isFinal = llvm::ConstantInt::getTrue(i1);
            st = B.CreateCall(suspendDecl, {tokArg, isFinal}, dst);
        } else { st = llvm::ConstantInt::get(llvm::Type::getInt8Ty(LL), 0); }
        S.vmap[dst]=st; S.vtypes[dst]= tctx.get_base(edn::BaseType::I8); return true;
    }
    if(op=="coro-save" && il.size()==3){
        std::string dst=getName(1); std::string h=getName(2); if(dst.empty()||h.empty()||!S.vmap.count(h)) return true;
        llvm::Value* tokV=nullptr; if(enableCoro){
            auto saveDecl = llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::coro_save);
            tokV = B.CreateCall(saveDecl, {S.vmap[h]}, dst);
        } else { tokV = llvm::ConstantTokenNone::get(LL); }
        S.vmap[dst]=tokV; return true;
    }
    if(op=="coro-id" && il.size()==2){
        std::string dst=getName(1); if(dst.empty()) return true; if(lastCoroIdTok) S.vmap[dst]= lastCoroIdTok; return true;
    }
    if(op=="coro-size" && il.size()==2){
        std::string dst=getName(1); if(dst.empty()) return true; llvm::Value* sz=nullptr; if(enableCoro){
            auto *i64 = llvm::Type::getInt64Ty(LL); auto sizeDecl = llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::coro_size, {i64}); sz = B.CreateCall(sizeDecl, {}, dst);
        } else { sz = llvm::ConstantInt::get(llvm::Type::getInt64Ty(LL), 0); }
        S.vmap[dst]=sz; S.vtypes[dst]= tctx.get_base(edn::BaseType::I64); return true;
    }
    if(op=="coro-alloc" && il.size()==3){
        std::string dst=getName(1); std::string cid=getName(2); if(dst.empty()||cid.empty()||!S.vmap.count(cid)) return true; llvm::Value* need=nullptr; if(enableCoro){ auto allocDecl = llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::coro_alloc); need = B.CreateCall(allocDecl, {S.vmap[cid]}, dst);} else { need = llvm::ConstantInt::getFalse(llvm::Type::getInt1Ty(LL)); } S.vmap[dst]=need; S.vtypes[dst]= tctx.get_base(edn::BaseType::I1); return true;
    }
    if(op=="coro-free" && il.size()==4){
        std::string dst=getName(1); std::string cid=getName(2); std::string h=getName(3); if(dst.empty()||cid.empty()||h.empty()||!S.vmap.count(cid)||!S.vmap.count(h)) return true; llvm::Value* mem=nullptr; if(enableCoro){ auto freeDecl = llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::coro_free); auto *i8=llvm::Type::getInt8Ty(LL); auto *i8p=llvm::PointerType::getUnqual(i8); auto *hdlCast = B.CreateBitCast(S.vmap[h], i8p); mem = B.CreateCall(freeDecl, {S.vmap[cid], hdlCast}, dst);} else { mem = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(LL))); } S.vmap[dst]=mem; S.vtypes[dst]= tctx.get_pointer(tctx.get_base(edn::BaseType::I8)); return true;
    }
    if(op=="coro-promise" && il.size()==3){
        std::string dst=getName(1); std::string h=getName(2); if(dst.empty()||h.empty()||!S.vmap.count(h)) return true; llvm::Value* p=nullptr; if(enableCoro){ auto promDecl = llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::coro_promise); auto *i32=llvm::Type::getInt32Ty(LL); auto *i1=llvm::Type::getInt1Ty(LL); auto *i8=llvm::Type::getInt8Ty(LL); auto *i8p=llvm::PointerType::getUnqual(i8); llvm::Value* alignZero=llvm::ConstantInt::get(i32,0); llvm::Value* fromPromise=llvm::ConstantInt::getFalse(i1); auto *hdlCast = B.CreateBitCast(S.vmap[h], i8p); p = B.CreateCall(promDecl, {hdlCast, alignZero, fromPromise}, dst);} else { p = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(LL))); } S.vmap[dst]=p; S.vtypes[dst]= tctx.get_pointer(tctx.get_base(edn::BaseType::I8)); return true;
    }
    if(op=="coro-resume" && il.size()==2){ std::string h=getName(1); if(h.empty()||!S.vmap.count(h)) return true; if(enableCoro){ auto resumeDecl = llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::coro_resume); auto *i8=llvm::Type::getInt8Ty(LL); auto *i8p=llvm::PointerType::getUnqual(i8); auto *hdlCast=B.CreateBitCast(S.vmap[h], i8p); B.CreateCall(resumeDecl,{hdlCast}); } return true; }
    if(op=="coro-destroy" && il.size()==2){ std::string h=getName(1); if(h.empty()||!S.vmap.count(h)) return true; if(enableCoro){ auto destroyDecl = llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::coro_destroy); auto *i8=llvm::Type::getInt8Ty(LL); auto *i8p=llvm::PointerType::getUnqual(i8); auto *hdlCast=B.CreateBitCast(S.vmap[h], i8p); B.CreateCall(destroyDecl,{hdlCast}); } return true; }
    if(op=="coro-done" && il.size()==3){ std::string dst=getName(1); std::string h=getName(2); if(dst.empty()||h.empty()||!S.vmap.count(h)) return true; llvm::Value* d=nullptr; if(enableCoro){ auto doneDecl = llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::coro_done); auto *i8=llvm::Type::getInt8Ty(LL); auto *i8p=llvm::PointerType::getUnqual(i8); auto *hdlCast=B.CreateBitCast(S.vmap[h], i8p); d = B.CreateCall(doneDecl,{hdlCast}, dst);} else { d = llvm::ConstantInt::getFalse(llvm::Type::getInt1Ty(LL)); } S.vmap[dst]=d; S.vtypes[dst]= tctx.get_base(edn::BaseType::I1); return true; }
    if(op=="coro-end" && il.size()==2){ std::string h=getName(1); if(h.empty()||!S.vmap.count(h)) return true; if(enableCoro){ auto endDecl=llvm::Intrinsic::getDeclaration(&M, llvm::Intrinsic::coro_end); auto *i1=llvm::Type::getInt1Ty(LL); llvm::Value* u0=llvm::ConstantInt::getFalse(i1); auto *tokNone=llvm::ConstantTokenNone::get(LL); (void)B.CreateCall(endDecl,{S.vmap[h], u0, tokNone}, "coro.end"); } return true; }
    return false; // not a coro op
}

} // namespace edn::ir::coro_ops
