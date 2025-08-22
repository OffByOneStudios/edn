// di.hpp - Debug Info (DI) helper module skeleton
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DebugInfoMetadata.h>

#include "edn/types.hpp"
#include "edn/ir/debug.hpp"

namespace edn::ir::di {

// Attach a DISubprogram to the function if debug is enabled.
// Returns the created (or existing) DISubprogram; nullptr when debug disabled.
llvm::DISubprogram* attach_function_debug(debug::DebugManager& dbg,
                                          llvm::Function& F,
                                          const std::string& fname,
                                          edn::TypeId retTy,
                                          const std::vector<std::pair<std::string, edn::TypeId>>& params,
                                          unsigned lineNo);

// Emit parameter debug info (dbg.value intrinsics) at function entry.
// vtypes maps parameter names to their EDN TypeId.
void emit_parameter_debug(debug::DebugManager& dbg,
                          llvm::Function& F,
                          llvm::IRBuilder<>& builder,
                          const std::unordered_map<std::string, edn::TypeId>& vtypes);

// Declare a local variable (slot-backed or SSA) with a dbg.declare/dbg.value style intrinsic.
// For now, we only provide a skeleton; implementation can be expanded later.
void declare_local(debug::DebugManager& dbg,
                   llvm::IRBuilder<>& builder,
                   llvm::Value* value,
                   const std::string& name,
                   edn::TypeId ty,
                   unsigned lineNo);

} // namespace edn::ir::di
