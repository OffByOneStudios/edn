#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace rustlite::pegtl_front {

// Shared state and helpers moved out of the monolithic parser.cpp
struct build_state {
    struct fn_emit {
        std::string name;
        std::vector<std::string> body;
        bool has_zero{false};
        std::string zero_sym{"%__rl_zero"};
        int tmp_counter{0};
        std::string gensym(const char* base){ return std::string("%__rl_") + base + std::to_string(++tmp_counter); }
        std::vector<std::vector<std::string>> block_stack;
        std::vector<std::vector<std::string>> block_results;
        std::vector<std::string>& sink(){ return block_stack.empty()? body : block_stack.back(); }
        std::vector<std::string> cond_results;
        std::vector<std::pair<std::string,std::string>> params;
        std::string ret_type{"i32"};
        bool saw_ret{false};
    };
    std::vector<fn_emit> fns;
    fn_emit* current(){ return fns.empty()? nullptr : &fns.back(); }
};

inline void ltrim(const std::string& s, size_t& i){ while(i<s.size() && isspace((unsigned char)s[i])) ++i; }
inline std::string parse_ident(const std::string& s, size_t& i){ size_t b=i; if(b<s.size() && (isalpha((unsigned char)s[b])||s[b]=='_')){ ++i; while(i<s.size()){ char c=s[i]; if(isalnum((unsigned char)c)||c=='_') ++i; else break; } return s.substr(b, i-b);} return std::string(); }

// Forward for expression lowering used by many actions
std::pair<std::string,std::string> lower_simple_expr_into(build_state::fn_emit& fn, const std::string& expr_src);

} // namespace rustlite::pegtl_front
