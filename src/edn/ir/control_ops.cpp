#include "edn/ir/control_ops.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "edn/ir/types.hpp" // provides edn::ir::size_in_bytes wrapper
#include "edn/ir/layout.hpp"

using namespace edn;

namespace edn::ir::control_ops {

static std::string symName(const node_ptr& n) {
    if (!n) return {};
    if (std::holds_alternative<symbol>(n->data)) return std::get<symbol>(n->data).name;
    return {};
}

static std::string trimPct(const std::string& s) {
    if (!s.empty() && s[0]=='%') return s.substr(1);
    return s;
}

static bool isOp(const std::vector<node_ptr>& il, const char* name) {
    if (il.empty()) return false;
    if (!il[0] || !std::holds_alternative<symbol>(il[0]->data)) return false;
    return std::get<symbol>(il[0]->data).name == name;
}

bool handle(Context& C, const std::vector<node_ptr>& il) {
    auto& B = C.S.builder;
    auto& llctx = C.S.llctx;
    auto& vmap = C.S.vmap;
    auto& vtypes = C.S.vtypes;
    auto& varSlots = C.S.varSlots;

    // if ------------------------------------------------------------------
    if (isOp(il, "if")) {
        if (il.size() >= 3) {
            std::string cName = trimPct(symName(il[1]));
            llvm::Value* condV = nullptr;
            if (auto sIt = varSlots.find(cName); sIt != varSlots.end()) {
                if (auto tIt = vtypes.find(cName); tIt != vtypes.end())
                    condV = B.CreateLoad(C.S.map_type(tIt->second), sIt->second, cName);
            } else {
                condV = C.eval_defined(cName);
                if (!condV) condV = C.get_val(il[1]);
            }
            if (!condV) return true; // malformed -> skip
            auto *thenBB = llvm::BasicBlock::Create(llctx, "if.then." + std::to_string(C.cfCounter++), C.F);
            llvm::BasicBlock* elseBB = nullptr;
            auto *mergeBB = llvm::BasicBlock::Create(llctx, "if.end." + std::to_string(C.cfCounter++), C.F);
            bool hasElse = il.size() >= 4 && std::holds_alternative<vector_t>(il[3]->data);
            if (hasElse)
                elseBB = llvm::BasicBlock::Create(llctx, "if.else." + std::to_string(C.cfCounter++), C.F);
            if (!B.GetInsertBlock()->getTerminator())
                B.CreateCondBr(condV, thenBB, hasElse ? elseBB : mergeBB);
            // then (push lexical scope for 'then')
            B.SetInsertPoint(thenBB);
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->pushLexicalBlock(/*line*/1, 1, &B);
            if (std::holds_alternative<vector_t>(il[2]->data))
                C.emit_ref(std::get<vector_t>(il[2]->data).elems);
            if (!B.GetInsertBlock()->getTerminator())
                B.CreateBr(mergeBB);
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->popScope(&B);
            if (hasElse) {
                B.SetInsertPoint(elseBB);
                if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                    C.S.debug_manager->pushLexicalBlock(/*line*/1, 1, &B);
                C.emit_ref(std::get<vector_t>(il[3]->data).elems);
                if (!B.GetInsertBlock()->getTerminator())
                    B.CreateBr(mergeBB);
                if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                    C.S.debug_manager->popScope(&B);
            }
            B.SetInsertPoint(mergeBB);
        }
        return true;
    }

    // while ----------------------------------------------------------------
    if (isOp(il, "while")) {
        if (il.size() >= 3 && std::holds_alternative<vector_t>(il[2]->data)) {
            auto *condBB = llvm::BasicBlock::Create(llctx, "while.cond." + std::to_string(C.cfCounter++), C.F);
            auto *bodyBB = llvm::BasicBlock::Create(llctx, "while.body." + std::to_string(C.cfCounter++), C.F);
            auto *endBB  = llvm::BasicBlock::Create(llctx, "while.end."  + std::to_string(C.cfCounter++), C.F);
            if (!B.GetInsertBlock()->getTerminator()) B.CreateBr(condBB);
            B.SetInsertPoint(condBB);
            std::string wName = trimPct(symName(il[1]));
            llvm::Value* condV = nullptr;
            if (auto sIt = varSlots.find(wName); sIt != varSlots.end()) {
                if (auto tIt = vtypes.find(wName); tIt != vtypes.end())
                    condV = B.CreateLoad(C.S.map_type(tIt->second), sIt->second, wName);
            } else {
                condV = C.eval_defined(wName);
                if (!condV) condV = C.get_val(il[1]);
            }
            if (!condV) {
                B.CreateBr(endBB);
                B.SetInsertPoint(endBB);
            } else {
                B.CreateCondBr(condV, bodyBB, endBB);
                B.SetInsertPoint(bodyBB);
                C.loopEndStack.push_back(endBB);
                C.loopContinueStack.push_back(condBB);
                if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                    C.S.debug_manager->pushLexicalBlock(/*line*/1,1,&B);
                C.emit_ref(std::get<vector_t>(il[2]->data).elems);
                if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                    C.S.debug_manager->popScope(&B);
                C.loopContinueStack.pop_back();
                C.loopEndStack.pop_back();
                if (!B.GetInsertBlock()->getTerminator()) B.CreateBr(condBB);
                B.SetInsertPoint(endBB);
            }
        }
        return true;
    }

    // for -------------------------------------------------------------------
    if (isOp(il, "for")) {
        // (for :init [..] :cond %c :step [..] :body [..])
        std::vector<node_ptr> initVec, stepVec, bodyVec; std::string condVar;
        for (size_t i = 1; i < il.size(); ++i) {
            if (!il[i] || !std::holds_alternative<keyword>(il[i]->data)) break;
            std::string kw = std::get<keyword>(il[i]->data).name;
            if (++i >= il.size()) break; auto val = il[i];
            if (kw=="init" && val && std::holds_alternative<vector_t>(val->data)) initVec = std::get<vector_t>(val->data).elems;
            else if (kw=="cond" && val && std::holds_alternative<symbol>(val->data)) condVar = symName(val);
            else if (kw=="step" && val && std::holds_alternative<vector_t>(val->data)) stepVec = std::get<vector_t>(val->data).elems;
            else if (kw=="body" && val && std::holds_alternative<vector_t>(val->data)) bodyVec = std::get<vector_t>(val->data).elems;
        }
        if (!initVec.empty()) C.emit_ref(initVec);
        auto *condBB = llvm::BasicBlock::Create(llctx, "for.cond." + std::to_string(C.cfCounter++), C.F);
        auto *bodyBB = llvm::BasicBlock::Create(llctx, "for.body." + std::to_string(C.cfCounter++), C.F);
        auto *stepBB = llvm::BasicBlock::Create(llctx, "for.step." + std::to_string(C.cfCounter++), C.F);
        auto *endBB  = llvm::BasicBlock::Create(llctx, "for.end."  + std::to_string(C.cfCounter++), C.F);
        if (!B.GetInsertBlock()->getTerminator()) B.CreateBr(condBB);
        B.SetInsertPoint(condBB);
        std::string cName = trimPct(condVar);
        llvm::Value* condV = nullptr;
        if (auto sIt = varSlots.find(cName); sIt != varSlots.end()) {
            if (auto tIt = vtypes.find(cName); tIt != vtypes.end()) condV = B.CreateLoad(C.S.map_type(tIt->second), sIt->second, cName);
        } else {
            condV = C.eval_defined(cName);
            if (!condV) {
                if (auto itc = vmap.find(cName); itc != vmap.end()) condV = itc->second;
            }
        }
        if (!condV) {
            B.CreateBr(endBB); B.SetInsertPoint(endBB); return true;
        }
        B.CreateCondBr(condV, bodyBB, endBB);
        B.SetInsertPoint(bodyBB);
        C.loopEndStack.push_back(endBB);
        C.loopContinueStack.push_back(stepBB);
        if (!bodyVec.empty()) {
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->pushLexicalBlock(/*line*/1,1,&B);
            C.emit_ref(bodyVec);
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->popScope(&B);
        }
        C.loopContinueStack.pop_back();
        if (!B.GetInsertBlock()->getTerminator()) B.CreateBr(stepBB);
        B.SetInsertPoint(stepBB);
        if (!stepVec.empty()) {
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->pushLexicalBlock(/*line*/1,1,&B);
            C.emit_ref(stepVec);
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->popScope(&B);
        }
        C.loopEndStack.pop_back();
        if (!stepBB->getTerminator()) B.CreateBr(condBB);
        B.SetInsertPoint(endBB);
        return true;
    }

    // switch ----------------------------------------------------------------
    if (isOp(il, "switch")) {
        if (il.size() < 2) return true; // skip malformed
        std::string expr = trimPct(symName(il[1]));
        if (expr.empty() || !vmap.count(expr)) return true;
        llvm::Value* exprV = vmap[expr];
        // parse sections
        node_ptr casesNode = nullptr, defaultNode = nullptr; bool haveDefault=false;
        for (size_t i=2;i<il.size();++i) {
            if (!il[i] || !std::holds_alternative<keyword>(il[i]->data)) break;
            std::string kw = std::get<keyword>(il[i]->data).name; if (++i >= il.size()) break; auto val = il[i];
            if (kw=="cases" && val && std::holds_alternative<vector_t>(val->data)) casesNode = val;
            else if (kw=="default" && val && std::holds_alternative<vector_t>(val->data)) { defaultNode = val; haveDefault=true; }
        }
        std::vector<std::pair<int64_t,std::vector<node_ptr>>> cases;
        std::vector<node_ptr> defaultBody;
        if (casesNode) {
            for (auto &cv : std::get<vector_t>(casesNode->data).elems) {
                if (!cv || !std::holds_alternative<list>(cv->data)) continue;
                auto &cl = std::get<list>(cv->data).elems;
                if (cl.size()<3) continue;
                if (!std::holds_alternative<symbol>(cl[0]->data) || std::get<symbol>(cl[0]->data).name != "case") continue;
                if (!std::holds_alternative<int64_t>(cl[1]->data)) continue;
                if (!std::holds_alternative<vector_t>(cl[2]->data)) continue;
                int64_t cval = std::get<int64_t>(cl[1]->data);
                cases.emplace_back(cval, std::get<vector_t>(cl[2]->data).elems);
            }
        }
        if (haveDefault && defaultNode) defaultBody = std::get<vector_t>(defaultNode->data).elems;
        auto *mergeBB = llvm::BasicBlock::Create(llctx, "switch.end." + std::to_string(C.cfCounter++), C.F);
        std::vector<llvm::BasicBlock*> caseBlocks; caseBlocks.reserve(cases.size());
        for (auto &cv : cases) caseBlocks.push_back(llvm::BasicBlock::Create(llctx, "switch.case." + std::to_string(cv.first) + "." + std::to_string(C.cfCounter++), C.F));
        llvm::BasicBlock* defaultBB = haveDefault ? llvm::BasicBlock::Create(llctx, "switch.default." + std::to_string(C.cfCounter++), C.F) : mergeBB;
        auto *curBB = B.GetInsertBlock(); if (!curBB->getTerminator()) B.CreateBr(caseBlocks.empty()? defaultBB : caseBlocks[0]);
        for (size_t ci=0; ci<cases.size(); ++ci) {
            B.SetInsertPoint(caseBlocks[ci]);
            int64_t cval = cases[ci].first;
            llvm::Value* constVal = nullptr;
            if (exprV->getType()->isIntegerTy()) constVal = llvm::ConstantInt::get(exprV->getType(), (uint64_t)cval, true);
            else { B.CreateBr(defaultBB); continue; }
            llvm::Value* cmp = B.CreateICmpEQ(exprV, constVal, "swcmp");
            auto *bodyBB = llvm::BasicBlock::Create(llctx, "switch.case.body." + std::to_string(cval) + "." + std::to_string(C.cfCounter++), C.F);
            auto *nextCmpBB = (ci+1<cases.size()) ? caseBlocks[ci+1] : defaultBB;
            B.CreateCondBr(cmp, bodyBB, nextCmpBB);
            B.SetInsertPoint(bodyBB);
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->pushLexicalBlock(/*line*/1,1,&B);
            C.emit_ref(cases[ci].second);
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->popScope(&B);
            if (!bodyBB->getTerminator()) B.CreateBr(mergeBB);
        }
        if (haveDefault) {
            B.SetInsertPoint(defaultBB);
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->pushLexicalBlock(/*line*/1,1,&B);
            C.emit_ref(defaultBody);
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->popScope(&B);
            if (!defaultBB->getTerminator()) B.CreateBr(mergeBB);
        }
        B.SetInsertPoint(mergeBB);
        return true;
    }

    // match -----------------------------------------------------------------
    if (isOp(il, "match")) {
        if (il.size() < 3) return true;
        bool resultMode=false; std::string dstName; TypeId resultTy=0; size_t argBase=1;
        if (std::holds_alternative<symbol>(il[1]->data)) {
            std::string maybeDst = symName(il[1]);
            if (!maybeDst.empty() && maybeDst[0]=='%') {
                resultMode=true; dstName = trimPct(maybeDst);
                if (il.size()<5) return true;
                try { resultTy = C.S.tctx.parse_type(il[2]); } catch (...) { return true; }
                argBase=3;
            }
        }
        std::string sname = symName(il[argBase]);
        std::string val = trimPct(symName(il[argBase+1]));
        if (sname.empty() || val.empty() || !vmap.count(val)) return true;
        auto *ST = llvm::StructType::getTypeByName(llctx, "struct." + sname);
        if (!ST) return true;
        llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(llctx), 0);
        llvm::Value* tagIdx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(llctx), 0);
        auto *tagPtr = B.CreateInBoundsGEP(ST, vmap[val], {zero, tagIdx}, "match.tag.addr");
        auto *tagVal = B.CreateLoad(llvm::Type::getInt32Ty(llctx), tagPtr, "match.tag");
        node_ptr casesNode=nullptr, defaultNode=nullptr; bool haveDefault=false;
        for (size_t i = argBase + 2; i < il.size(); ++i) {
            if (!il[i] || !std::holds_alternative<keyword>(il[i]->data)) break;
            std::string kw = std::get<keyword>(il[i]->data).name; if (++i >= il.size()) break; auto v = il[i];
            if (kw=="cases" && v && std::holds_alternative<vector_t>(v->data)) casesNode = v;
            else if (kw=="default" && v) { defaultNode = v; haveDefault = true; }
        }
        if (!casesNode) return true;
        auto tagMapIt = C.sum_variant_tag.find(sname); if (tagMapIt == C.sum_variant_tag.end()) return true;
        auto &tagMap = tagMapIt->second;
        auto *mergeBB = llvm::BasicBlock::Create(llctx, "match.end." + std::to_string(C.cfCounter++), C.F);
        struct CaseInfo { int tag; std::string vname; std::vector<node_ptr> body; std::vector<std::pair<std::string,size_t>> binds; std::string valueVar; };
        std::vector<CaseInfo> cases; cases.reserve(std::get<vector_t>(casesNode->data).elems.size());
        for (auto &cv : std::get<vector_t>(casesNode->data).elems) {
            if (!cv || !std::holds_alternative<list>(cv->data)) continue;
            auto &cl = std::get<list>(cv->data).elems; if (cl.size()<3) continue;
            if (!std::holds_alternative<symbol>(cl[0]->data) || std::get<symbol>(cl[0]->data).name != "case") continue;
            std::string vname = symName(cl[1]); if (vname.empty()) continue; auto tIt = tagMap.find(vname); if (tIt == tagMap.end()) continue;
            std::vector<node_ptr> bodyElems; std::vector<std::pair<std::string,size_t>> binds; std::string valueVar;
            if (std::holds_alternative<vector_t>(cl[2]->data)) {
                auto &ve = std::get<vector_t>(cl[2]->data).elems; bodyElems.reserve(ve.size());
                for (size_t bi=0; bi<ve.size(); ++bi) { auto &bn = ve[bi]; if (bn && std::holds_alternative<keyword>(bn->data)) { std::string kw = std::get<keyword>(bn->data).name; if (kw=="value" && bi+1<ve.size() && ve[bi+1] && std::holds_alternative<symbol>(ve[bi+1]->data)) { valueVar = trimPct(symName(ve[bi+1])); ++bi; continue; } } bodyElems.push_back(bn);} }
            else if (std::holds_alternative<keyword>(cl[2]->data)) {
                node_ptr bindsNode=nullptr, bodyNode=nullptr, valueNode=nullptr;
                for (size_t ci=2; ci<cl.size(); ++ci) { if (!cl[ci] || !std::holds_alternative<keyword>(cl[ci]->data)) break; std::string kw = std::get<keyword>(cl[ci]->data).name; if (++ci >= cl.size()) break; auto valn = cl[ci]; if (kw=="binds") bindsNode=valn; else if (kw=="body") bodyNode=valn; else if (kw=="value") valueNode=valn; }
                if (bodyNode && std::holds_alternative<vector_t>(bodyNode->data)) {
                    auto &ve = std::get<vector_t>(bodyNode->data).elems; bodyElems.reserve(ve.size());
                    for (size_t bi=0; bi<ve.size(); ++bi) { auto &bn2 = ve[bi]; if (bn2 && std::holds_alternative<keyword>(bn2->data)) { std::string kw2 = std::get<keyword>(bn2->data).name; if (kw2=="value" && bi+1<ve.size() && ve[bi+1] && std::holds_alternative<symbol>(ve[bi+1]->data)) { valueVar = trimPct(symName(ve[bi+1])); ++bi; continue; } } bodyElems.push_back(bn2);} }
                if (bindsNode && std::holds_alternative<vector_t>(bindsNode->data)) { for (auto &bn : std::get<vector_t>(bindsNode->data).elems) { if (!bn || !std::holds_alternative<list>(bn->data)) continue; auto &bl = std::get<list>(bn->data).elems; if (bl.size()!=3) continue; if (!std::holds_alternative<symbol>(bl[0]->data) || std::get<symbol>(bl[0]->data).name != "bind") continue; if (!std::holds_alternative<symbol>(bl[1]->data)) continue; std::string bname = trimPct(symName(bl[1])); if (bname.empty()) continue; if (!std::holds_alternative<int64_t>(bl[2]->data)) continue; int64_t idx = std::get<int64_t>(bl[2]->data); if (idx < 0) continue; binds.emplace_back(bname, (size_t)idx); } }
                if (valueNode && std::holds_alternative<symbol>(valueNode->data)) valueVar = trimPct(symName(valueNode));
            } else continue;
            cases.push_back(CaseInfo{tIt->second, vname, std::move(bodyElems), std::move(binds), valueVar});
        }
        std::vector<llvm::BasicBlock*> cmpBlocks; cmpBlocks.reserve(cases.size());
        for (size_t i=0;i<cases.size();++i) cmpBlocks.push_back(llvm::BasicBlock::Create(llctx, "match.case." + std::to_string(i) + "." + std::to_string(C.cfCounter++), C.F));
        llvm::BasicBlock* defaultBB = haveDefault ? llvm::BasicBlock::Create(llctx, "match.default." + std::to_string(C.cfCounter++), C.F) : mergeBB;
        struct IncomingVal { llvm::Value* val; llvm::BasicBlock* pred; };
        std::vector<IncomingVal> incomings;
        auto *curBB = B.GetInsertBlock(); if (!curBB->getTerminator()) B.CreateBr(cmpBlocks.empty()? defaultBB : cmpBlocks[0]);
        for (size_t ci=0; ci<cases.size(); ++ci) {
            B.SetInsertPoint(cmpBlocks[ci]);
            llvm::Value* cval = llvm::ConstantInt::get(llvm::Type::getInt32Ty(llctx), (uint64_t)cases[ci].tag, true);
            auto *cmp = B.CreateICmpEQ(tagVal, cval, "match.cmp");
            auto *bodyBB = llvm::BasicBlock::Create(llctx, "match.body." + std::to_string(ci) + "." + std::to_string(C.cfCounter++), C.F);
            auto *next = (ci+1<cases.size()) ? cmpBlocks[ci+1] : defaultBB;
            B.CreateCondBr(cmp, bodyBB, next);
            B.SetInsertPoint(bodyBB);
            if (!cases[ci].binds.empty()) {
                llvm::Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(llctx), 0);
                llvm::Value *payIdx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(llctx), 1);
                auto *payloadPtr = B.CreateInBoundsGEP(ST, vmap[val], {zero, payIdx}, "match.payload.addr");
                auto *i8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(llctx));
                auto *rawPayloadPtr = B.CreateBitCast(payloadPtr, i8PtrTy, "match.raw");
                if (auto vfieldsIt = C.sum_variant_field_types.find(sname); vfieldsIt != C.sum_variant_field_types.end()) {
                    auto &variants = vfieldsIt->second; int tag = cases[ci].tag;
                    if (tag >=0 && (size_t)tag < variants.size()) {
                        auto &fields = variants[tag];
                        for (auto &bp : cases[ci].binds) {
                            size_t idx = bp.second; if (idx >= fields.size()) continue;
                            uint64_t offset = 0; for (size_t fi=0; fi<idx; ++fi) { llvm::Type* fl = C.S.map_type(fields[fi]); offset += edn::ir::size_in_bytes(C.S.module, fl); }
                            llvm::Value* offVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(llctx), offset);
                            llvm::Value* fieldRaw = B.CreateInBoundsGEP(llvm::Type::getInt8Ty(llctx), rawPayloadPtr, offVal, bp.first + ".raw");
                            llvm::Type* fieldLL = C.S.map_type(fields[idx]);
                            auto *fieldPtrTy = llvm::PointerType::getUnqual(fieldLL);
                            auto *typedPtr = B.CreateBitCast(fieldRaw, fieldPtrTy, bp.first + ".ptr");
                            auto *lv = B.CreateLoad(fieldLL, typedPtr, bp.first);
                            vmap[bp.first] = lv; vtypes[bp.first] = fields[idx];
                        }
                    }
                }
            }
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->pushLexicalBlock(/*line*/1,1,&B);
            C.emit_ref(cases[ci].body);
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->popScope(&B);
            if (resultMode) {
                std::string vnm = cases[ci].valueVar; if (!vnm.empty() && vmap.count(vnm)) incomings.push_back({vmap[vnm], bodyBB});
                else if (resultTy) incomings.push_back({llvm::UndefValue::get(C.S.map_type(resultTy)), bodyBB});
            }
            if (!bodyBB->getTerminator()) B.CreateBr(mergeBB);
        }
        if (haveDefault) {
            B.SetInsertPoint(defaultBB);
            std::vector<node_ptr> defaultBody; std::string defaultValueVar;
            if (std::holds_alternative<vector_t>(defaultNode->data)) {
                auto &ve = std::get<vector_t>(defaultNode->data).elems; defaultBody.reserve(ve.size());
                for (size_t di=0; di<ve.size(); ++di) { auto &dn = ve[di]; if (dn && std::holds_alternative<keyword>(dn->data)) { std::string kw = std::get<keyword>(dn->data).name; if (kw=="value" && di+1<ve.size() && ve[di+1] && std::holds_alternative<symbol>(ve[di+1]->data)) { defaultValueVar = trimPct(symName(ve[di+1])); ++di; continue; } } defaultBody.push_back(dn);} }
            else if (std::holds_alternative<list>(defaultNode->data)) {
                auto &dl = std::get<list>(defaultNode->data).elems; size_t diStart=0; if (!dl.empty() && std::holds_alternative<symbol>(dl[0]->data) && std::get<symbol>(dl[0]->data).name=="default") diStart=1; for (size_t di=diStart; di<dl.size(); ++di) { if (!dl[di] || !std::holds_alternative<keyword>(dl[di]->data)) break; std::string kw = std::get<keyword>(dl[di]->data).name; if (++di >= dl.size()) break; auto valn = dl[di]; if (kw=="body" && valn && std::holds_alternative<vector_t>(valn->data)) { auto &ve = std::get<vector_t>(valn->data).elems; defaultBody.reserve(ve.size()); for (size_t bj=0; bj<ve.size(); ++bj) { auto &bn = ve[bj]; if (bn && std::holds_alternative<keyword>(bn->data)) { std::string kw2 = std::get<keyword>(bn->data).name; if (kw2=="value" && bj+1<ve.size() && ve[bj+1] && std::holds_alternative<symbol>(ve[bj+1]->data)) { defaultValueVar = trimPct(symName(ve[bj+1])); ++bj; continue; } } defaultBody.push_back(bn);} } else if (kw=="value" && valn && std::holds_alternative<symbol>(valn->data)) { defaultValueVar = trimPct(symName(valn)); } }
            }
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->pushLexicalBlock(/*line*/1,1,&B);
            C.emit_ref(defaultBody);
            if (C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
                C.S.debug_manager->popScope(&B);
            if (resultMode) {
                if (!defaultValueVar.empty() && vmap.count(defaultValueVar)) incomings.push_back({vmap[defaultValueVar], defaultBB});
                else if (resultTy) incomings.push_back({llvm::UndefValue::get(C.S.map_type(resultTy)), defaultBB});
            }
            if (!defaultBB->getTerminator()) B.CreateBr(mergeBB);
        }
        B.SetInsertPoint(mergeBB);
        if (resultMode && resultTy) {
            auto *phi = B.CreatePHI(C.S.map_type(resultTy), (unsigned)incomings.size(), dstName);
            for (auto &inc : incomings) phi->addIncoming(inc.val, inc.pred);
            vmap[dstName] = phi; vtypes[dstName] = resultTy;
        }
        return true;
    }

    // break -----------------------------------------------------------------
    if (isOp(il, "break")) {
        if (!C.loopEndStack.empty() && !B.GetInsertBlock()->getTerminator())
            B.CreateBr(C.loopEndStack.back());
        return true; // terminator
    }

    // continue ---------------------------------------------------------------
    if (isOp(il, "continue")) {
        if (!C.loopContinueStack.empty() && !B.GetInsertBlock()->getTerminator())
            B.CreateBr(C.loopContinueStack.back());
        return true; // terminator
    }

    return false; // not ours
}

}
