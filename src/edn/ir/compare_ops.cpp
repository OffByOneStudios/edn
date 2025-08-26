#include "edn/ir/compare_ops.hpp"
// Forward declare resolver to mitigate intermittent header/PCH stale issues.
namespace edn { namespace ir { namespace resolver { llvm::Value* get_value(builder::State&, const edn::node_ptr&); } } }
#include "edn/ir/resolver.hpp" // added for resolver::get_value
#include <llvm/IR/IRBuilder.h>

namespace edn::ir::compare_ops {

static std::string symName(const edn::node_ptr &n){
    if(!n) return {};
    if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name;
    if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data);
    return {};
}
static std::string trimPct(const std::string& s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }

bool handle_int_simple(builder::State& S, const std::vector<edn::node_ptr>& il, const edn::node_ptr& inst){
    if(il.size()!=5 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    const std::string op = std::get<edn::symbol>(il[0]->data).name;
    if(op!="eq" && op!="ne" && op!="lt" && op!="gt" && op!="le" && op!="ge") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    auto *va = edn::ir::resolver::get_value(S, il[3]);
    auto *vb = edn::ir::resolver::get_value(S, il[4]);
    if(!va||!vb) return false;
    llvm::CmpInst::Predicate P = llvm::CmpInst::ICMP_EQ;
    if(op=="eq") P = llvm::CmpInst::ICMP_EQ; else if(op=="ne") P=llvm::CmpInst::ICMP_NE; else if(op=="lt") P=llvm::CmpInst::ICMP_SLT; else if(op=="gt") P=llvm::CmpInst::ICMP_SGT; else if(op=="le") P=llvm::CmpInst::ICMP_SLE; else P=llvm::CmpInst::ICMP_SGE;
    auto *res = S.builder.CreateICmp(P, va, vb, dst);
    S.vmap[dst]=res; S.vtypes[dst]=S.tctx.get_base(BaseType::I1); S.defNode[dst]=inst; return true;
}

bool handle_icmp(builder::State& S, const std::vector<edn::node_ptr>& il){
    if(il.size()!=7 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    if(std::get<edn::symbol>(il[0]->data).name!="icmp") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    if(!il[3] || !std::holds_alternative<edn::keyword>(il[3]->data)) return false;
    std::string pred = symName(il[4]);
    auto *va = edn::ir::resolver::get_value(S, il[5]);
    auto *vb = edn::ir::resolver::get_value(S, il[6]);
    if(!va||!vb) return false;
    llvm::CmpInst::Predicate P = llvm::CmpInst::ICMP_EQ;
    if(pred=="eq") P=llvm::CmpInst::ICMP_EQ; else if(pred=="ne") P=llvm::CmpInst::ICMP_NE; else if(pred=="slt") P=llvm::CmpInst::ICMP_SLT; else if(pred=="sgt") P=llvm::CmpInst::ICMP_SGT; else if(pred=="sle") P=llvm::CmpInst::ICMP_SLE; else if(pred=="sge") P=llvm::CmpInst::ICMP_SGE; else if(pred=="ult") P=llvm::CmpInst::ICMP_ULT; else if(pred=="ugt") P=llvm::CmpInst::ICMP_UGT; else if(pred=="ule") P=llvm::CmpInst::ICMP_ULE; else if(pred=="uge") P=llvm::CmpInst::ICMP_UGE; else return false;
    auto *res = S.builder.CreateICmp(P, va, vb, dst);
    S.vmap[dst]=res; S.vtypes[dst]=S.tctx.get_base(BaseType::I1); return true;
}

bool handle_fcmp(builder::State& S, const std::vector<edn::node_ptr>& il){
    if(il.size()!=7 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    if(std::get<edn::symbol>(il[0]->data).name!="fcmp") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    if(!il[3] || !std::holds_alternative<edn::keyword>(il[3]->data)) return false;
    std::string pred = symName(il[4]);
    auto *va = edn::ir::resolver::get_value(S, il[5]);
    auto *vb = edn::ir::resolver::get_value(S, il[6]);
    if(!va||!vb) return false;
    llvm::CmpInst::Predicate P = llvm::CmpInst::FCMP_OEQ;
    if(pred=="oeq") P=llvm::CmpInst::FCMP_OEQ; else if(pred=="one") P=llvm::CmpInst::FCMP_ONE; else if(pred=="olt") P=llvm::CmpInst::FCMP_OLT; else if(pred=="ogt") P=llvm::CmpInst::FCMP_OGT; else if(pred=="ole") P=llvm::CmpInst::FCMP_OLE; else if(pred=="oge") P=llvm::CmpInst::FCMP_OGE; else if(pred=="ord") P=llvm::CmpInst::FCMP_ORD; else if(pred=="uno") P=llvm::CmpInst::FCMP_UNO; else if(pred=="ueq") P=llvm::CmpInst::FCMP_UEQ; else if(pred=="une") P=llvm::CmpInst::FCMP_UNE; else if(pred=="ult") P=llvm::CmpInst::FCMP_ULT; else if(pred=="ugt") P=llvm::CmpInst::FCMP_UGT; else if(pred=="ule") P=llvm::CmpInst::FCMP_ULE; else if(pred=="uge") P=llvm::CmpInst::FCMP_UGE; else return false;
    auto *res = S.builder.CreateFCmp(P, va, vb, dst);
    S.vmap[dst]=res; S.vtypes[dst]=S.tctx.get_base(BaseType::I1); return true;
}

} // namespace edn::ir::compare_ops
