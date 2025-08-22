#pragma once

#include <vector>
#include <string>
#include <functional>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include "edn/edn.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::flow_ops {

// Handles simple lexical block constructs currently still in edn.cpp
// (block :locals [ (local <ty> %name)... ] :body [ ... ])
struct Context {
    builder::State &S;
    llvm::Function *F;
    std::function<void(const std::vector<edn::node_ptr>&)> emit_ref; // recurse into body vectors
};

// Returns true if handled.
bool handle_block(Context &C, const std::vector<edn::node_ptr>& il, std::vector<std::pair<llvm::AllocaInst*, edn::TypeId>> &localSlots);

}
