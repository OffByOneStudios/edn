#pragma once
#include "../prelude.hpp"
#include "../grammar.hpp"
#include <tao/pegtl.hpp>

namespace rustlite::pegtl_front::actions {
using namespace tao::pegtl;
using rustlite::pegtl_front::build_state;

template<typename Rule>
struct action : nothing<Rule> {};

// Ensure we always have a function to sink free-standing statements, like the legacy behavior
template<> struct action< grammar::module_rule > {
    template<typename Input>
    static void apply(const Input&, build_state& st){
        if(st.fns.empty()){
            build_state::fn_emit f; f.name = "__surface"; st.fns.push_back(std::move(f));
        }
    }
};

} // namespace rustlite::pegtl_front::actions
