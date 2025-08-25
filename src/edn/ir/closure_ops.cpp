#include "edn/ir/closure_ops.hpp"
#include "edn/ir/di.hpp"
#include <llvm/IR/IRBuilder.h>

namespace edn::ir::closure_ops {

static std::string symName(const edn::node_ptr &n){ if(!n) return {}; if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name; if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; }
static std::string trimPct(const std::string &s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }

bool handle_closure(builder::State& S, const std::vector<edn::node_ptr>& il,
                    const std::vector<edn::node_ptr>& top, size_t& cfCounter){
    if(!(il.size()>=5)) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="closure") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId fnPtrTy; try { fnPtrTy = S.tctx.parse_type(il[2]); } catch(...) { return false; }
    std::string callee = symName(il[3]); if(callee.empty()) return false;
    if(!std::holds_alternative<edn::vector_t>(il[4]->data)) return false; auto caps = std::get<edn::vector_t>(il[4]->data).elems; if(caps.size()!=1) return false; std::string envVar = trimPct(symName(caps[0])); if(envVar.empty()||!S.vmap.count(envVar)) return false;
    // Ensure target function exists; synthesize if needed from headers
    auto *TargetF = S.module.getFunction(callee);
    if(!TargetF){
        for(size_t ti=1; ti<top.size(); ++ti){ auto &fnNode = top[ti]; if(!fnNode || !std::holds_alternative<edn::list>(fnNode->data)) continue; auto &fl2 = std::get<edn::list>(fnNode->data).elems; if(fl2.empty() || !std::holds_alternative<edn::symbol>(fl2[0]->data) || std::get<edn::symbol>(fl2[0]->data).name!="fn") continue; std::string fname2; edn::TypeId retHeader = S.tctx.get_base(edn::BaseType::Void); std::vector<edn::TypeId> paramTypeIds; bool varargFlag=false; for(size_t j=1;j<fl2.size();++j){ if(!fl2[j] || !std::holds_alternative<edn::keyword>(fl2[j]->data)) break; std::string kw = std::get<edn::keyword>(fl2[j]->data).name; if(++j>=fl2.size()) break; auto val = fl2[j]; if(kw=="name") fname2 = symName(val); else if(kw=="ret"){ try { retHeader = S.tctx.parse_type(val); } catch(...) { retHeader = S.tctx.get_base(edn::BaseType::Void);} } else if(kw=="params" && val && std::holds_alternative<edn::vector_t>(val->data)){ for(auto &p: std::get<edn::vector_t>(val->data).elems){ if(!p || !std::holds_alternative<edn::list>(p->data)) continue; auto &pl = std::get<edn::list>(p->data).elems; if(pl.size()==3 && std::holds_alternative<edn::symbol>(pl[0]->data) && std::get<edn::symbol>(pl[0]->data).name=="param"){ try { edn::TypeId pty = S.tctx.parse_type(pl[1]); paramTypeIds.push_back(pty);} catch(...){} } } } else if(kw=="vararg"){ if(val && std::holds_alternative<bool>(val->data)) varargFlag = std::get<bool>(val->data); } }
            if(fname2==callee){ std::vector<llvm::Type*> pls; for(auto pid: paramTypeIds) pls.push_back(S.map_type(pid)); auto *fty = llvm::FunctionType::get(S.map_type(retHeader), pls, varargFlag); TargetF = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, callee, &S.module); break; }
        }
        if(!TargetF) return false;
    }
    // Create unique private global holding env pointer
    auto *envVal = S.vmap[envVar]; llvm::Type *envPtrTy = envVal->getType(); std::string gname = "__edn.closure.env." + std::to_string(reinterpret_cast<uintptr_t>(envVal)) + "." + std::to_string(cfCounter++);
    auto *G = new llvm::GlobalVariable(S.module, envPtrTy, false, llvm::GlobalValue::PrivateLinkage, llvm::Constant::getNullValue(envPtrTy), gname);
    S.builder.CreateStore(envVal, G);
    // Build thunk of function pointer type that loads env and calls target
    const edn::Type &PT = S.tctx.at(fnPtrTy); if(PT.kind!=edn::Type::Kind::Pointer) return false; const edn::Type &FT = S.tctx.at(PT.pointee); if(FT.kind!=edn::Type::Kind::Function) return false;
    std::vector<llvm::Type*> thunkParams; thunkParams.reserve(FT.params.size()); for(auto pid: FT.params) thunkParams.push_back(S.map_type(pid));
    auto *thunkFTy = llvm::FunctionType::get(S.map_type(FT.ret), thunkParams, FT.variadic);
    std::string thunkName = "__edn.closure.thunk." + callee + "." + std::to_string(cfCounter++);
    auto *ThunkF = llvm::Function::Create(thunkFTy, llvm::Function::PrivateLinkage, thunkName, &S.module);
    size_t ai2=0; for(auto &a: ThunkF->args()) a.setName("a"+std::to_string(ai2++));
    auto *thunkEntry = llvm::BasicBlock::Create(S.llctx, "entry", ThunkF);
    llvm::IRBuilder<> tb(thunkEntry);
    llvm::Value *loadedEnv = tb.CreateLoad(envPtrTy, G, "env"); std::vector<llvm::Value*> callArgs; callArgs.push_back(loadedEnv); for(auto &a: ThunkF->args()) callArgs.push_back(&a);
    // Debug: attach DISubprogram for thunk + parameter env if enabled
    if(S.debug_manager && S.debug_manager->enableDebugInfo){
        std::vector<std::pair<std::string, edn::TypeId>> paramMeta; paramMeta.reserve(FT.params.size()+1);
        // First synthetic env param (match underlying pointer type; map to opaque i8* for DI simplicity)
        edn::TypeId envTyId = PT.pointee; // fallback; if mismatch, treat as void*
        paramMeta.emplace_back("env", envTyId);
        for(size_t pi=0; pi<FT.params.size(); ++pi){ paramMeta.emplace_back("a"+std::to_string(pi), FT.params[pi]); }
        di::attach_function_debug(*S.debug_manager, *ThunkF, thunkName, FT.ret, paramMeta, /*line*/1);
        // Emit dbg.value for each param (a0..), env loaded value after load
        if(ThunkF->getSubprogram()){
            // Insert env parameter dbg.value referencing loadedEnv
            di::declare_local(*S.debug_manager, tb, loadedEnv, "env", envTyId, 1);
        }
    }
    if(!TargetF->getFunctionType()->isFunctionVarArg() || TargetF->arg_empty() || TargetF->getFunctionType()) {
        // basic sanity print
        fprintf(stderr, "[dbg][closure_ops] target func '%s' fty ok, param_count=%u\n", TargetF->getName().str().c_str(), (unsigned)TargetF->getFunctionType()->getNumParams());
    }
    auto *rawTargetFTy = TargetF->getFunctionType();
    if(!llvm::isa<llvm::FunctionType>(rawTargetFTy)) {
        fprintf(stderr, "[dbg][cast] expected FunctionType for closure target but got different kind=%u name=%s\n", (unsigned)rawTargetFTy->getTypeID(), TargetF->getName().str().c_str());
    }
    auto *targetFTy = llvm::cast<llvm::FunctionType>(rawTargetFTy);
    auto *call = tb.CreateCall(targetFTy, TargetF, callArgs, targetFTy->getReturnType()->isVoidTy()?"":"retv");
    if(targetFTy->getReturnType()->isVoidTy()) tb.CreateRetVoid(); else tb.CreateRet(call);
    S.vmap[dst]=ThunkF; S.vtypes[dst]=fnPtrTy; return true;
}

