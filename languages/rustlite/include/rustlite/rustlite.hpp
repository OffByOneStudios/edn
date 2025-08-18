#pragma once
#include <string>
#include <vector>
#include <optional>
#include <utility>
#include "edn/edn.hpp"

namespace rustlite {

// Extremely small Rust-like surface: fn, let, match on an enum-like sum.
// We lower to EDN by emitting EDN S-exprs and calling into the core edn pipeline.

struct EmitOptions {
    bool enable_debug = false;
};

struct Program {
    // Raw EDN text we generate via macros/transforms; kept simple for bootstrap.
    std::string edn_text;
};

// Simple builder API that appends EDN forms. In a real impl we'd parse a surface language.
class Builder {
public:
    Builder(){ root_ = edn::node_list({ edn::n_sym("module") }); }
    Builder& begin_module(){ /* already started with (module ...); no-op for compatibility */ return *this; }
    Builder& end_module(){ /* no-op; build() serializes the current AST */ return *this; }

    // Define a sum type (enum)
    Builder& sum_enum(const std::string& name, const std::vector<std::pair<std::string,std::vector<std::string>>>& variants);

    // Define a struct type via rustlite macro rstruct
    Builder& rstruct(const std::string& name, const std::vector<std::pair<std::string,std::string>>& fields);

    // Define a function with a body of raw EDN instructions (already using macro shapes where needed)
    Builder& fn_raw(const std::string& name, const std::string& ret_type, const std::vector<std::pair<std::string,std::string>>& params, const std::string& body_ir_vec);

    Program build() const { return Program{ edn::to_string(root_) }; }
private:
    edn::node_ptr root_;
};

} // namespace rustlite
