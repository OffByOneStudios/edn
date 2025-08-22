#pragma once

#include <vector>
#include <string>
#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::return_ops {

struct Context {
    builder::State& S;
    llvm::Function* currentFunction;
    llvm::FunctionType* fty;
    bool& functionDone;
};

bool handle(Context C, const std::vector<edn::node_ptr>& il);

} // namespace edn::ir::return_ops
