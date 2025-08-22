#pragma once

#include <vector>
#include <string>
#include <functional>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>

#include "edn/edn.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::control_ops {

// Context passed to control-flow handlers, bundling required references
struct Context {
    builder::State& S;
    int& cfCounter;
    llvm::Function* F; // current function being built
    std::function<void(const std::vector<edn::node_ptr>&)> emit_ref; // recursive emitter for nested vectors
    std::function<llvm::Value*(const std::string&)> eval_defined; // recompute some SSA values
    std::function<llvm::Value*(const edn::node_ptr&)> get_val;    // resolve arbitrary node
    std::vector<llvm::BasicBlock*>& loopEndStack;
    std::vector<llvm::BasicBlock*>& loopContinueStack;
    const std::unordered_map<std::string, std::unordered_map<std::string,int>>& sum_variant_tag; // for match
    const std::unordered_map<std::string, std::vector<std::vector<edn::TypeId>>>& sum_variant_field_types; // for match binds
};

// Attempt to handle one instruction list (il) as a control-flow construct.
// Supported ops (initial extraction slice): if, while, for, switch, match, break, continue.
// Returns true if handled/emitted.
bool handle(Context& C, const std::vector<edn::node_ptr>& il);

}
