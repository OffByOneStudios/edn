// Rewritten builder.cpp (forced rewrite to purge stale references)
#include "edn/ir/builder.hpp"
#include "edn/ir/types.hpp"
#include <cstring>

namespace edn::ir::builder {

static std::string symName(const edn::node_ptr &n){
    if(!n) return {}; if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name;
    if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; }
static std::string trimPct(const std::string& s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }
static void strip_suffix(std::string& s, const char* suf){ size_t L=std::strlen(suf); if(s.size()>=L && s.rfind(suf)==s.size()-L) s.resize(s.size()-L); }

llvm::Value* get_value(State& S, const edn::node_ptr& n){
    std::string nm = trimPct(symName(n)); if(nm.empty()) return nullptr;
    auto loadFromAliasedVar = [&](const std::string& key)->llvm::Value*{
        auto aIt = S.initAlias.find(key); if(aIt==S.initAlias.end()) return (llvm::Value*)nullptr;
        const std::string& var = aIt->second;
        if(auto s2 = S.varSlots.find(var); s2!=S.varSlots.end()){
            auto t2 = S.vtypes.find(var); if(t2==S.vtypes.end()) return (llvm::Value*)nullptr;
            return S.builder.CreateLoad(S.map_type(t2->second), s2->second, var);
        }
        if(auto vv = S.vmap.find(var); vv!=S.vmap.end()) return vv->second; return (llvm::Value*)nullptr; };
    if(auto* aliased = loadFromAliasedVar(nm)) return aliased;
    std::string normalized = nm; strip_suffix(normalized, ".cst.load"); strip_suffix(normalized, ".load"); strip_suffix(normalized, ".cst.tmp"); strip_suffix(normalized, ".tmp");
    if(normalized!=nm){ if(auto* aliased2 = loadFromAliasedVar(normalized)) return aliased2; }
    if(auto sIt = S.varSlots.find(nm); sIt!=S.varSlots.end()){
        auto tIt = S.vtypes.find(nm); if(tIt==S.vtypes.end()) return nullptr; return S.builder.CreateLoad(S.map_type(tIt->second), sIt->second, nm); }
    auto it = S.vmap.find(nm); return (it!=S.vmap.end()) ? it->second : nullptr;
}

llvm::Value* resolve_preferring_slots(State& S, const edn::node_ptr& n){
    std::string nm = trimPct(symName(n)); if(nm.empty()) return get_value(S,n);
    auto pickSlot = [&](const std::string& var)->llvm::Value*{ auto sIt=S.varSlots.find(var); if(sIt!=S.varSlots.end()){ auto tIt=S.vtypes.find(var); if(tIt==S.vtypes.end()) return (llvm::Value*)nullptr; return S.builder.CreateLoad(S.map_type(tIt->second), sIt->second, var);} return (llvm::Value*)nullptr; };
    if(auto aIt=S.initAlias.find(nm); aIt!=S.initAlias.end()) if(auto* lv=pickSlot(aIt->second)) return lv;
    std::string normalized = nm; strip_suffix(normalized, ".cst.load"); strip_suffix(normalized, ".load"); strip_suffix(normalized, ".cst.tmp"); strip_suffix(normalized, ".tmp");
    if(normalized!=nm){ if(auto aIt2=S.initAlias.find(normalized); aIt2!=S.initAlias.end()) if(auto* lv=pickSlot(aIt2->second)) return lv; }
    if(auto* lv=pickSlot(nm)) return lv; return get_value(S,n);
}

llvm::Value* eval_defined(State& S, const std::string& name){
    auto it = S.defNode.find(name); if(it==S.defNode.end()) return nullptr; auto dn=it->second;
    if(!dn || !std::holds_alternative<edn::list>(dn->data)) return nullptr; auto& dl=std::get<edn::list>(dn->data).elems;
    if(dl.empty() || !std::holds_alternative<edn::symbol>(dl[0]->data)) return nullptr; std::string dop=std::get<edn::symbol>(dl[0]->data).name;
    auto valOf = [&](size_t idx)->llvm::Value*{ if(idx>=dl.size()) return nullptr; return get_value(S, dl[idx]); };
    if((dop=="eq"||dop=="ne"||dop=="lt"||dop=="gt"||dop=="le"||dop=="ge") && dl.size()==5){
        llvm::Value* va=valOf(3); llvm::Value* vb=valOf(4); if(!va||!vb) return nullptr; llvm::CmpInst::Predicate P=llvm::CmpInst::ICMP_EQ;
        if(dop=="ne") P=llvm::CmpInst::ICMP_NE; else if(dop=="lt") P=llvm::CmpInst::ICMP_SLT; else if(dop=="gt") P=llvm::CmpInst::ICMP_SGT; else if(dop=="le") P=llvm::CmpInst::ICMP_SLE; else if(dop=="ge") P=llvm::CmpInst::ICMP_SGE;
        return S.builder.CreateICmp(P, va, vb, name+".re"); }
    if((dop=="and"||dop=="or"||dop=="xor") && dl.size()==5){ llvm::Value* va=valOf(3); llvm::Value* vb=valOf(4); if(!va||!vb) return nullptr;
        if(dop=="and") return S.builder.CreateAnd(va,vb,name+".re"); if(dop=="or") return S.builder.CreateOr(va,vb,name+".re"); return S.builder.CreateXor(va,vb,name+".re"); }
    return nullptr; }

} // namespace edn::ir::builder
