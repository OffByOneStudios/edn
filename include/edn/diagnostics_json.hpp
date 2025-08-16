// diagnostics_json.hpp - JSON serialization for TypeCheckResult (Phase 3 diagnostics)
#pragma once
#include "edn/type_check.hpp"
#include <string>

namespace edn {

// Escape a string for safe JSON output.
std::string json_escape(const std::string& s);

// Serialize diagnostics to a compact JSON string.
std::string diagnostics_to_json(const TypeCheckResult& r);

// If EDN_DIAG_JSON=1 in the environment, print diagnostics JSON to stderr.
void maybe_print_json(const TypeCheckResult& r);

} // namespace edn
