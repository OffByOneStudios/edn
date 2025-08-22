#pragma once

#include <vector>
#include <string>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::const_ops {

// (const %dst <type> <literal>) where literal may be int64_t or double; others become undef
bool handle(builder::State& S, const std::vector<edn::node_ptr>& il);

} // namespace edn::ir::const_ops
