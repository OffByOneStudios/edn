#pragma once

#include "edn/ir/context.hpp"

#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>

namespace edn::ir::exceptions {

// Choose and declare the EH personality function for the module based on EmitEnv.
// Returns nullptr if no personality was requested.
// Policy: If both personalities are somehow requested, Itanium wins on non-Windows,
// otherwise SEH is used. Callers attach the returned constant to functions as needed.
llvm::Constant* select_personality(llvm::Module& M, const edn::EmitEnv& env);

// Create a minimal Itanium landingpad basic block that resumes immediately.
// Used for panic fallback when no active try is present.
// Returns the created landingpad basic block (in the given function).
llvm::BasicBlock* create_panic_cleanup_landingpad(llvm::Function* F, llvm::IRBuilder<>& refBuilder);

// Create a catch-all Itanium landingpad and branch to the provided handlerBB.
// Returns the created landingpad basic block.
llvm::BasicBlock* create_catch_all_landingpad(llvm::Function* F,
											  llvm::IRBuilder<>& refBuilder,
											  llvm::BasicBlock* handlerBB);

// Ensure a reusable Windows SEH cleanup funclet exists; creates one if needed.
// If existingCleanupBB is non-null, it's returned unchanged; otherwise a new
// cleanuppad/cleanupret block is created and returned.
llvm::BasicBlock* ensure_seh_cleanup(llvm::Function* F,
									 llvm::IRBuilder<>& refBuilder,
									 llvm::BasicBlock* existingCleanupBB);

// Create the SEH catch scaffolding for a catch-all try: a catchswitch in a
// dispatch block with a single catchpad leading into catchPadBB. The catchswitch
// unwinds to contBB.
struct SEHCatchScaffold {
	llvm::BasicBlock* dispatchBB;   // contains catchswitch
	llvm::BasicBlock* catchPadBB;   // where catchpad is placed
	llvm::CatchPadInst* catchPad;   // the created catchpad
};

SEHCatchScaffold create_seh_catch_scaffold(llvm::Function* F,
										   llvm::IRBuilder<>& refBuilder,
										   llvm::BasicBlock* contBB);

} // namespace edn::ir::exceptions
