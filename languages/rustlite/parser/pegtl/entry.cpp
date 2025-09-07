#include "../parser.hpp"
#include "prelude.hpp"
#include "grammar.hpp"
#include "actions/all.hpp"
#include <tao/pegtl.hpp>

namespace rustlite {
using namespace rustlite::pegtl_front;
using namespace rustlite::pegtl_front::grammar;
using namespace rustlite::pegtl_front::actions;

ParseResult Parser::parse_string(std::string_view src, std::string_view filename) const {
    tao::pegtl::memory_input in(src, std::string(filename));
    build_state st;
    try {
        tao::pegtl::parse< module_rule, action >(in, st);
        ParseResult r; r.success=true; r.edn = "[]"; return r;
    } catch (const tao::pegtl::parse_error& e) {
        auto p = e.positions().front();
        ParseResult r; r.success=false; r.error_message = e.what(); r.line = static_cast<int>(p.line); r.column = static_cast<int>(p.column); return r;
    }
}

} // namespace rustlite
