#pragma once

#include "edn/edn.hpp"
#include <vector>

namespace edn { class IREmitter; }
namespace edn { namespace ir { namespace collect {

// Prepass to register structs/unions/sums and emit globals.
// Ensures struct bodies are set before any DataLayout queries in later stages.
void run(const std::vector<edn::node_ptr>& top, edn::IREmitter* emitter);

}}} // namespace edn::ir::collect
