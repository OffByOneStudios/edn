#pragma once

#include <vector>
#include <string>
#include <functional>
#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::call_ops {

struct Context {
    builder::State& S;
    llvm::Function* currentFunction; // F
    std::vector<llvm::BasicBlock*>& sehExceptTargetStack;
    std::vector<llvm::BasicBlock*>& itnExceptTargetStack;
    llvm::BasicBlock*& sehCleanupBB;
    bool enableEHItanium;
    bool enableEHSEH;
    bool panicUnwind;
    llvm::Constant* selectedPersonality;
    size_t& cfCounter;
    const std::vector<edn::node_ptr>& topLevel; // for forward decl header scan
    std::function<llvm::Value*(const edn::node_ptr&)> getVal;
};

// Dispatch handler for call-like IR forms: (call %dst <retTy> %callee %arg1 ...)
bool handle_call(Context C, const std::vector<edn::node_ptr>& il);

// Handle varargs helpers (currently placeholders): va-start, va-arg, va-end
bool handle_varargs(Context C, const std::vector<edn::node_ptr>& il);

} // namespace edn::ir::call_ops
