// NOTE: This header was force-rewritten to eliminate any lingering inline definitions
// of resolver functions (get_value, etc.) that previously caused ODR/redefinition
// issues when an older cached build still saw inline wrappers. If you see duplicates
// again, ensure your build directory is fully regenerated.
#pragma once

#include <string>
#include <string_view>
#include <optional>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>

#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::resolver {

// Ensure a stack slot exists for name (alloca in entry). Optionally initialize from existing SSA.
llvm::AllocaInst* ensure_slot(builder::State& S, const std::string& name, edn::TypeId ty, bool initFromCurrent);

// Look up a value by EDN node (out-of-line defined in resolver.cpp). Declaration only, no inline body.
llvm::Value* get_value(builder::State& S, const edn::node_ptr& n);

// Recompute value from defining node (subset of ops) mirroring legacy evalDefined.
llvm::Value* eval_defined(builder::State& S, const std::string& name);

// Bind (name -> value) with declared type (SSA or slot pointer). Overwrites current visible binding.
void bind_value(builder::State& S, const std::string& name, llvm::Value* v, edn::TypeId ty);

} // namespace edn::ir::resolver
