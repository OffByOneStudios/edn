#pragma once
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "edn/transform.hpp" // for edn::Transformer

namespace rustlite {

// Shared state captured by certain macros (enum variant counts, tuple arities, etc.).
struct MacroContext {
    std::unordered_map<std::string,size_t> enumVariantCounts; // for ematch exhaustiveness
    std::unordered_map<std::string,size_t> tupleVarArity;     // %var -> arity for tuple members
    std::unordered_set<size_t> tupleArities;                  // distinct tuple arities encountered
};

// Grouped registration functions. Each installs a related set of macros.
void register_literal_macros(edn::Transformer&, const std::shared_ptr<MacroContext>&);
void register_extern_macros(edn::Transformer&, const std::shared_ptr<MacroContext>&);
void register_var_control_macros(edn::Transformer&, const std::shared_ptr<MacroContext>&);
void register_sum_enum_macros(edn::Transformer&, const std::shared_ptr<MacroContext>&);
void register_closure_macros(edn::Transformer&, const std::shared_ptr<MacroContext>&);
void register_struct_trait_macros(edn::Transformer&, const std::shared_ptr<MacroContext>&);
void register_field_index_macros(edn::Transformer&, const std::shared_ptr<MacroContext>&);
void register_tuple_array_macros(edn::Transformer&, const std::shared_ptr<MacroContext>&);
void register_assert_macros(edn::Transformer&, const std::shared_ptr<MacroContext>&);
void register_call_macros(edn::Transformer&, const std::shared_ptr<MacroContext>&);
void register_alias_macros(edn::Transformer&, const std::shared_ptr<MacroContext>&);

} // namespace rustlite
