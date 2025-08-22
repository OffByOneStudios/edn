#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::phi_ops {

struct PendingPhi {
    std::string dst;
    edn::TypeId ty;
    std::vector<std::pair<std::string,std::string>> incomings; // (valueName, blockLabel)
    llvm::BasicBlock* insertBlock {nullptr};
};

// Inspect instruction list il; if it's a phi form, record a PendingPhi and return true.
bool collect(builder::State& S,
             const std::vector<edn::node_ptr>& il,
             std::vector<PendingPhi>& outPending);

// Realize all recorded PHI nodes, updating S.vmap / S.vtypes.
void finalize(builder::State& S,
              std::vector<PendingPhi>& pending,
              llvm::Function* F,
              std::function<llvm::Type*(edn::TypeId)> map_type);

}
