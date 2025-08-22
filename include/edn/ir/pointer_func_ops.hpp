#pragma once

#include <vector>
#include <string>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::pointer_func_ops {

// addr: (addr %dst (ptr <T>) %src)
bool handle_addr(builder::State& S, const std::vector<edn::node_ptr>& il,
                 std::function<llvm::AllocaInst*(const std::string&, edn::TypeId, bool)> ensureSlot);

// deref: (deref %dst <T> %ptr)
bool handle_deref(builder::State& S, const std::vector<edn::node_ptr>& il);

// fnptr: (fnptr %dst (ptr (fn-type ...)) Name)
bool handle_fnptr(builder::State& S, const std::vector<edn::node_ptr>& il);

// call-indirect: (call-indirect %dst <ret> %fptr %args...)
bool handle_call_indirect(builder::State& S, const std::vector<edn::node_ptr>& il);

}
