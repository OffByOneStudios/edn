#pragma once
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/di.hpp"
#include "edn/ir/builder.hpp"
#include "edn/ir/resolver.hpp"

namespace edn::ir::function_builder {
struct FunctionBuildInputs {
    std::string name;
    edn::TypeId retType{};
    std::vector<std::pair<std::string, edn::TypeId>> params; // (name, type)
    bool isVariadic = false;
    bool isExternal = false;
};

struct CreatedFunction {
    llvm::Function* function = nullptr;
    edn::ir::builder::State* state = nullptr; // owned by this object
};

// Creates (and if non-external, prepares entry block + builder State) for a function.
// On debug builds attaches DISubprogram & parameter variable info.
CreatedFunction create_function(edn::TypeContext& tctx,
                                std::function<llvm::Type*(edn::TypeId)> map_type,
                                llvm::Module& module,
                                llvm::LLVMContext& llctx,
                                edn::ir::debug::DebugManager* dbg,
                                const FunctionBuildInputs& in,
                                llvm::Function* personality,
                                bool enableCoro,
                                bool enableEHItanium,
                                bool enableEHSEH);
}
