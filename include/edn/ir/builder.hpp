#pragma once

#include <string>
#include <unordered_map>
#include <functional>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/debug.hpp"

namespace edn::ir::builder {

// Minimal shared state for instruction helpers. Holds references to the current
// IRBuilder and the local SSA/slot maps in the main emitter.
struct State {
    llvm::IRBuilder<>& builder;
    llvm::LLVMContext& llctx;
    llvm::Module& module;
    edn::TypeContext& tctx; // available if needed by callers
    std::function<llvm::Type*(edn::TypeId)> map_type; // supplied by IREmitter
    std::unordered_map<std::string, llvm::Value*>& vmap;
    std::unordered_map<std::string, edn::TypeId>& vtypes;
    std::unordered_map<std::string, llvm::AllocaInst*>& varSlots;
    std::unordered_map<std::string, std::string>& initAlias;
    std::unordered_map<std::string, edn::node_ptr>& defNode;

    std::shared_ptr<edn::ir::debug::DebugManager> debug_manager;

    // --- Shadowing support --------------------------------------------------
    // Current lexical depth (0 = function body). Managed in tandem with DebugManager scope pushes.
    int lexicalDepth = 0;
    // For each variable name, stack of shadowed allocas/values with the depth they were declared.
    struct ShadowEntry { int depth; llvm::AllocaInst* slot; edn::TypeId ty; };
    std::unordered_map<std::string, std::vector<ShadowEntry>> shadowSlots;
};

// Unwind shadow slots when leaving a lexical scope (after State.lexicalDepth already decremented)
inline void unwind_scope(State& S){
    int depth = S.lexicalDepth;
    for(auto it = S.shadowSlots.begin(); it != S.shadowSlots.end();){
        auto &vec = it->second;
        while(!vec.empty() && vec.back().depth > depth) vec.pop_back();
        if(vec.empty()){
            S.varSlots.erase(it->first);
            S.vmap.erase(it->first);
            S.vtypes.erase(it->first);
            it = S.shadowSlots.erase(it);
        } else {
            // Restore latest visible shadow
            auto &ent = vec.back();
            S.varSlots[it->first] = ent.slot;
            S.vmap[it->first] = ent.slot; // pointer (alloca)
            S.vtypes[it->first] = S.tctx.get_pointer(ent.ty);
            ++it;
        }
    }
}

// Forward declarations (implemented in resolver).
} // namespace edn::ir::builder

namespace edn::ir::resolver {
    // Forward declarations only (implemented out-of-line in resolver.cpp). No inline bodies here.
    llvm::Value* get_value(edn::ir::builder::State& S, const edn::node_ptr& n);
    llvm::Value* eval_defined(edn::ir::builder::State& S, const std::string& name);
}

namespace edn::ir::builder {

// Transitional wrappers removed: call edn::ir::resolver::get_value / eval_defined directly.
llvm::Value* resolve_preferring_slots(State& S, const edn::node_ptr& n);

} // namespace edn::ir::builder
