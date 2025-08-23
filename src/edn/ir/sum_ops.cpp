#include "edn/ir/sum_ops.hpp"
#include "edn/ir/types.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

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
    auto *ST = llvm::StructType::getTypeByName(S.llctx, "struct."+sname); if(!ST) return false; 
    // Instrument: log struct element LLVM type IDs for diagnostics
    if(ST && ST->isSized()){
        fprintf(stderr, "[dbg][sum_new] struct %s elem_count=%u\n", sname.c_str(), (unsigned)ST->getNumContainedTypes());
        for(unsigned ei=0; ei<ST->getNumContainedTypes(); ++ei){ auto *et = ST->getElementType(ei); if(et){ fprintf(stderr, "[dbg][sum_new]  elem %u typeid=%u\n", ei, (unsigned)et->getTypeID()); } }
    }
    auto *allocaPtr = S.builder.CreateAlloca(ST, nullptr, dst);
    llvm::Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx), 0); llvm::Value *tagIdx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); 
    fprintf(stderr, "[dbg][sum_new] creating tag GEP for %s (ST=%p)\n", sname.c_str(), (void*)ST);
    auto *tagPtr = S.builder.CreateInBoundsGEP(ST, allocaPtr, {zero, tagIdx}, dst+".tag.addr"); S.builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),(uint64_t)tag,true), tagPtr);
    llvm::Value *payIdx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),1); 
    fprintf(stderr, "[dbg][sum_new] creating payload GEP for %s (ST=%p)\n", sname.c_str(), (void*)ST);
    auto *payloadPtr = S.builder.CreateInBoundsGEP(ST, allocaPtr, {zero, payIdx}, dst+".payload.addr"); auto *i8Ptr=llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(S.llctx)); auto *rawPayloadPtr = S.builder.CreateBitCast(payloadPtr, i8Ptr, dst+".raw");
    uint64_t offset=0; for(size_t i=0;i<vals.size() && i<variants[tag].size(); ++i){ llvm::Type *fty = S.map_type(variants[tag][i]); if(!fty){ fprintf(stderr, "[dbg][sum_new] null LLVM type for variant field %zu of %s.%s\n", i, sname.c_str(), vname.c_str()); return false; } uint64_t fsz = edn::ir::size_in_bytes(S.module, fty); fprintf(stderr, "[dbg][sum_new] field %zu size=%llu tyid=%u\n", i, (unsigned long long)fsz, (unsigned)fty->getTypeID()); auto *offVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(S.llctx), offset); auto *dstPtr = S.builder.CreateInBoundsGEP(llvm::Type::getInt8Ty(S.llctx), rawPayloadPtr, offVal, dst+".fld"+std::to_string(i)+".raw"); auto *typedPtr = S.builder.CreateBitCast(dstPtr, llvm::PointerType::getUnqual(fty), dst+".fld"+std::to_string(i)+".ptr"); S.builder.CreateStore(vals[i], typedPtr); offset += fsz; }
    S.vmap[dst]=allocaPtr; S.vtypes[dst]=S.tctx.get_pointer(S.tctx.get_struct(sname));
    // Emit debug info (parity with legacy in edn.cpp)
    if(S.debug_manager && S.debug_manager->enableDebugInfo && S.builder.GetInsertBlock()){
        if(auto *F = S.builder.GetInsertBlock()->getParent(); F && F->getSubprogram()){
            auto *lv = S.debug_manager->DIB->createAutoVariable(F->getSubprogram(), dst, S.debug_manager->DI_File,
                S.builder.getCurrentDebugLocation() ? S.builder.getCurrentDebugLocation()->getLine() : F->getSubprogram()->getLine(),
                S.debug_manager->diTypeOf(S.tctx.get_struct(sname)));
            auto *expr = S.debug_manager->DIB->createExpression();
            (void)S.debug_manager->DIB->insertDeclare(allocaPtr, lv, expr, S.builder.getCurrentDebugLocation(), S.builder.GetInsertBlock());
        }
    }
    // Env-gated immediate verification & IR dump after sum_new
    static bool verifyAfter = [](){ const char* v = std::getenv("EDN_VERIFY_AFTER_SUM_OPS"); return v && std::string(v)=="1"; }();
    static bool dumpAfter = [](){ const char* v = std::getenv("EDN_DUMP_IR_AFTER_SUM_OPS"); return v && std::string(v)=="1"; }();
    if((verifyAfter || dumpAfter) && S.builder.GetInsertBlock()){
        if(auto *F = S.builder.GetInsertBlock()->getParent()){
            // Only dump/verify if all existing basic blocks are properly terminated (otherwise IR is transiently invalid mid-emission)
            bool structurallyComplete = true;
            for(auto &BB : *F){ if(!BB.getTerminator()){ structurallyComplete = false; break; } }
            if(dumpAfter){ llvm::errs() << "[sum_ops][dump] After sum_new in function " << F->getName(); if(!structurallyComplete) llvm::errs() << " (incomplete: skipping verify)"; llvm::errs() << "\n"; F->print(llvm::errs()); llvm::errs() << "\n"; }
            if(verifyAfter && structurallyComplete){ if(llvm::verifyFunction(*F, &llvm::errs())) llvm::errs() << "[sum_ops][verify] FAIL after sum_new\n"; }
            else if(verifyAfter && !structurallyComplete){ llvm::errs() << "[sum_ops][verify] skip after sum_new (function incomplete)\n"; }
        }
    }
    // Diagnostic: optionally force structural completeness after sum_new by splitting block
    static bool earlyTerm = [](){ const char* v = std::getenv("EDN_EARLY_TERM_AFTER_SUM_NEW"); return v && std::string(v)=="1"; }();
    if(earlyTerm && S.builder.GetInsertBlock()){
        llvm::BasicBlock* curBB = S.builder.GetInsertBlock();
        if(!curBB->getTerminator()){
            if(auto *F = curBB->getParent()){
                auto *contBB = llvm::BasicBlock::Create(S.llctx, "cont.after.sum_new", F);
                llvm::IRBuilder<> tb(curBB);
                tb.CreateBr(contBB);
                S.builder.SetInsertPoint(contBB);
                fprintf(stderr, "[diag][sum_new] inserted branch to %s to keep IR structurally valid mid-emission\n", contBB->getName().str().c_str());
            }
        }
    }
    // Stronger diagnostic: optionally inject a provisional return immediately after sum_new
    static bool retAfter = [](){ const char* v = std::getenv("EDN_RET_AFTER_SUM_NEW"); return v && std::string(v)=="1"; }();
    if(retAfter && S.builder.GetInsertBlock()){
        if(auto *F = S.builder.GetInsertBlock()->getParent()){
            if(!F->getEntryBlock().getTerminator()){
                llvm::IRBuilder<> rb(&F->getEntryBlock());
                // Only add if truly no terminator anywhere (best-effort quick check)
                bool anyTerm=false; for(auto &BB : *F){ if(BB.getTerminator()){ anyTerm=true; break; } }
                if(!anyTerm){
                    if(F->getReturnType()->isVoidTy()) rb.CreateRetVoid();
                    else rb.CreateRet(llvm::Constant::getNullValue(F->getReturnType()));
                    // Continue building in a fresh unreachable block to avoid halting emission logic
                    auto *deadBB = llvm::BasicBlock::Create(S.llctx, "dead.after.provisional.ret", F);
                    S.builder.SetInsertPoint(deadBB);
                    fprintf(stderr, "[diag][sum_new] inserted provisional return in %s to avoid incomplete function state\n", F->getName().str().c_str());
                }
            }
        }
    }
    return true;
}

