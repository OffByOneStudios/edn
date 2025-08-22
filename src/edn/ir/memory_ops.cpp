#include "edn/ir/memory_ops.hpp"
#include "edn/ir/types.hpp"
#include <llvm/IR/IRBuilder.h>

namespace edn::ir::memory_ops {

static std::string symName(const edn::node_ptr &n){ if(!n) return {}; if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name; if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; }
static std::string trimPct(const std::string &s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }

bool handle_assign(builder::State& S, const std::vector<edn::node_ptr>& il){
    // (assign %dst %src)
    if(il.size()!=3) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="assign") return false;
    std::string dst = trimPct(symName(il[1])); std::string src = trimPct(symName(il[2])); if(dst.empty()||src.empty()) return false;
    auto svIt = S.vmap.find(src); if(svIt==S.vmap.end()) return false; auto tyIt = S.vtypes.find(src); if(tyIt==S.vtypes.end()) return false; auto sty = tyIt->second;
    // ensure slot
    // replicate logic of ensureSlot inline: create alloca in function entry if missing
    if(!S.varSlots.count(dst)){
        // Find entry block (assume builder has insertion block within function)
        llvm::Function* F = S.builder.GetInsertBlock()->getParent();
        llvm::IRBuilder<> eb(&*F->getEntryBlock().getFirstInsertionPt());
        auto *slot = eb.CreateAlloca(S.map_type(sty), nullptr, dst+".slot");
        S.varSlots[dst] = slot;
        S.vtypes[dst] = sty; // type of stored value
    }
    auto *slot = S.varSlots[dst];
    S.builder.CreateStore(svIt->second, slot);
    // Keep SSA map in sync with a load
    auto *cur = S.builder.CreateLoad(S.map_type(sty), slot, dst);
    S.vmap[dst] = cur; S.vtypes[dst] = sty;
    return true;
}

bool handle_alloca(builder::State& S, const std::vector<edn::node_ptr>& il){
    // (alloca %dst <type>)
    if(il.size()!=3) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="alloca") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false; 
    edn::TypeId ty; try { ty = S.tctx.parse_type(il[2]); } catch(...) { return false; }
    auto *av = S.builder.CreateAlloca(S.map_type(ty), nullptr, dst);
    S.vmap[dst] = av; S.vtypes[dst] = S.tctx.get_pointer(ty);
    // Debug info (mirrors legacy inline implementation)
    if (S.debug_manager && S.debug_manager->enableDebugInfo) {
        llvm::Function *F = S.builder.GetInsertBlock() ? S.builder.GetInsertBlock()->getParent() : nullptr;
        if (F && F->getSubprogram()) {
            unsigned line = 0;
            llvm::DebugLoc curLoc = S.builder.getCurrentDebugLocation();
            if (curLoc) line = curLoc.getLine();
            if (line == 0) line = F->getSubprogram()->getLine();
            auto *lv = S.debug_manager->DIB->createAutoVariable(
                F->getSubprogram(),
                dst,
                S.debug_manager->DI_File,
                line,
                S.debug_manager->diTypeOf(ty)
            );
            auto *expr = S.debug_manager->DIB->createExpression();
            (void)S.debug_manager->DIB->insertDeclare(av, lv, expr, curLoc, S.builder.GetInsertBlock());
        }
    }
    return true;
}

bool handle_store(builder::State& S, const std::vector<edn::node_ptr>& il){
    // (store %ptr %val)
    if(il.size()!=4) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="store") return false;
    std::string ptrn = trimPct(symName(il[2])); std::string valn = trimPct(symName(il[3])); if(ptrn.empty()||valn.empty()) return false;
    auto pit = S.vmap.find(ptrn); auto vit = S.vmap.find(valn); if(pit==S.vmap.end()||vit==S.vmap.end()) return false;
    S.builder.CreateStore(vit->second, pit->second); return true;
}

bool handle_gload(builder::State& S, const std::vector<edn::node_ptr>& il){
    // (gload %dst <type> GlobalName)
    if(il.size()!=4) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="gload") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId ty; try { ty = S.tctx.parse_type(il[2]); } catch(...) { return false; }
    std::string gname = symName(il[3]); if(gname.empty()) return false;
    auto *gv = S.module.getGlobalVariable(gname); if(!gv) return false;
    auto *lv = S.builder.CreateLoad(S.map_type(ty), gv, dst);
    S.vmap[dst] = lv; S.vtypes[dst] = ty; return true;
}

