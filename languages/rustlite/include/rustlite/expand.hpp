#pragma once
#include "edn/edn.hpp"

namespace rustlite {

// Expand rustlite-specific syntactic sugar into core EDN forms.
// Currently supports:
//  - (rif-let SumType Variant %ptr :bind %x :then [ ... ] :else [ ... ])
//      -> (match SumType %ptr :cases [ (case Variant :binds [ (bind %x 0) ] :body [ ... ]) ] :default [ ... ])
edn::node_ptr expand_rustlite(const edn::node_ptr& module_ast);

} // namespace rustlite
