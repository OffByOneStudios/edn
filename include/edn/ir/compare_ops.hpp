#pragma once

#include <vector>
#include <string>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::compare_ops {

// Integer compare via direct op names: eq/ne/lt/gt/le/ge (legacy simple form). il.size()==5.
// Sets defNode for resulting SSA value.
bool handle_int_simple(builder::State& S, const std::vector<edn::node_ptr>& il, const edn::node_ptr& inst);

// Integer compare with explicit predicate: (icmp %dst :pred <pred> %a %b) il.size()==7.
bool handle_icmp(builder::State& S, const std::vector<edn::node_ptr>& il);

// Floating-point compare with predicate: (fcmp %dst :pred <pred> %a %b) il.size()==7.
bool handle_fcmp(builder::State& S, const std::vector<edn::node_ptr>& il);

} // namespace edn::ir::compare_ops
