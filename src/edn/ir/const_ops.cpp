#include "edn/ir/const_ops.hpp"
#include "edn/ir/types.hpp"
#include <llvm/IR/Constants.h>

namespace edn::ir::const_ops {

static std::string symName(const edn::node_ptr &n){ if(!n) return {}; if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name; if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; }
static std::string trimPct(const std::string& s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }

bool handle(builder::State& S, const std::vector<edn::node_ptr>& il){
    if(il.size()!=4 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    if(std::get<edn::symbol>(il[0]->data).name!="const") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId ty; try{ ty = S.tctx.parse_type(il[2]); }catch(...){ return false; }
    llvm::Type* lty = S.map_type(ty);
    llvm::Value* cv=nullptr;
    if(std::holds_alternative<int64_t>(il[3]->data)) {
        auto val = (uint64_t)std::get<int64_t>(il[3]->data);
        if(auto *ity = llvm::dyn_cast<llvm::IntegerType>(lty)) {
            cv = llvm::ConstantInt::get(ity, val, true);
        } else if(lty->isPointerTy() && val==0) {
            cv = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(lty));
        } else if(val==0) {
            // Use generic null value for aggregates / other zero-initializable types
            cv = llvm::Constant::getNullValue(lty);
        } else {
            // Fallback: reject mismatched integer literal for non-integer type; leave undef
            fprintf(stderr, "[warn][const_ops] integer literal applied to non-integer type kind=%u; producing undef for now.\n", (unsigned)lty->getTypeID());
        }
    }
    else if(std::holds_alternative<double>(il[3]->data)) cv = llvm::ConstantFP::get(lty, std::get<double>(il[3]->data));
    if(!cv) cv = llvm::UndefValue::get(lty);
    S.vmap[dst]=cv; S.vtypes[dst]=ty; return true;
}

} // namespace edn::ir::const_ops
