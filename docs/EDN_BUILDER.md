# EDN Builder Ergonomics

This library provides ergonomic helpers to build EDN ASTs directly in C++ and serialize them later. These helpers avoid string concatenation and enable safe composition.

Key additions (2025-08-18):
- Factory helpers: `edn::n_sym`, `edn::n_kw`, `edn::n_str`, `edn::n_i64`, `edn::n_f64`, `edn::n_bool`
- Collection builders: `edn::node_list`, `edn::node_vec`, `edn::node_map`, `edn::node_set`
- Stream-style append: `operator<<` on lists/vectors/sets/maps and containers of `node_ptr`

Example: build a simple module with one function using the helpers:

```cpp
#include <edn/edn.hpp>

edn::node_ptr make_module(){
  using namespace edn;
  auto mod = node_list({ n_sym("module") });
  auto params = node_vec({ node_list({ n_sym("param"), n_sym("i32"), n_sym("%a") }),
                           node_list({ n_sym("param"), n_sym("i32"), n_sym("%b") }) });
  auto body = node_vec({ node_list({ n_sym("const"), n_sym("%t"), n_sym("i32"), n_i64(1) }),
                         node_list({ n_sym("ret"), n_sym("i32"), n_sym("%t") }) });
  auto fn = node_list({ n_sym("fn"), n_kw("name"), n_str("add"), n_kw("ret"), n_sym("i32"),
                        n_kw("params"), params, n_kw("body"), body });
  mod << fn; // append to module
  return mod;
}

// Later
std::string text = edn::to_string(make_module());
```

Rustlite Builder now uses these APIs internally, storing an `edn::node_ptr` and serializing at the end. See `languages/rustlite/include/rustlite/rustlite.hpp` for details.
