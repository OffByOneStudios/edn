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
};

// Resolve a node to an LLVM Value*, respecting variable slots and initializer aliases.
llvm::Value* get_value(State& S, const edn::node_ptr& n);

// Like get_value, but prefers loading from variable slots when possible, even if an SSA value exists.
llvm::Value* resolve_preferring_slots(State& S, const edn::node_ptr& n);

// Attempt to recompute a named SSA value from its defining EDN node (subset of ops).
llvm::Value* eval_defined(State& S, const std::string& name);

} // namespace edn::ir::builder
