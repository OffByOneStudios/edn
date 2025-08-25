#include "edn/ir/variable_ops.hpp"
#include "edn/ir/types.hpp"
#include "edn/ir/debug.hpp"
#include "edn/ir/di.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>

namespace edn::ir::variable_ops {

static std::string symName(const edn::node_ptr &n){ if(!n) return {}; if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name; if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; }
static std::string trimPct(const std::string& s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }

bool handle_as(builder::State& S, const std::vector<edn::node_ptr>& il,
               std::function<llvm::AllocaInst*(const std::string&, edn::TypeId, bool)> ensureSlot,
               std::unordered_map<std::string,std::string>& initAlias,
               bool enableDebugInfo,
               llvm::Function* F,
               std::shared_ptr<edn::ir::debug::DebugManager> dbgMgr){
    if(il.size()!=4 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    if(std::get<edn::symbol>(il[0]->data).name!="as") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId ty; try{ ty = S.tctx.parse_type(il[2]); }catch(...){ return false; }
    llvm::Type* lty = S.map_type(ty);
    llvm::Value* initV = nullptr;
    if(std::holds_alternative<edn::symbol>(il[3]->data)){
        std::string initName = trimPct(symName(il[3]));
        if(!initName.empty()){
            if(auto it = S.vmap.find(initName); it!=S.vmap.end()) initV = it->second;
        }
    } else if(std::holds_alternative<int64_t>(il[3]->data)){
        initV = llvm::ConstantInt::get(lty, (uint64_t)std::get<int64_t>(il[3]->data), true);
    } else if(std::holds_alternative<double>(il[3]->data)){
        initV = llvm::ConstantFP::get(lty, std::get<double>(il[3]->data));
    }
    if(!initV) initV = llvm::UndefValue::get(lty);
    auto *slot = ensureSlot(dst, ty, /*initFromCurrent=*/false);
    S.builder.CreateStore(initV, slot);
    S.vtypes[dst]=ty;
    auto *loaded = S.builder.CreateLoad(S.map_type(ty), slot, dst);
    S.vmap[dst]=loaded;
    if(std::holds_alternative<edn::symbol>(il[3]->data)){
        std::string initName = trimPct(symName(il[3]));
        if(!initName.empty()){
            initAlias[initName]=dst; S.vmap[initName]=loaded; S.vtypes[initName]=ty;
        }
    }
    if(enableDebugInfo && F && dbgMgr){
        // Use centralized helper; future logic (declare vs value) lives there
        di::declare_local(*dbgMgr, S.builder, slot, dst, ty,
                          S.builder.getCurrentDebugLocation()? S.builder.getCurrentDebugLocation()->getLine() : (F->getSubprogram()? F->getSubprogram()->getLine() : 1));
    }
    return true;
}

} // namespace edn::ir::variable_ops
