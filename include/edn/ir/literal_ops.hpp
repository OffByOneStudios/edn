#pragma once

#include <vector>
#include <string>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::literal_ops {

// Handle string/byte literal pointer ops which synthesize (or reuse) internal constant
// globals and return an i8* pointer to the first element.
// Forms:
//   (cstr %dst "literal")          -> %dst : (ptr i8) to null-terminated bytes
//   (bytes %dst [ b0 b1 ... ])      -> %dst : (ptr i8) to raw byte sequence (no terminator)
// Returns true if handled.
bool handle_cstr(builder::State& S, const std::vector<edn::node_ptr>& il);
bool handle_bytes(builder::State& S, const std::vector<edn::node_ptr>& il);

}
