#include "edn/ir/pointer_func_ops.hpp"
#include "edn/ir/types.hpp"
#include <llvm/IR/IRBuilder.h>

namespace edn::ir::pointer_func_ops {

static std::string symName(const edn::node_ptr &n){ if(!n) return {}; if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name; if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; }
static std::string trimPct(const std::string& s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }

bool handle_addr(builder::State& S, const std::vector<edn::node_ptr>& il,
                 std::function<llvm::AllocaInst*(const std::string&, edn::TypeId, bool)> ensureSlot){
    if(il.size()!=4 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    if(std::get<edn::symbol>(il[0]->data).name!="addr") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId annot; try{ annot = S.tctx.parse_type(il[2]); }catch(...){ return false; }
    std::string srcName = trimPct(symName(il[3])); if(srcName.empty()) return false;
    const edn::Type &AT = S.tctx.at(annot); if(AT.kind!=edn::Type::Kind::Pointer) return false;
    edn::TypeId valTy = AT.pointee; if(S.vtypes.count(srcName)) valTy = S.vtypes[srcName];
    auto *slot = ensureSlot(srcName, valTy, /*initFromCurrent=*/true);
    S.vmap[dst]=slot; S.vtypes[dst]=annot; return true;
}

bool handle_deref(builder::State& S, const std::vector<edn::node_ptr>& il){
    if(il.size()!=4 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    if(std::get<edn::symbol>(il[0]->data).name!="deref") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId ty; try{ ty = S.tctx.parse_type(il[2]); }catch(...){ return false; }
    std::string ptrName = trimPct(symName(il[3])); if(ptrName.empty()) return false;
    if(!S.vtypes.count(ptrName)) return false;
    edn::TypeId pty = S.vtypes[ptrName]; const edn::Type &PT = S.tctx.at(pty);
    if(PT.kind!=edn::Type::Kind::Pointer || PT.pointee!=ty) return false;
    auto *pv = S.vmap[ptrName]; if(!pv) return false;
    auto *lv = S.builder.CreateLoad(S.map_type(ty), pv, dst);
    S.vmap[dst]=lv; S.vtypes[dst]=ty; return true;
}

bool handle_fnptr(builder::State& S, const std::vector<edn::node_ptr>& il){
    if(il.size()!=4 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    if(std::get<edn::symbol>(il[0]->data).name!="fnptr") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId pty; try{ pty = S.tctx.parse_type(il[2]); }catch(...){ return false; }
    std::string fname = symName(il[3]); if(fname.empty()) return false;
    auto *F = S.module.getFunction(fname);
    if(!F){
        const edn::Type &PT = S.tctx.at(pty); if(PT.kind!=edn::Type::Kind::Pointer) return false;
        const edn::Type &FT = S.tctx.at(PT.pointee); if(FT.kind!=edn::Type::Kind::Function) return false;
        std::vector<llvm::Type*> ps; for(auto pid: FT.params) ps.push_back(S.map_type(pid));
        auto *ftyDecl = llvm::FunctionType::get(S.map_type(FT.ret), ps, FT.variadic);
        F = llvm::Function::Create(ftyDecl, llvm::Function::ExternalLinkage, fname, &S.module);
    }
    if(!F) return false;
    S.vmap[dst]=F; S.vtypes[dst]=pty; return true;
}

bool handle_call_indirect(builder::State& S, const std::vector<edn::node_ptr>& il){
    if(il.size()<4 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    if(std::get<edn::symbol>(il[0]->data).name!="call-indirect") return false;
    std::string dst = trimPct(symName(il[1]));
    edn::TypeId retTy; try{ retTy = S.tctx.parse_type(il[2]); }catch(...){ return false; }
    std::string fptrName = trimPct(symName(il[3])); if(fptrName.empty()) return false;
    if(!S.vtypes.count(fptrName)) return false;
    edn::TypeId fpty = S.vtypes[fptrName]; const edn::Type &FPT = S.tctx.at(fpty);
    if(FPT.kind!=edn::Type::Kind::Pointer) return false; const edn::Type &FT = S.tctx.at(FPT.pointee);
    if(FT.kind!=edn::Type::Kind::Function) return false;
    auto *calleeV = S.vmap[fptrName]; if(!calleeV) return false;
    std::vector<llvm::Value*> args; bool bad=false;
    for(size_t ai=4; ai<il.size(); ++ai){
        std::string an = trimPct(symName(il[ai]));
        if(an.empty() || !S.vmap.count(an)){ bad=true; break; }
        args.push_back(S.vmap[an]);
    }
    if(bad) return false;
    llvm::Type *rawFty = S.map_type(FPT.pointee);
    if(!llvm::isa<llvm::FunctionType>(rawFty)) {
        fprintf(stderr, "[dbg][cast] expected FunctionType in call-indirect but got kind=%u for pointee type id=%llu\n", (unsigned)rawFty->getTypeID(), (unsigned long long)FPT.pointee);
    }
    llvm::FunctionType *fty = llvm::cast<llvm::FunctionType>(rawFty);
    auto *ci = S.builder.CreateCall(fty, calleeV, args, fty->getReturnType()->isVoidTy()?"":dst);
    if(!fty->getReturnType()->isVoidTy()){
        S.vmap[dst]=ci; S.vtypes[dst]=retTy;
    }
    return true;
}

} // namespace edn::ir::pointer_func_ops