bool handle_sum_is(builder::State& S, const std::vector<edn::node_ptr>& il,
                   const std::unordered_map<std::string, std::unordered_map<std::string,int>>& sum_variant_tag){
    // (sum-is %dst SumName %val Variant)
    // TODO(debug-info): If we later want to expose the temporary comparison result to the debugger
    // with a stable name, we could emit a dbg.value here referencing the i1 result.
    if(il.size()!=5) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data)|| std::get<edn::symbol>(il[0]->data).name!="sum-is") return false;
    std::string dst=trimPct(symName(il[1])); std::string sname=symName(il[2]); std::string val=trimPct(symName(il[3])); std::string vname=symName(il[4]); if(dst.empty()||sname.empty()||val.empty()||vname.empty()) return false; if(!S.vmap.count(val)) return false; auto tIt = sum_variant_tag.find(sname); if(tIt==sum_variant_tag.end()) return false; auto vtIt = tIt->second.find(vname); if(vtIt==tIt->second.end()) return false; int tag=vtIt->second; auto *ST=llvm::StructType::getTypeByName(S.llctx, "struct."+sname); if(!ST) return false; llvm::Value *zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); llvm::Value *tagIdx=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); auto *tagPtr = S.builder.CreateInBoundsGEP(ST, S.vmap[val], {zero, tagIdx}, dst+".tag.addr"); auto *loaded = S.builder.CreateLoad(llvm::Type::getInt32Ty(S.llctx), tagPtr, dst+".tag"); auto *cmp = S.builder.CreateICmpEQ(loaded, llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),(uint64_t)tag,true), dst); S.vmap[dst]=cmp; S.vtypes[dst]=S.tctx.get_base(edn::BaseType::I1); return true;
}