bool handle_gstore(builder::State& S, const std::vector<edn::node_ptr>& il){
    // (gstore GlobalName %val)
    if(il.size()!=4) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="gstore") return false;
    std::string gname = symName(il[2]); std::string valn = trimPct(symName(il[3])); if(gname.empty()||valn.empty()) return false;
    auto *gv = S.module.getGlobalVariable(gname); if(!gv) return false;
    auto vit = S.vmap.find(valn); if(vit==S.vmap.end()) return false;
    S.builder.CreateStore(vit->second, gv); return true;
}

bool handle_load(builder::State& S, const std::vector<edn::node_ptr>& il){
    // (load %dst <type> %ptr)
    if(il.size()!=4) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="load") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId ty; try { ty = S.tctx.parse_type(il[2]); } catch(...) { return false; }
    std::string ptrn = trimPct(symName(il[3])); auto it = S.vmap.find(ptrn); if(it==S.vmap.end() || !S.vtypes.count(ptrn)) return false;
    edn::TypeId pty = S.vtypes[ptrn]; const auto &PT = S.tctx.at(pty); if(PT.kind!=edn::Type::Kind::Pointer || PT.pointee!=ty) return false;
    auto *lv = S.builder.CreateLoad(S.map_type(ty), it->second, dst); S.vmap[dst]=lv; S.vtypes[dst]=ty; return true;
}

bool handle_index(builder::State& S, const std::vector<edn::node_ptr>& il){
    // (index %dst <elem-ty> %base %idx)
    if(il.size()!=5) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="index") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId elemTy; try { elemTy = S.tctx.parse_type(il[2]); } catch(...) { return false; }
    auto *baseV = builder::get_value(S, il[3]); auto *idxV = builder::get_value(S, il[4]); if(!baseV || !idxV) return false;
    std::string baseName = trimPct(symName(il[3])); if(!S.vtypes.count(baseName)) return false;
    edn::TypeId baseTyId = S.vtypes[baseName]; const edn::Type* baseTy = &S.tctx.at(baseTyId); if(baseTy->kind!=edn::Type::Kind::Pointer) return false;
    const edn::Type* arrTy = &S.tctx.at(baseTy->pointee); if(arrTy->kind!=edn::Type::Kind::Array || arrTy->elem!=elemTy) return false;
    llvm::Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx), 0);
    auto *gep = S.builder.CreateInBoundsGEP(S.map_type(baseTy->pointee), baseV, {zero, idxV}, dst);
    S.vmap[dst] = gep; S.vtypes[dst] = S.tctx.get_pointer(elemTy); return true;
}

bool handle_array_lit(builder::State& S, const std::vector<edn::node_ptr>& il){
    // (array-lit %dst <elem-type> <size> [ %e0 ... ])
    // TODO(debug-info): Consider emitting DI metadata for synthetic array literal temporaries
    // similar to handle_alloca once array literals need debugger visibility.
    if(il.size()!=5) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="array-lit") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId elemTy; try { elemTy = S.tctx.parse_type(il[2]); } catch(...) { return false; }
    if(!std::holds_alternative<int64_t>(il[3]->data)) return false; uint64_t asz = (uint64_t)std::get<int64_t>(il[3]->data); if(asz==0) return false;
    if(!std::holds_alternative<edn::vector_t>(il[4]->data)) return false; auto &elems = std::get<edn::vector_t>(il[4]->data).elems; if(elems.size()!=asz) return false;
    edn::TypeId arrTy = S.tctx.get_array(elemTy, asz); auto *AT = llvm::cast<llvm::ArrayType>(S.map_type(arrTy));
    auto *allocaPtr = S.builder.CreateAlloca(AT, nullptr, dst);
    for(size_t i=0;i<elems.size();++i){ if(!std::holds_alternative<edn::symbol>(elems[i]->data)) continue; std::string val = trimPct(symName(elems[i])); if(val.empty()) continue; auto vit=S.vmap.find(val); if(vit==S.vmap.end()) continue; llvm::Value *zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); llvm::Value *idx=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),(uint32_t)i); auto *gep = S.builder.CreateInBoundsGEP(AT, allocaPtr, {zero, idx}, dst+".elem"+std::to_string(i)+".addr"); S.builder.CreateStore(vit->second, gep);}    
    S.vmap[dst]=allocaPtr; S.vtypes[dst]=S.tctx.get_pointer(arrTy); return true;
}

