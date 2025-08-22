#include "edn/ir/sum_ops.hpp"
#include "edn/ir/types.hpp"
#include <llvm/IR/IRBuilder.h>

namespace edn::ir::sum_ops {

static std::string symName(const edn::node_ptr &n){ if(!n) return {}; if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name; if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; }
static std::string trimPct(const std::string &s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }

bool handle_sum_new(builder::State& S, const std::vector<edn::node_ptr>& il,
                    const std::unordered_map<std::string, std::unordered_map<std::string,int>>& sum_variant_tag,
                    const std::unordered_map<std::string, std::vector<std::vector<edn::TypeId>>>& sum_variant_field_types){
    // (sum-new %dst SumName Variant [ %v* ])
    if(!(il.size()==4 || il.size()==5)) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="sum-new") return false;
    std::string dst = trimPct(symName(il[1])); std::string sname = symName(il[2]); std::string vname = symName(il[3]); if(dst.empty()||sname.empty()||vname.empty()) return false;
    auto vtagMapIt = sum_variant_tag.find(sname); auto vfieldsIt = sum_variant_field_types.find(sname); if(vtagMapIt==sum_variant_tag.end()||vfieldsIt==sum_variant_field_types.end()) return false; auto tIt = vtagMapIt->second.find(vname); if(tIt==vtagMapIt->second.end()) return false; int tag=tIt->second; auto &variants=vfieldsIt->second; if(tag<0 || (size_t)tag>=variants.size()) return false;
    std::vector<llvm::Value*> vals; if(il.size()==5 && std::holds_alternative<edn::vector_t>(il[4]->data)){ for(auto &nv: std::get<edn::vector_t>(il[4]->data).elems){ std::string vn=trimPct(symName(nv)); if(vn.empty()||!S.vmap.count(vn)){ vals.clear(); break; } vals.push_back(S.vmap[vn]); } }
    auto *ST = llvm::StructType::getTypeByName(S.llctx, "struct."+sname); if(!ST) return false; auto *allocaPtr = S.builder.CreateAlloca(ST, nullptr, dst);
    llvm::Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx), 0); llvm::Value *tagIdx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); auto *tagPtr = S.builder.CreateInBoundsGEP(ST, allocaPtr, {zero, tagIdx}, dst+".tag.addr"); S.builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),(uint64_t)tag,true), tagPtr);
    llvm::Value *payIdx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),1); auto *payloadPtr = S.builder.CreateInBoundsGEP(ST, allocaPtr, {zero, payIdx}, dst+".payload.addr"); auto *i8Ptr=llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(S.llctx)); auto *rawPayloadPtr = S.builder.CreateBitCast(payloadPtr, i8Ptr, dst+".raw");
    uint64_t offset=0; for(size_t i=0;i<vals.size() && i<variants[tag].size(); ++i){ llvm::Type *fty = S.map_type(variants[tag][i]); uint64_t fsz = edn::ir::size_in_bytes(S.module, fty); auto *offVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(S.llctx), offset); auto *dstPtr = S.builder.CreateInBoundsGEP(llvm::Type::getInt8Ty(S.llctx), rawPayloadPtr, offVal, dst+".fld"+std::to_string(i)+".raw"); auto *typedPtr = S.builder.CreateBitCast(dstPtr, llvm::PointerType::getUnqual(fty), dst+".fld"+std::to_string(i)+".ptr"); S.builder.CreateStore(vals[i], typedPtr); offset += fsz; }
    S.vmap[dst]=allocaPtr; S.vtypes[dst]=S.tctx.get_pointer(S.tctx.get_struct(sname)); return true;
}

