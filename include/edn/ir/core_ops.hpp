#pragma once

#include <vector>
#include <string>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::core_ops {

// Returns true if the instruction vector 'il' was recognized and emitted.
bool handle_integer_arith(builder::State& S, const std::vector<edn::node_ptr>& il);
bool handle_float_arith(builder::State& S, const std::vector<edn::node_ptr>& il);
bool handle_bitwise_shift(builder::State& S, const std::vector<edn::node_ptr>& il, const edn::node_ptr& inst);
bool handle_ptr_add_sub(builder::State& S, const std::vector<edn::node_ptr>& il);
bool handle_ptr_diff(builder::State& S, const std::vector<edn::node_ptr>& il);

}
