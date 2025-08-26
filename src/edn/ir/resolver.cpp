#include "edn/ir/resolver.hpp"
#include <cstring>
#include <llvm/IR/Instructions.h>
#include "edn/ir/types.hpp"

namespace edn::ir::resolver {

static llvm::BasicBlock* entry_block(builder::State& S){
    auto *F = S.builder.GetInsertBlock() ? S.builder.GetInsertBlock()->getParent() : nullptr;
    return F ? &F->getEntryBlock() : nullptr;
}

llvm::AllocaInst* ensure_slot(builder::State& S, const std::string& name, edn::TypeId ty, bool initFromCurrent){
    if(auto it_existingSlot = S.varSlots.find(name); it_existingSlot != S.varSlots.end()) return it_existingSlot->second;
    auto *entryBB_forAlloc = entry_block(S);
    if(!entryBB_forAlloc){ return nullptr; }
    if(const char* dbg = std::getenv("EDN_DEBUG_AS")) fprintf(stderr, "[dbg][ensure_slot][enter] name=%s typeId=%llu entryBB=%p\n", name.c_str(), (unsigned long long)ty, (void*)entryBB_forAlloc);
    llvm::Type *rawTy = nullptr;
    try { rawTy = S.map_type(ty); } catch(...){ rawTy = nullptr; }
    if(!rawTy){ if(const char* dbg = std::getenv("EDN_DEBUG_AS")) fprintf(stderr, "[dbg][ensure_slot][abort] name=%s reason=map_type-null\n", name.c_str()); return nullptr; }
    if(rawTy->isVoidTy()) { if(const char* dbg = std::getenv("EDN_DEBUG_AS")) fprintf(stderr, "[dbg][ensure_slot][abort] name=%s reason=void-ty\n", name.c_str()); return nullptr; }
    bool emptyEntry = entryBB_forAlloc->begin()==entryBB_forAlloc->end();
    if(const char* dbg = std::getenv("EDN_DEBUG_AS")){
        auto *F = entryBB_forAlloc->getParent();
        fprintf(stderr, "[dbg][ensure_slot][alloc] name=%s rawTyID=%u func=%p empty=%d\n", name.c_str(), (unsigned)rawTy->getTypeID(), (void*)F, (int)emptyEntry);
    }
    llvm::AllocaInst* slot_alloc = nullptr;
    if(emptyEntry){
        llvm::IRBuilder<> entryAllocBuilder(entryBB_forAlloc);
        slot_alloc = entryAllocBuilder.CreateAlloca(rawTy, nullptr, name + ".slot");
    } else {
        llvm::IRBuilder<> entryAllocBuilder(&*entryBB_forAlloc->getFirstInsertionPt());
        slot_alloc = entryAllocBuilder.CreateAlloca(rawTy, nullptr, name + ".slot");
    }
    if(const char* dbg = std::getenv("EDN_DEBUG_AS")) fprintf(stderr, "[dbg][ensure_slot][post-alloc] name=%s slot=%p\n", name.c_str(), (void*)slot_alloc);
    S.varSlots[name] = slot_alloc;
    // Record the variable's value type (not pointer). Slot presence tracked in varSlots.
    S.vtypes[name] = ty;
    if(initFromCurrent){
        if(auto itCurrentVal = S.vmap.find(name); itCurrentVal != S.vmap.end() && itCurrentVal->second){
            // Revert to original behavior: perform store at current insertion point.
            S.builder.CreateStore(itCurrentVal->second, slot_alloc);
        }
    }
    // Shadow tracking
    S.shadowSlots[name].push_back({S.lexicalDepth, slot_alloc, ty});
    // Do NOT put the raw slot pointer into vmap as the value; loads will produce SSA values.
    return slot_alloc;
}

void bind_value(builder::State& S, const std::string& name, llvm::Value* v, edn::TypeId ty){
    S.vmap[name] = v;
    S.vtypes[name] = ty;
}

static llvm::Value* load_from_aliased(builder::State& S, const std::string& key){
    auto it = S.initAlias.find(key); if(it==S.initAlias.end()) return nullptr;
    const std::string &var = it->second;
    if(auto s2 = S.varSlots.find(var); s2 != S.varSlots.end()){
        auto t2 = S.vtypes.find(var); if(t2==S.vtypes.end()) return nullptr;
        return S.builder.CreateLoad(S.map_type(t2->second), s2->second, var);
    }
    if(auto vv = S.vmap.find(var); vv != S.vmap.end()) return vv->second;
    return nullptr;
}

llvm::Value* get_value(builder::State& S, const edn::node_ptr& n){
    if(!n) return nullptr;
    std::string nm;
    if(std::holds_alternative<edn::symbol>(n->data)) nm = std::get<edn::symbol>(n->data).name; else if(std::holds_alternative<std::string>(n->data)) nm = std::get<std::string>(n->data); else return nullptr;
    if(!nm.empty() && nm[0]=='%') nm.erase(0,1);
    if(nm.empty()) return nullptr;
    // Always prefer an existing slot (mutable variable) to any initializer alias to avoid
    // baking an initial constant into loop bodies before mutation.
    if(auto sIt = S.varSlots.find(nm); sIt != S.varSlots.end()){
        auto tIt = S.vtypes.find(nm); if(tIt==S.vtypes.end()) return nullptr; return S.builder.CreateLoad(S.map_type(tIt->second), sIt->second, nm);
    }
    if(auto aliased = load_from_aliased(S, nm)) return aliased;
    auto normalized = nm;
    auto stripSuffix = [&](const char* suf){ size_t L = std::strlen(suf); if(normalized.size()>=L && normalized.rfind(suf)==normalized.size()-L) normalized.resize(normalized.size()-L); };
    stripSuffix(".cst.load"); stripSuffix(".load"); stripSuffix(".cst.tmp"); stripSuffix(".tmp");
    if(normalized != nm){ if(auto aliased2 = load_from_aliased(S, normalized)) return aliased2; }
    // Slot already checked above.
    if(auto it = S.vmap.find(nm); it != S.vmap.end()) return it->second;
    return nullptr;
}

llvm::Value* eval_defined(builder::State& S, const std::string& name){
    auto it = S.defNode.find(name); if(it==S.defNode.end()) return nullptr;
    auto dn = it->second; if(!dn || !std::holds_alternative<edn::list>(dn->data)) return nullptr;
    auto &dl = std::get<edn::list>(dn->data).elems; if(dl.empty() || !std::holds_alternative<edn::symbol>(dl[0]->data)) return nullptr;
    std::string dop = std::get<edn::symbol>(dl[0]->data).name;
    auto val_of_resolved = [&](size_t idx)->llvm::Value*{
        if(idx>=dl.size()) return nullptr;
    return get_value(S, dl[idx]);
    };
    auto emit_cmp = [&](llvm::CmpInst::Predicate P)->llvm::Value*{ auto *va = val_of_resolved(3); auto *vb = val_of_resolved(4); if(!va||!vb) return nullptr; return S.builder.CreateICmp(P, va, vb, name+".re"); };
    if((dop=="eq"||dop=="ne"||dop=="lt"||dop=="gt"||dop=="le"||dop=="ge") && dl.size()==5){
        if(dop=="eq") return emit_cmp(llvm::CmpInst::ICMP_EQ);
        if(dop=="ne") return emit_cmp(llvm::CmpInst::ICMP_NE);
        if(dop=="lt") return emit_cmp(llvm::CmpInst::ICMP_SLT);
        if(dop=="gt") return emit_cmp(llvm::CmpInst::ICMP_SGT);
        if(dop=="le") return emit_cmp(llvm::CmpInst::ICMP_SLE);
        return emit_cmp(llvm::CmpInst::ICMP_SGE);
    }
    if((dop=="and"||dop=="or"||dop=="xor") && dl.size()==5){ auto *va = val_of_resolved(3); auto *vb = val_of_resolved(4); if(!va||!vb) return nullptr; if(dop=="and") return S.builder.CreateAnd(va,vb,name+".re"); if(dop=="or") return S.builder.CreateOr(va,vb,name+".re"); return S.builder.CreateXor(va,vb,name+".re"); }
    return nullptr;
}

} // namespace edn::ir::resolver