bool handle_sum_is(builder::State& S, const std::vector<edn::node_ptr>& il,
                   const std::unordered_map<std::string, std::unordered_map<std::string,int>>& sum_variant_tag){
    // (sum-is %dst SumName %val Variant)
    if(il.size()!=5) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data)|| std::get<edn::symbol>(il[0]->data).name!="sum-is") return false;
    std::string dst=trimPct(symName(il[1])); std::string sname=symName(il[2]); std::string val=trimPct(symName(il[3])); std::string vname=symName(il[4]); if(dst.empty()||sname.empty()||val.empty()||vname.empty()) return false; if(!S.vmap.count(val)) return false; auto tIt = sum_variant_tag.find(sname); if(tIt==sum_variant_tag.end()) return false; auto vtIt = tIt->second.find(vname); if(vtIt==tIt->second.end()) return false; int tag=vtIt->second; auto *ST=llvm::StructType::getTypeByName(S.llctx, "struct."+sname); if(!ST) return false; llvm::Value *zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); llvm::Value *tagIdx=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); auto *tagPtr = S.builder.CreateInBoundsGEP(ST, S.vmap[val], {zero, tagIdx}, dst+".tag.addr"); auto *loaded = S.builder.CreateLoad(llvm::Type::getInt32Ty(S.llctx), tagPtr, dst+".tag"); auto *cmp = S.builder.CreateICmpEQ(loaded, llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),(uint64_t)tag,true), dst); S.vmap[dst]=cmp; S.vtypes[dst]=S.tctx.get_base(edn::BaseType::I1); return true;
}

bool handle_sum_get(builder::State& S, const std::vector<edn::node_ptr>& il,
                    const std::unordered_map<std::string, std::unordered_map<std::string,int>>& sum_variant_tag,
                    const std::unordered_map<std::string, std::vector<std::vector<edn::TypeId>>>& sum_variant_field_types){
    // (sum-get %dst SumName %val Variant <index>)
    if(il.size()!=6) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data)|| std::get<edn::symbol>(il[0]->data).name!="sum-get") return false;
    std::string dst=trimPct(symName(il[1])); std::string sname=symName(il[2]); std::string val=trimPct(symName(il[3])); std::string vname=symName(il[4]); if(dst.empty()||sname.empty()||val.empty()||vname.empty()) return false; if(!S.vmap.count(val)) return false; if(!std::holds_alternative<int64_t>(il[5]->data)) return false; int64_t idxLit=(int64_t)std::get<int64_t>(il[5]->data); if(idxLit<0) return false; size_t idx=(size_t)idxLit;
    auto vfieldsIt = sum_variant_field_types.find(sname); auto vtagIt = sum_variant_tag.find(sname); if(vfieldsIt==sum_variant_field_types.end()||vtagIt==sum_variant_tag.end()) return false; auto vtIt=vtagIt->second.find(vname); if(vtIt==vtagIt->second.end()) return false; int tag=vtIt->second; auto &variants=vfieldsIt->second; if(tag<0||(size_t)tag>=variants.size()) return false; auto &fields=variants[tag]; if(idx>=fields.size()) return false; edn::TypeId fieldTyId=fields[idx]; auto *ST=llvm::StructType::getTypeByName(S.llctx, "struct."+sname); if(!ST) return false;
    llvm::Value *zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); llvm::Value *payIdx=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),1); auto *payloadPtr=S.builder.CreateInBoundsGEP(ST, S.vmap[val], {zero, payIdx}, dst+".payload.addr"); auto *i8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(S.llctx)); auto *rawPayloadPtr = S.builder.CreateBitCast(payloadPtr, i8PtrTy, dst+".raw"); uint64_t offset=0; for(size_t i=0;i<idx && i<fields.size(); ++i){ llvm::Type *fty = S.map_type(fields[i]); offset += edn::ir::size_in_bytes(S.module, fty);} llvm::Value *offVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(S.llctx), offset); llvm::Value *fieldRaw = S.builder.CreateInBoundsGEP(llvm::Type::getInt8Ty(S.llctx), rawPayloadPtr, offVal, dst+".fld.raw"); llvm::Type *fieldLL = S.map_type(fieldTyId); auto *fieldPtrTy = llvm::PointerType::getUnqual(fieldLL); auto *typedPtr = S.builder.CreateBitCast(fieldRaw, fieldPtrTy, dst+".fld.ptr"); auto *lv = S.builder.CreateLoad(fieldLL, typedPtr, dst); S.vmap[dst]=lv; S.vtypes[dst]=fieldTyId; return true; }

} // namespace edn::ir::sum_ops
