#include "edn/ir/return_ops.hpp"

using namespace edn;

namespace edn::ir::return_ops {

static std::string trimPct(const std::string& s){ if(!s.empty() && s[0]=='%') return s.substr(1); return s; }
static std::string symName(const edn::node_ptr& n){ if(!n) return {}; if(std::holds_alternative<symbol>(n->data)) return std::get<symbol>(n->data).name; return {}; }

bool handle(Context C, const std::vector<edn::node_ptr>& il){
    if(il.empty() || !il[0] || !std::holds_alternative<symbol>(il[0]->data)) return false;
    if(std::get<symbol>(il[0]->data).name != "ret" || il.size()!=3) return false;
    auto& B = C.S.builder; auto& varSlots = C.S.varSlots; auto& vtypes = C.S.vtypes; auto& vmap = C.S.vmap; auto map_type = C.S.map_type;
    std::string rv = trimPct(symName(il[2])); if(!rv.empty()){
        auto sIt = varSlots.find(rv);
        if(sIt != varSlots.end()){
            auto tIt = vtypes.find(rv); if(tIt != vtypes.end()){
                auto *lv = B.CreateLoad(map_type(tIt->second), sIt->second, rv + ".ret");
                B.CreateRet(lv); C.functionDone = true; return true; }
        }
        if(vmap.count(rv)) { B.CreateRet(vmap[rv]); C.functionDone = true; return true; }
    }
    if(C.fty->getReturnType()->isVoidTy()) B.CreateRetVoid(); else B.CreateRet(llvm::Constant::getNullValue(C.fty->getReturnType())); C.functionDone = true; return true;
}

} // namespace edn::ir::return_ops