bool handle_make_closure(builder::State& S, const std::vector<edn::node_ptr>& il,
                         const std::vector<edn::node_ptr>& top){
    if(!(il.size()>=4)) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="make-closure") return false; std::string dst=trimPct(symName(il[1])); if(dst.empty()) return false; std::string callee = symName(il[2]); if(callee.empty()) return false; if(!std::holds_alternative<edn::vector_t>(il[3]->data)) return false; auto caps = std::get<edn::vector_t>(il[3]->data).elems; if(caps.size()!=1) return false; std::string envVar=trimPct(symName(caps[0])); if(envVar.empty()||!S.vmap.count(envVar)) return false; auto *TargetF = S.module.getFunction(callee); if(!TargetF){ for(size_t ti=1; ti<top.size(); ++ti){ auto &fnNode=top[ti]; if(!fnNode || !std::holds_alternative<edn::list>(fnNode->data)) continue; auto &fl2 = std::get<edn::list>(fnNode->data).elems; if(fl2.empty() || !std::holds_alternative<edn::symbol>(fl2[0]->data) || std::get<edn::symbol>(fl2[0]->data).name!="fn") continue; std::string fname2; edn::TypeId retHeader = S.tctx.get_base(edn::BaseType::Void); std::vector<edn::TypeId> paramTypeIds; bool varargFlag=false; for(size_t j=1;j<fl2.size();++j){ if(!fl2[j] || !std::holds_alternative<edn::keyword>(fl2[j]->data)) break; std::string kw=std::get<edn::keyword>(fl2[j]->data).name; if(++j>=fl2.size()) break; auto val=fl2[j]; if(kw=="name") fname2=symName(val); else if(kw=="ret"){ try{ retHeader=S.tctx.parse_type(val);}catch(...){ retHeader=S.tctx.get_base(edn::BaseType::Void);} } else if(kw=="params" && val && std::holds_alternative<edn::vector_t>(val->data)){ for(auto &p: std::get<edn::vector_t>(val->data).elems){ if(!p || !std::holds_alternative<edn::list>(p->data)) continue; auto &pl = std::get<edn::list>(p->data).elems; if(pl.size()==3 && std::holds_alternative<edn::symbol>(pl[0]->data) && std::get<edn::symbol>(pl[0]->data).name=="param"){ try{ edn::TypeId pty=S.tctx.parse_type(pl[1]); paramTypeIds.push_back(pty);}catch(... ){} } } } else if(kw=="vararg"){ if(val && std::holds_alternative<bool>(val->data)) varargFlag = std::get<bool>(val->data); } }
            if(fname2==callee){ std::vector<llvm::Type*> pls; for(auto pid: paramTypeIds) pls.push_back(S.map_type(pid)); auto *fty=llvm::FunctionType::get(S.map_type(retHeader), pls, varargFlag); TargetF=llvm::Function::Create(fty, llvm::Function::ExternalLinkage, callee, &S.module); break; } }
        if(!TargetF) return false; }
    std::string sname = "__edn.closure." + callee; auto *ST = llvm::StructType::getTypeByName(S.llctx, "struct."+sname); if(!ST){ auto *i8ptr = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(S.llctx)); std::vector<llvm::Type*> flds = {i8ptr, S.vmap[envVar]->getType()}; ST = llvm::StructType::create(S.llctx, flds, "struct."+sname); }
    auto *allocaPtr = S.builder.CreateAlloca(ST, nullptr, dst); llvm::Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); llvm::Value *idxEnv = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),1); auto *envPtr = S.builder.CreateInBoundsGEP(ST, allocaPtr, {zero, idxEnv}, dst+".env.addr"); S.builder.CreateStore(S.vmap[envVar], envPtr); llvm::Value *idxFn = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); auto *fnPtr = S.builder.CreateInBoundsGEP(ST, allocaPtr, {zero, idxFn}, dst+".fn.addr"); auto *i8ptr = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(S.llctx)); llvm::Value *fnAsI8 = S.builder.CreateBitCast(TargetF, i8ptr, dst+".fn.cast"); S.builder.CreateStore(fnAsI8, fnPtr); S.vmap[dst]=allocaPtr; S.vtypes[dst]=S.tctx.get_pointer(S.tctx.get_struct(sname));
    if(S.debug_manager && S.debug_manager->enableDebugInfo){
        di::declare_local(*S.debug_manager, S.builder, allocaPtr, dst, S.tctx.get_struct(sname), S.builder.getCurrentDebugLocation()? S.builder.getCurrentDebugLocation()->getLine(): (S.builder.GetInsertBlock()->getParent()->getSubprogram()? S.builder.GetInsertBlock()->getParent()->getSubprogram()->getLine():1));
    }
    return true; }

