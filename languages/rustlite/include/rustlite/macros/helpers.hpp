// Shared small helper utilities for rustlite macro source files.
#pragma once
#include "edn/edn.hpp"
#include <memory>
#include <string>

namespace rustlite {
using edn::node; using edn::node_ptr; using edn::symbol; using edn::keyword;

inline node_ptr rl_make_sym(const std::string& s){ return std::make_shared<node>( node{ symbol{s}, {} } ); }
inline node_ptr rl_make_kw(const std::string& s){ return std::make_shared<node>( node{ keyword{s}, {} } ); }
inline node_ptr rl_make_i64(int64_t v){ return std::make_shared<node>( node{ v, {} } ); }
inline std::string rl_gensym(const std::string& base){ static uint64_t n=0; return "%__rl_"+base+"_"+std::to_string(++n); }
}
