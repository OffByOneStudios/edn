#pragma once

#include <vector>
#include <string>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::cast_ops {

// Handles LLVM-like cast ops: zext, sext, trunc, bitcast, sitofp, uitofp, fptosi, fptoui, ptrtoint, inttoptr
// Form: (<op> %dst <to-type> %src) => il.size()==4
bool handle(builder::State& S, const std::vector<edn::node_ptr>& il);

} // namespace edn::ir::cast_ops
