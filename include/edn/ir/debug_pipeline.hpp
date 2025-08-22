#pragma once

#include <memory>
#include <llvm/IR/Module.h>

#include "edn/ir/debug.hpp"

namespace edn::ir::debug_pipeline {

// Finalize debug info if enabled and DIBuilder present.
inline void finalize_debug(std::shared_ptr<edn::ir::debug::DebugManager>& dbg, bool enable){
    if(enable && dbg && dbg->DIB){ dbg->DIB->finalize(); }
}

// Run optional optimization / custom pass pipeline based on env vars:
//   EDN_ENABLE_PASSES=1 enables running
//   EDN_PASS_PIPELINE textual pipeline overrides presets if set
//   EDN_OPT_LEVEL (0/1/2/3) selects preset when no custom pipeline
//   EDN_VERIFY_IR=1 adds verification before/after
void run_pass_pipeline(llvm::Module& M);

} // namespace edn::ir::debug_pipeline
