#pragma once

#include <vector>
#include <string>
#include <llvm/IR/Value.h>

#include "edn/edn.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::coro_ops {

// Handle any (coro-*) instruction. Returns true if handled.
// lastCoroIdTok is updated when a coro.id token is produced (via coro-begin sequence).
bool handle(builder::State& S,
            const std::vector<edn::node_ptr>& il,
            bool enableCoro,
            llvm::Value*& lastCoroIdTok);

}