bool handle_sum_get(builder::State& S, const std::vector<edn::node_ptr>& il,
                    const std::unordered_map<std::string, std::unordered_map<std::string,int>>& sum_variant_tag,
                    const std::unordered_map<std::string, std::vector<std::vector<edn::TypeId>>>& sum_variant_field_types){
    // (sum-get %dst SumName %val Variant <index>)
    // TODO(debug-info): Potentially attach a dbg.value for extracted field if named source mapping desired.
    if(il.size()!=6) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data)|| std::get<edn::symbol>(il[0]->data).name!="sum-get") return false;
    std::string dst=trimPct(symName(il[1])); std::string sname=symName(il[2]); std::string val=trimPct(symName(il[3])); std::string vname=symName(il[4]); if(dst.empty()||sname.empty()||val.empty()||vname.empty()) return false; if(!S.vmap.count(val)) return false; if(!std::holds_alternative<int64_t>(il[5]->data)) return false; int64_t idxLit=(int64_t)std::get<int64_t>(il[5]->data); if(idxLit<0) return false; size_t idx=(size_t)idxLit;
    auto vfieldsIt = sum_variant_field_types.find(sname); auto vtagIt = sum_variant_tag.find(sname); if(vfieldsIt==sum_variant_field_types.end()||vtagIt==sum_variant_tag.end()) return false; auto vtIt=vtagIt->second.find(vname); if(vtIt==vtagIt->second.end()) return false; int tag=vtIt->second; auto &variants=vfieldsIt->second; if(tag<0||(size_t)tag>=variants.size()) return false; auto &fields=variants[tag]; if(idx>=fields.size()) return false; edn::TypeId fieldTyId=fields[idx]; auto *ST=llvm::StructType::getTypeByName(S.llctx, "struct."+sname); if(!ST) return false;
    llvm::Value *zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); llvm::Value *payIdx=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),1); 
    fprintf(stderr, "[dbg][sum_get] creating payload GEP for %s (ST=%p)\n", sname.c_str(), (void*)ST);
    auto *payloadPtr=S.builder.CreateInBoundsGEP(ST, S.vmap[val], {zero, payIdx}, dst+".payload.addr"); auto *i8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(S.llctx)); auto *rawPayloadPtr = S.builder.CreateBitCast(payloadPtr, i8PtrTy, dst+".raw"); uint64_t offset=0; for(size_t i=0;i<idx && i<fields.size(); ++i){ llvm::Type *fty = S.map_type(fields[i]); offset += edn::ir::size_in_bytes(S.module, fty);} llvm::Value *offVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(S.llctx), offset); llvm::Value *fieldRaw = S.builder.CreateInBoundsGEP(llvm::Type::getInt8Ty(S.llctx), rawPayloadPtr, offVal, dst+".fld.raw"); llvm::Type *fieldLL = S.map_type(fieldTyId); auto *fieldPtrTy = llvm::PointerType::getUnqual(fieldLL); auto *typedPtr = S.builder.CreateBitCast(fieldRaw, fieldPtrTy, dst+".fld.ptr"); auto *lv = S.builder.CreateLoad(fieldLL, typedPtr, dst); S.vmap[dst]=lv; S.vtypes[dst]=fieldTyId; 
    static bool verifyAfter = [](){ const char* v = std::getenv("EDN_VERIFY_AFTER_SUM_OPS"); return v && std::string(v)=="1"; }();
    static bool dumpAfter = [](){ const char* v = std::getenv("EDN_DUMP_IR_AFTER_SUM_OPS"); return v && std::string(v)=="1"; }();
    if((verifyAfter || dumpAfter) && S.builder.GetInsertBlock()){
        if(auto *F = S.builder.GetInsertBlock()->getParent()){
            bool structurallyComplete = true; for(auto &BB : *F){ if(!BB.getTerminator()){ structurallyComplete=false; break; } }
            if(dumpAfter){ llvm::errs() << "[sum_ops][dump] After sum_get in function " << F->getName(); if(!structurallyComplete) llvm::errs() << " (incomplete: skipping verify)"; llvm::errs() << "\n"; F->print(llvm::errs()); llvm::errs() << "\n"; }
            if(verifyAfter && structurallyComplete){ if(llvm::verifyFunction(*F, &llvm::errs())) llvm::errs() << "[sum_ops][verify] FAIL after sum_get\n"; }
            else if(verifyAfter && !structurallyComplete){ llvm::errs() << "[sum_ops][verify] skip after sum_get (function incomplete)\n"; }
        }
    }
    return true; }

} // namespace edn::ir::sum_ops
