#pragma once

#include <vector>
#include <string>
#include <unordered_map>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::closure_ops {

// (closure %dst (ptr (fn-type ...)) Callee [ %env ])
bool handle_closure(builder::State& S, const std::vector<edn::node_ptr>& il,
                    const std::vector<edn::node_ptr>& top, size_t& cfCounter);

// (make-closure %dst Callee [ %env ])
bool handle_make_closure(builder::State& S, const std::vector<edn::node_ptr>& il,
                         const std::vector<edn::node_ptr>& top);

// (call-closure %dst <ret> %clos %args...)
bool handle_call_closure(builder::State& S, const std::vector<edn::node_ptr>& il);

}
