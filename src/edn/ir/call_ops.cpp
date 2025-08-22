#include "edn/ir/call_ops.hpp"
#include "edn/ir/exceptions.hpp"
#include "edn/ir/exception_ops.hpp"

using namespace edn;

namespace edn::ir::call_ops {

static std::string trimPct(const std::string& s) {
    if(!s.empty() && s[0]=='%') return s.substr(1); return s; }
static std::string symName(const edn::node_ptr& n) {
    if(!n) return {}; if(std::holds_alternative<symbol>(n->data)) return std::get<symbol>(n->data).name; return {}; }

bool handle_varargs(Context C, const std::vector<edn::node_ptr>& il) {
    if(il.empty() || !il[0] || !std::holds_alternative<symbol>(il[0]->data)) return false;
    const std::string op = std::get<symbol>(il[0]->data).name;
    if(op == "va-start" && il.size()==2) {
        std::string ap = trimPct(symName(il[1]));
        if(ap.empty()) return true; // noop but consumed
        llvm::Value* nullp = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(C.S.llctx)));
        C.S.vmap[ap] = nullp; // type already validated by checker
        return true;
    } else if(op == "va-arg" && il.size()==4) {
        std::string dst = trimPct(symName(il[1]));
        if(dst.empty()) return true; // produce undef & consume
        TypeId ty; try { ty = C.S.tctx.parse_type(il[2]); } catch(...) { return true; }
        llvm::Type* lty = C.S.map_type(ty);
        llvm::Value* uv = llvm::UndefValue::get(lty);
        C.S.vmap[dst] = uv; C.S.vtypes[dst] = ty; return true;
    } else if(op == "va-end" && il.size()==2) {
        return true; // noop
    }
    return false;
}

