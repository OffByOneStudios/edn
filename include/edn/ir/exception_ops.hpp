#pragma once

#include <vector>
#include <string>
#include <functional>
#include <memory>

#include "edn/edn.hpp"
#include "edn/ir/builder.hpp"
#include "edn/ir/exceptions.hpp"

namespace edn::ir::exception_ops {

struct Context {
    builder::State& S;                      // shared value/type maps etc.
    llvm::IRBuilder<>& builder;             // current IRBuilder
    llvm::LLVMContext& llctx;               // LLVM context
    llvm::Module& module;                   // module
    llvm::Function* F;                      // current function
    bool enableDebugInfo;                   // debug flag
    bool panicUnwind;                       // whether panic should unwind instead of trap
    bool enableEHItanium;                   // Itanium model enabled
    bool enableEHSEH;                       // SEH model enabled
    llvm::Constant* selectedPersonality;    // personality (may be null)
    size_t& cfCounter;                      // counter for unique block naming
    std::vector<llvm::BasicBlock*>& sehExceptTargetStack; // active SEH exception targets
    std::vector<llvm::BasicBlock*>& itnExceptTargetStack; // active Itanium exception targets
    llvm::BasicBlock*& sehCleanupBB;        // shared SEH cleanup funclet block
    std::function<void(const std::vector<edn::node_ptr>&)> emit_nested; // emit nested instruction lists
};

// Handle a (panic) instruction. Returns true if handled.
bool handle_panic(Context& C, const std::vector<edn::node_ptr>& il);

// Handle a (try :body [...] :catch [...]) form. Returns true if handled.
bool handle_try(Context& C, const std::vector<edn::node_ptr>& il);

} // namespace edn::ir::exception_ops