bool handle_struct_lit(builder::State& S, const std::vector<edn::node_ptr>& il,
                       const std::unordered_map<std::string, std::unordered_map<std::string, size_t>>& struct_field_index,
                       const std::unordered_map<std::string, std::vector<edn::TypeId>>& struct_field_types){
    // (struct-lit %dst StructName [ field1 %v1 ... ])
    // TODO(debug-info): Potentially attach debug info for struct literal stack allocations
    // if we later surface them as user-visible temporaries or named variables.
    if(il.size()!=4) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="struct-lit") return false;
    std::string dst = trimPct(symName(il[1])); std::string sname = symName(il[2]); if(dst.empty()||sname.empty()) return false;
    if(!std::holds_alternative<edn::vector_t>(il[3]->data)) return false;
    auto idxIt = struct_field_index.find(sname); auto ftIt = struct_field_types.find(sname); if(idxIt==struct_field_index.end()||ftIt==struct_field_types.end()) return false;
    auto *ST = llvm::StructType::getTypeByName(S.llctx, "struct."+sname);
    if(!ST){ std::vector<llvm::Type*> ftys; for(auto tid: ftIt->second) ftys.push_back(S.map_type(tid)); ST = llvm::StructType::create(S.llctx, ftys, "struct."+sname); }
    auto *allocaPtr = S.builder.CreateAlloca(ST, nullptr, dst);
    auto &vec = std::get<edn::vector_t>(il[3]->data).elems;
    for(size_t i=0, fi=0; i+1<vec.size(); i+=2, ++fi){ if(!std::holds_alternative<edn::symbol>(vec[i]->data) || !std::holds_alternative<edn::symbol>(vec[i+1]->data)) continue; std::string fname = symName(vec[i]); std::string val=trimPct(symName(vec[i+1])); if(val.empty()) continue; auto vit=S.vmap.find(val); if(vit==S.vmap.end()) continue; auto idxMapIt = idxIt->second.find(fname); if(idxMapIt==idxIt->second.end()) continue; uint32_t fidx = idxMapIt->second; llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); llvm::Value* fIndex = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx), fidx); auto *gep = S.builder.CreateInBoundsGEP(ST, allocaPtr, {zero, fIndex}, dst+"."+fname+".addr"); S.builder.CreateStore(vit->second, gep);}    
    S.vmap[dst]=allocaPtr; S.vtypes[dst]=S.tctx.get_pointer(S.tctx.get_struct(sname)); return true;
}

bool handle_member(builder::State& S, const std::vector<edn::node_ptr>& il,
                   const std::unordered_map<std::string, llvm::StructType*>& struct_types,
                   const std::unordered_map<std::string, std::unordered_map<std::string, size_t>>& struct_field_index,
                   const std::unordered_map<std::string, std::vector<edn::TypeId>>& struct_field_types){
    // (member %dst Struct %base field)
    if(il.size()!=5) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="member") return false;
    std::string dst=trimPct(symName(il[1])); std::string sname=symName(il[2]); std::string base=trimPct(symName(il[3])); std::string fname=symName(il[4]); if(dst.empty()||sname.empty()||base.empty()||fname.empty()) return false;
    auto bit = S.vmap.find(base); if(bit==S.vmap.end() || !S.vtypes.count(base)) return false; edn::TypeId bty=S.vtypes[base]; const edn::Type &BT=S.tctx.at(bty); edn::TypeId structId=0; bool baseIsPtr=false; if(BT.kind==edn::Type::Kind::Pointer){ baseIsPtr=true; if(S.tctx.at(BT.pointee).kind==edn::Type::Kind::Struct) structId=BT.pointee; } else if(BT.kind==edn::Type::Kind::Struct) structId=bty; if(structId==0||!baseIsPtr) return false; const edn::Type &ST=S.tctx.at(structId); if(ST.kind!=edn::Type::Kind::Struct || ST.struct_name!=sname) return false;
    auto stIt = struct_types.find(sname); if(stIt==struct_types.end()) return false; auto idxIt=struct_field_index.find(sname); if(idxIt==struct_field_index.end()) return false; auto fIt=idxIt->second.find(fname); if(fIt==idxIt->second.end()) return false; size_t fidx=fIt->second; auto ftIt=struct_field_types.find(sname); if(ftIt==struct_field_types.end()||fidx>=ftIt->second.size()) return false; llvm::Value *zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); llvm::Value *fieldIndex=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),(uint32_t)fidx); auto *gep=S.builder.CreateInBoundsGEP(stIt->second, bit->second, {zero, fieldIndex}, dst+".addr"); auto *lv=S.builder.CreateLoad(S.map_type(ftIt->second[fidx]), gep, dst); S.vmap[dst]=lv; S.vtypes[dst]=ftIt->second[fidx]; return true;
}

