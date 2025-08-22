#include "edn/ir/phi_ops.hpp"
#include <llvm/IR/IRBuilder.h>

namespace edn::ir::phi_ops {

static std::string symName(const edn::node_ptr &n){ if(!n) return {}; if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name; if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; }
static std::string trimPct(const std::string& s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }

bool collect(builder::State& S,
             const std::vector<edn::node_ptr>& il,
             std::vector<PendingPhi>& outPending){
    if(il.size()!=4 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    if(std::get<edn::symbol>(il[0]->data).name!="phi") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId ty; try { ty = S.tctx.parse_type(il[2]); } catch(...) { return false; }
    if(!il[3] || !std::holds_alternative<edn::vector_t>(il[3]->data)) return false;
    std::vector<std::pair<std::string,std::string>> incomings;
    for(auto &inc : std::get<edn::vector_t>(il[3]->data).elems){
        if(!inc || !std::holds_alternative<edn::list>(inc->data)) continue;
        auto &pl = std::get<edn::list>(inc->data).elems;
        if(pl.size()!=2) continue;
        std::string val = trimPct(symName(pl[0]));
        std::string label = symName(pl[1]);
        if(!val.empty() && !label.empty()) incomings.emplace_back(val,label);
    }
    outPending.push_back(PendingPhi{dst, ty, std::move(incomings), S.builder.GetInsertBlock()});
    return true;
}

void finalize(builder::State& S,
              std::vector<PendingPhi>& pending,
              llvm::Function* F,
              std::function<llvm::Type*(edn::TypeId)> map_type){
    for(auto &pp : pending){
        if(!pp.insertBlock) continue;
        llvm::IRBuilder<> tmpBuilder(pp.insertBlock, pp.insertBlock->begin());
        llvm::PHINode* phi = tmpBuilder.CreatePHI(map_type(pp.ty), (unsigned)pp.incomings.size(), pp.dst);
        S.vmap[pp.dst] = phi;
        S.vtypes[pp.dst] = pp.ty;
        for(auto &inc : pp.incomings){
            auto itv = S.vmap.find(inc.first);
            if(itv == S.vmap.end()) continue;
            llvm::BasicBlock* pred = nullptr;
            for(auto &bb : *F){ if(bb.getName() == inc.second){ pred=&bb; break; } }
            if(pred) phi->addIncoming(itv->second, pred);
        }
    }
    pending.clear();
}

} // namespace edn::ir::phi_ops