bool handle_call(Context C, const std::vector<edn::node_ptr>& il) {
    if(il.empty() || !il[0] || !std::holds_alternative<symbol>(il[0]->data)) return false;
    const std::string op = std::get<symbol>(il[0]->data).name;
    if(op != "call" || il.size() < 4) return false;
    auto& B = C.S.builder;
    auto& llctx = C.S.llctx;
    auto& module = C.S.module;
    auto& vmap = C.S.vmap;
    auto& vtypes = C.S.vtypes;
    std::string dst = trimPct(symName(il[1]));
    TypeId retTy; try { retTy = C.S.tctx.parse_type(il[2]); } catch(...) { return true; }
    std::string callee = symName(il[3]); if(callee.empty()) return true;
    llvm::Function* CF = module.getFunction(callee);
    auto map_type = C.S.map_type;
    if(!CF) {
        bool foundHeader=false; std::vector<llvm::Type*> headerParamLL; bool headerVariadic=false; TypeId retHeader = C.S.tctx.get_base(BaseType::Void);
        for(size_t ti=1; ti<C.topLevel.size() && !foundHeader; ++ti){
            auto &fnNode = C.topLevel[ti];
            if(!fnNode || !std::holds_alternative<list>(fnNode->data)) continue;
            auto &fl2 = std::get<list>(fnNode->data).elems;
            if(fl2.empty() || !std::holds_alternative<symbol>(fl2[0]->data) || std::get<symbol>(fl2[0]->data).name != "fn") continue;
            std::string fname2; std::vector<TypeId> paramTypeIds; bool varargFlag=false; retHeader = C.S.tctx.get_base(BaseType::Void);
            for(size_t j=1;j<fl2.size();++j){
                if(!fl2[j] || !std::holds_alternative<keyword>(fl2[j]->data)) break; std::string kw = std::get<keyword>(fl2[j]->data).name; if(++j>=fl2.size()) break; auto val=fl2[j];
                if(kw=="name") fname2 = symName(val);
                else if(kw=="ret") { try { retHeader = C.S.tctx.parse_type(val);} catch(...) { retHeader = C.S.tctx.get_base(BaseType::Void);} }
                else if(kw=="params" && val && std::holds_alternative<vector_t>(val->data)) {
                    for(auto &p: std::get<vector_t>(val->data).elems){ if(!p || !std::holds_alternative<list>(p->data)) continue; auto &pl = std::get<list>(p->data).elems; if(pl.size()==3 && std::holds_alternative<symbol>(pl[0]->data) && std::get<symbol>(pl[0]->data).name=="param") { try { TypeId pty = C.S.tctx.parse_type(pl[1]); paramTypeIds.push_back(pty);} catch(...){} } }
                } else if(kw=="vararg") { if(val && std::holds_alternative<bool>(val->data)) varargFlag = std::get<bool>(val->data); }
            }
            if(fname2 == callee) {
                for(auto pid: paramTypeIds) headerParamLL.push_back(map_type(pid));
                headerVariadic = varargFlag;
                CF = llvm::Function::Create(llvm::FunctionType::get(map_type(retHeader), headerParamLL, headerVariadic), llvm::Function::ExternalLinkage, callee, &module);
                foundHeader=true; break;
            }
        }
        if(!foundHeader) {
            std::vector<llvm::Type*> argLTys; argLTys.reserve(il.size()-4);
            for(size_t ai=4; ai<il.size(); ++ai){ std::string av = trimPct(symName(il[ai])); if(av.empty() || !vtypes.count(av)) { argLTys.clear(); break;} argLTys.push_back(map_type(vtypes[av])); }
            CF = llvm::Function::Create(llvm::FunctionType::get(map_type(retTy), argLTys, false), llvm::Function::ExternalLinkage, callee, &module);
        }
    }
    std::vector<llvm::Value*> args; args.reserve(il.size()-4);
    for(size_t ai=4; ai<il.size(); ++ai){ llvm::Value* v = C.getVal(il[ai]); if(!v){ args.clear(); break;} args.push_back(v); }
    if(args.size() + 4 != il.size()) return true; // malformed, but consumed
    if(C.enableEHItanium && C.selectedPersonality){
        auto *contBB = llvm::BasicBlock::Create(llctx, "invoke.cont." + std::to_string(C.cfCounter++), C.currentFunction);
        llvm::BasicBlock* exTarget = C.itnExceptTargetStack.empty() ? edn::ir::exceptions::create_panic_cleanup_landingpad(C.currentFunction, B) : C.itnExceptTargetStack.back();
        llvm::InvokeInst* inv = B.CreateInvoke(CF, contBB, exTarget, args, CF->getReturnType()->isVoidTy() ? "" : dst);
        B.SetInsertPoint(contBB);
        if(!CF->getReturnType()->isVoidTy()) { vmap[dst] = inv; vtypes[dst] = retTy; }
    } else if(C.enableEHSEH && C.selectedPersonality){
        auto *contBB = llvm::BasicBlock::Create(llctx, "invoke.cont." + std::to_string(C.cfCounter++), C.currentFunction);
        C.sehCleanupBB = edn::ir::exceptions::ensure_seh_cleanup(C.currentFunction, B, C.sehCleanupBB);
        llvm::BasicBlock* exTarget = C.sehExceptTargetStack.empty() ? C.sehCleanupBB : C.sehExceptTargetStack.back();
        llvm::InvokeInst* inv = B.CreateInvoke(CF, contBB, exTarget, args, CF->getReturnType()->isVoidTy() ? "" : dst);
        B.SetInsertPoint(contBB);
        if(!CF->getReturnType()->isVoidTy()) { vmap[dst] = inv; vtypes[dst] = retTy; }
    } else {
        auto *callInst = B.CreateCall(CF, args, CF->getReturnType()->isVoidTy() ? "" : dst);
        if(!CF->getReturnType()->isVoidTy()) { vmap[dst] = callInst; vtypes[dst] = retTy; }
    }
    return true;
}

} // namespace edn::ir::call_ops
