#include "edn/ir/cast_ops.hpp"
#include "edn/ir/types.hpp"
// Forward declare resolver namespace in case header visibility issues cause
// the compiler to miss it (workaround for intermittent namespace lookup issue).
namespace edn { namespace ir { namespace resolver { llvm::Value* get_value(builder::State&, const edn::node_ptr&); } } }
#include "edn/ir/resolver.hpp" // ensure real declarations/definitions

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>

namespace edn::ir::cast_ops {

static std::string symName(const edn::node_ptr &n){
    if(!n) return {}; if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name; if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; }
static std::string trimPct(const std::string& s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }

bool handle(builder::State& S, const std::vector<edn::node_ptr>& il){
    if(il.size()!=4 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    const std::string op = std::get<edn::symbol>(il[0]->data).name;
    if(op!="zext" && op!="sext" && op!="trunc" && op!="bitcast" && op!="sitofp" && op!="uitofp" && op!="fptosi" && op!="fptoui" && op!="ptrtoint" && op!="inttoptr") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId toTy; try{ toTy = S.tctx.parse_type(il[2]); }catch(...){ return false; }
    auto *srcV = edn::ir::resolver::get_value(S, il[3]); if(!srcV) return false;
    llvm::Type* llvmTo = S.map_type(toTy);
    // If constant, load via temporary alloca to avoid constant folding corner cases that legacy path handled.
    if(llvm::isa<llvm::Constant>(srcV)){
        auto *tmpAlloca = S.builder.CreateAlloca(srcV->getType(), nullptr, trimPct(symName(il[3]))+".cst.tmp");
        S.builder.CreateStore(srcV, tmpAlloca);
        srcV = S.builder.CreateLoad(srcV->getType(), tmpAlloca, trimPct(symName(il[3]))+".cst.load");
    }
    llvm::Value* castV=nullptr;
    if(op=="zext") castV = S.builder.CreateZExt(srcV, llvmTo, dst);
    else if(op=="sext") castV = S.builder.CreateSExt(srcV, llvmTo, dst);
    else if(op=="trunc") castV = S.builder.CreateTrunc(srcV, llvmTo, dst);
    else if(op=="bitcast") castV = S.builder.CreateBitCast(srcV, llvmTo, dst);
    else if(op=="sitofp") castV = S.builder.CreateSIToFP(srcV, llvmTo, dst);
    else if(op=="uitofp") castV = S.builder.CreateUIToFP(srcV, llvmTo, dst);
    else if(op=="fptosi") castV = S.builder.CreateFPToSI(srcV, llvmTo, dst);
    else if(op=="fptoui") castV = S.builder.CreateFPToUI(srcV, llvmTo, dst);
    else if(op=="ptrtoint") castV = S.builder.CreatePtrToInt(srcV, llvmTo, dst);
    else if(op=="inttoptr") castV = S.builder.CreateIntToPtr(srcV, llvmTo, dst);
    if(!castV) return false;
    S.vmap[dst]=castV; S.vtypes[dst]=toTy; return true;
}

} // namespace edn::ir::cast_ops
