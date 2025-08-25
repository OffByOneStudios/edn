// Slimmed builder.cpp: retain only resolve_preferring_slots; delegate core lookups to resolver.
#include "edn/ir/builder.hpp"
#include "edn/ir/types.hpp"
#include "edn/ir/resolver.hpp"
#include <cstring>

namespace edn::ir::builder {

static std::string symName(const edn::node_ptr &n){ if(!n) return {}; if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name; if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; }
static std::string trimPct(const std::string& s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }
static void strip_suffix(std::string& s, const char* suf){ size_t L=std::strlen(suf); if(s.size()>=L && s.rfind(suf)==s.size()-L) s.resize(s.size()-L); }

llvm::Value* resolve_preferring_slots(State& S, const edn::node_ptr& n){
    std::string nm = trimPct(symName(n)); if(nm.empty()) return resolver::get_value(S,n);
    auto pickSlot = [&](const std::string& var)->llvm::Value*{ auto sIt=S.varSlots.find(var); if(sIt!=S.varSlots.end()){ auto tIt=S.vtypes.find(var); if(tIt==S.vtypes.end()) return (llvm::Value*)nullptr; return S.builder.CreateLoad(S.map_type(tIt->second), sIt->second, var);} return (llvm::Value*)nullptr; };
    if(auto aIt=S.initAlias.find(nm); aIt!=S.initAlias.end()) if(auto* lv=pickSlot(aIt->second)) return lv;
    std::string normalized = nm; strip_suffix(normalized, ".cst.load"); strip_suffix(normalized, ".load"); strip_suffix(normalized, ".cst.tmp"); strip_suffix(normalized, ".tmp");
    if(normalized!=nm){ if(auto aIt2=S.initAlias.find(normalized); aIt2!=S.initAlias.end()) if(auto* lv=pickSlot(aIt2->second)) return lv; }
    if(auto* lv=pickSlot(nm)) return lv; return resolver::get_value(S,n);
}

} // namespace edn::ir::builder
