#include "edn/ir/flow_ops.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

using namespace edn;

namespace edn::ir::flow_ops {

static std::string symName(const node_ptr &n){ if(!n) return {}; if(std::holds_alternative<symbol>(n->data)) return std::get<symbol>(n->data).name; return {}; }
static std::string trimPct(const std::string &s){ if(!s.empty() && s[0]=='%') return s.substr(1); return s; }

bool handle_block(Context &C, const std::vector<node_ptr>& il, std::vector<std::pair<llvm::AllocaInst*, TypeId>> &localSlots) {
    if(il.empty() || !il[0] || !std::holds_alternative<symbol>(il[0]->data)) return false;
    if(std::get<symbol>(il[0]->data).name != "block") return false;
    node_ptr localsNode=nullptr, bodyNode=nullptr;
    for(size_t i=1;i<il.size();++i){
        if(!il[i] || !std::holds_alternative<keyword>(il[i]->data)) break;
        std::string kw = std::get<keyword>(il[i]->data).name;
        if(++i>=il.size()) break; auto val = il[i];
        if(kw=="locals" && val && std::holds_alternative<vector_t>(val->data)) localsNode = val;
        else if(kw=="body" && val && std::holds_alternative<vector_t>(val->data)) bodyNode = val;
    }
    if(!localsNode && !bodyNode) return true; // accept empty
    // Push new lexical scope for debug (if enabled) via builder state debug_manager
    if(C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
    C.S.debug_manager->pushLexicalBlock(1,1,&C.S.builder);
    ++C.S.lexicalDepth;
    // Emit locals: (local <type> %name)
    if(localsNode){
        for(auto &ln : std::get<vector_t>(localsNode->data).elems){
            if(!ln || !std::holds_alternative<list>(ln->data)) continue; auto &ll = std::get<list>(ln->data).elems;
            if(ll.size()!=3) continue; if(!std::holds_alternative<symbol>(ll[0]->data) || std::get<symbol>(ll[0]->data).name != "local") continue;
            try {
                TypeId ty = C.S.tctx.parse_type(ll[1]);
                std::string nm = trimPct(symName(ll[2])); if(nm.empty()) continue;
                auto *alloca = C.S.builder.CreateAlloca(C.S.map_type(ty), nullptr, nm + ".slot");
                C.S.varSlots[nm] = alloca; C.S.vtypes[nm] = ty; localSlots.emplace_back(alloca, ty);
            } catch(...) { continue; }
        }
    }
    if(bodyNode){
        C.emit_ref(std::get<vector_t>(bodyNode->data).elems);
    }
    if(C.S.debug_manager && C.S.debug_manager->enableDebugInfo)
        C.S.debug_manager->popScope(&C.S.builder);
    return true;
}

}