bool handle_call_closure(builder::State& S, const std::vector<edn::node_ptr>& il){
    if(!(il.size()>=4)) return false; if(!std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="call-closure") return false; std::string dst=trimPct(symName(il[1])); if(dst.empty()) return false; edn::TypeId retTy; try { retTy = S.tctx.parse_type(il[2]); } catch(...) { return false; } std::string clos=trimPct(symName(il[3])); if(clos.empty()||!S.vmap.count(clos)) return false; auto ctyIt=S.vtypes.find(clos); if(ctyIt==S.vtypes.end()) return false; const edn::Type &CT = S.tctx.at(ctyIt->second); if(CT.kind!=edn::Type::Kind::Pointer) return false; const edn::Type &STy = S.tctx.at(CT.pointee); if(STy.kind!=edn::Type::Kind::Struct) return false; std::string sname = STy.struct_name; auto *ST = llvm::StructType::getTypeByName(S.llctx, "struct."+sname); if(!ST) return false; llvm::Value *zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); llvm::Value *idxFn=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),0); llvm::Value *idxEnv=llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx),1); auto *fnPtrAddr = S.builder.CreateInBoundsGEP(ST, S.vmap[clos], {zero, idxFn}, dst+".fn.addr"); auto *envAddr = S.builder.CreateInBoundsGEP(ST, S.vmap[clos], {zero, idxEnv}, dst+".env.addr"); auto *i8ptr2 = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(S.llctx)); llvm::Value *fnI8 = S.builder.CreateLoad(i8ptr2, fnPtrAddr, dst+".fn"); llvm::Type *envTy = ST->getElementType(1); llvm::Value *envV = S.builder.CreateLoad(envTy, envAddr, dst+".env"); std::vector<llvm::Value*> args; args.push_back(envV); for(size_t ai=4; ai<il.size(); ++ai){ std::string an=trimPct(symName(il[ai])); if(an.empty()||!S.vmap.count(an)){ args.clear(); break; } args.push_back(S.vmap[an]); } if(args.empty()) return false; std::string prefix="__edn.closure."; if(sname.rfind(prefix,0)!=0) return false; std::string callee = sname.substr(prefix.size()); auto *TargetF = S.module.getFunction(callee); if(!TargetF) return false; auto *calleeFTy = TargetF->getFunctionType();
    // Bitcast the erased i8* back to the precise function pointer type before calling to satisfy LLVM's type expectations.
    auto *typedFn = S.builder.CreateBitCast(fnI8, calleeFTy->getPointerTo(), dst+".fntyped");
    auto *call = S.builder.CreateCall(calleeFTy, typedFn, args, calleeFTy->getReturnType()->isVoidTy()?"":dst);
    if(!calleeFTy->getReturnType()->isVoidTy()){ S.vmap[dst]=call; S.vtypes[dst]=retTy; }
    return true; }

} // namespace edn::ir::closure_ops
