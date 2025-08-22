#pragma once

#include <vector>
#include <string>
#include <functional>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::variable_ops {

// (as %dst <type> %initOrLiteral)
// ensureSlot(name, ty, initFromCurrent) must allocate or return an alloca; returns true if handled.
bool handle_as(builder::State& S, const std::vector<edn::node_ptr>& il,
               std::function<llvm::AllocaInst*(const std::string&, edn::TypeId, bool)> ensureSlot,
               std::unordered_map<std::string,std::string>& initAlias,
               bool enableDebugInfo,
               llvm::Function* F,
               std::shared_ptr<edn::ir::debug::DebugManager> dbgMgr);

}
