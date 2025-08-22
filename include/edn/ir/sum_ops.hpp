#pragma once

#include <vector>
#include <string>
#include <unordered_map>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir/builder.hpp"

namespace edn::ir::sum_ops {

// Handlers for sum-new, sum-is, sum-get.
bool handle_sum_new(builder::State& S, const std::vector<edn::node_ptr>& il,
                    const std::unordered_map<std::string, std::unordered_map<std::string,int>>& sum_variant_tag,
                    const std::unordered_map<std::string, std::vector<std::vector<edn::TypeId>>>& sum_variant_field_types);
bool handle_sum_is(builder::State& S, const std::vector<edn::node_ptr>& il,
                   const std::unordered_map<std::string, std::unordered_map<std::string,int>>& sum_variant_tag);
bool handle_sum_get(builder::State& S, const std::vector<edn::node_ptr>& il,
                    const std::unordered_map<std::string, std::unordered_map<std::string,int>>& sum_variant_tag,
                    const std::unordered_map<std::string, std::vector<std::vector<edn::TypeId>>>& sum_variant_field_types);

}
