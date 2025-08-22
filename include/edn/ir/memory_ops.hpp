#pragma once

#include <vector>
#include <string>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::memory_ops {

// Handle variable-related memory operations and simple loads/stores/globals.
// Returns true if the instruction list 'il' was recognized and emitted.
// Covered ops (initial slice): assign, alloca, load, store, gload, gstore.
bool handle_assign(builder::State& S, const std::vector<edn::node_ptr>& il);
bool handle_alloca(builder::State& S, const std::vector<edn::node_ptr>& il);
bool handle_store(builder::State& S, const std::vector<edn::node_ptr>& il);
bool handle_gload(builder::State& S, const std::vector<edn::node_ptr>& il);
bool handle_gstore(builder::State& S, const std::vector<edn::node_ptr>& il);
bool handle_load(builder::State& S, const std::vector<edn::node_ptr>& il);
bool handle_index(builder::State& S, const std::vector<edn::node_ptr>& il);
bool handle_array_lit(builder::State& S, const std::vector<edn::node_ptr>& il);
bool handle_struct_lit(builder::State& S, const std::vector<edn::node_ptr>& il,
					   const std::unordered_map<std::string, std::unordered_map<std::string, size_t>>& struct_field_index,
					   const std::unordered_map<std::string, std::vector<edn::TypeId>>& struct_field_types);
bool handle_member(builder::State& S, const std::vector<edn::node_ptr>& il,
				   const std::unordered_map<std::string, llvm::StructType*>& struct_types,
				   const std::unordered_map<std::string, std::unordered_map<std::string, size_t>>& struct_field_index,
				   const std::unordered_map<std::string, std::vector<edn::TypeId>>& struct_field_types);
bool handle_member_addr(builder::State& S, const std::vector<edn::node_ptr>& il,
						const std::unordered_map<std::string, llvm::StructType*>& struct_types,
						const std::unordered_map<std::string, std::unordered_map<std::string, size_t>>& struct_field_index,
						const std::unordered_map<std::string, std::vector<edn::TypeId>>& struct_field_types);
bool handle_union_member(builder::State& S, const std::vector<edn::node_ptr>& il,
						 const std::unordered_map<std::string, llvm::StructType*>& struct_types,
						 const std::unordered_map<std::string, std::unordered_map<std::string, edn::TypeId>>& union_field_types);

}