bool handle_member_addr(builder::State& S, const std::vector<edn::node_ptr>& il,
                        const std::unordered_map<std::string, llvm::StructType*>& struct_types,
                        const std::unordered_map<std::string, std::unordered_map<std::string, size_t>>& struct_field_index,
                        const std::unordered_map<std::string, std::vector<edn::TypeId>>& struct_field_types){
    // (member-addr %dst Struct %base field)
    if(il.size()!=5) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="member-addr") return false;
    std::string dst=trimPct(symName(il[1])); std::string sname=symName(il[2]); std::string base=trimPct(symName(il[3])); std::string fname=symName(il[4]); if(dst.empty()||sname.empty()||base.empty()||fname.empty()) return false;
    auto bit = S.vmap.find(base); if(bit==S.vmap.end() || !S.vtypes.count(base)) return false; edn::TypeId bty=S.vtypes[base]; const edn::Type &BT=S.tctx.at(bty); edn::TypeId structId=0; bool baseIsPtr=false; if(BT.kind==edn::Type::Kind::Pointer){ baseIsPtr=true; if(S.tctx.at(BT.pointee).kind==edn::Type::Kind::Struct) structId=BT.pointee; } else if(BT.kind==edn::Type::Kind::Struct) structId=bty; if(structId==0||!baseIsPtr) return false; const edn::Type &ST=S.tctx.at(structId); if(ST.kind!=edn::Type::Kind::Struct || ST.struct_name!=sname) return false;
    auto stIt = struct_types.find(sname); if(stIt==struct_types.end()) return false; auto idxIt=struct_field_index.find(sname); if(idxIt==struct_field_index.end()) return false; auto fIt=idxIt->second.find(fname); if(fIt==idxIt->second.end()) return false; size_t fidx=fIt->second; auto ftIt=struct_field_types.find(sname); if(ftIt==struct_field_types.end()||fidx>=ftIt->second.size()) return false; llvm::Value *zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); llvm::Value *fieldIndex=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),(uint32_t)fidx); auto *gep=S.builder.CreateInBoundsGEP(stIt->second, bit->second, {zero, fieldIndex}, dst+".addr"); S.vmap[dst]=gep; S.vtypes[dst]=S.tctx.get_pointer(ftIt->second[fidx]); return true;
}

bool handle_union_member(builder::State& S, const std::vector<edn::node_ptr>& il,
                         const std::unordered_map<std::string, llvm::StructType*>& struct_types,
                         const std::unordered_map<std::string, std::unordered_map<std::string, edn::TypeId>>& union_field_types){
    // (union-member %dst Union %ptr field)
    if(il.size()!=5) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="union-member") return false;
    std::string dst=trimPct(symName(il[1])); std::string uname=symName(il[2]); std::string base=trimPct(symName(il[3])); std::string fname=symName(il[4]); if(dst.empty()||uname.empty()||base.empty()||fname.empty()) return false;
    auto bit = S.vmap.find(base); if(bit==S.vmap.end() || !S.vtypes.count(base)) return false; edn::TypeId bty=S.vtypes[base]; const edn::Type &BT=S.tctx.at(bty); if(BT.kind!=edn::Type::Kind::Pointer) return false; edn::TypeId pointee=BT.pointee; const edn::Type &PT=S.tctx.at(pointee); if(PT.kind!=edn::Type::Kind::Struct || PT.struct_name!=uname) return false;
    auto stIt = struct_types.find(uname); if(stIt==struct_types.end()) return false; auto uftIt=union_field_types.find(uname); if(uftIt==union_field_types.end()) return false; auto fTyIt=uftIt->second.find(fname); if(fTyIt==uftIt->second.end()) return false; edn::TypeId fieldTy=fTyIt->second;
    llvm::Value *zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); llvm::Value *storageIndex=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); auto *storagePtr=S.builder.CreateInBoundsGEP(stIt->second, bit->second, {zero, storageIndex}, dst+".ustorage.addr");
    llvm::Type *fieldLL = S.map_type(fieldTy); auto *fieldPtrTy = llvm::PointerType::getUnqual(fieldLL); auto *rawPtr = S.builder.CreateBitCast(storagePtr, fieldPtrTy, dst+".cast"); auto *lv = S.builder.CreateLoad(fieldLL, rawPtr, dst); S.vmap[dst]=lv; S.vtypes[dst]=fieldTy; return true;
}

} // namespace edn::ir::memory_ops
