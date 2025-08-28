#include "parser.hpp"
#include <tao/pegtl.hpp>
#include <sstream>
using namespace tao::pegtl;

namespace rustlite {

// Grammar v0.1:
// - Skip spaces and comments
// - Accept zero or more empty function items:  fn ident() { /* empty body */ }
// - Module := ws item* ws EOF
struct comment_line : seq< two<'/'>, until< eolf > > {};
struct block_comment : seq< one<'/'>, one<'*'>, until< seq< one<'*'>, one<'/'> > > > {};
struct space_or_comment : sor< space, comment_line, block_comment > {};

// tokens
struct kw_fn : string<'f','n'> {};
struct kw_return : string<'r','e','t','u','r','n'> {};
struct kw_if : string<'i','f'> {};
struct kw_else : string<'e','l','s','e'> {};
struct kw_while : string<'w','h','i','l','e'> {};
struct kw_loop : string<'l','o','o','p'> {};
struct kw_let : string<'l','e','t'> {};
struct kw_mut : string<'m','u','t'> {};
struct kw_break : string<'b','r','e','a','k'> {};
struct kw_continue : string<'c','o','n','t','i','n','u','e'> {};
struct ident_first : ranges<'a','z','A','Z','_','_'> {};
struct ident_rest : ranges<'a','z','A','Z','0','9','_','_'> {};
struct ident : seq< ident_first, star< ident_rest > > {};

template<typename Rule>
using ws = pad< Rule, space_or_comment >;
// Minimal block with optional integer literal statements: { (int ';')* }
struct lbrace : one<'{'> {};
struct rbrace : one<'}'> {};
struct lparen : one<'('> {};
struct rparen : one<')'> {};
struct semi : one<';'> {};
struct colon : one<':'> {};
struct equal : one<'='> {};
struct minus : one<'-'> {};
struct plus_tok : one<'+'> {};
struct star_tok : one<'*'> {};
struct slash_tok : one<'/'> {};
struct arrow : seq< one<'-'>, one<'>'> > {};
struct int_lit : plus< digit > {};
// Simple primary forms for statements: identifier, and optional call suffix with args
struct comma : one<','> {};
// forward decls so we can reference before definitions
struct signed_int;
struct paren_expr;
struct arg_expr : sor< ws< signed_int >, ws< ident >, ws< paren_expr > > {};
struct call_args_inner : seq< ws< arg_expr >, star< seq< ws< comma >, ws< arg_expr > > > > {};
struct call_args : seq< ws< lparen >, opt< call_args_inner >, ws< rparen > > {};
struct ident_or_call : seq< ws< ident >, opt< call_args > > {};
// signed int for expr statements: allow optional leading '-'
struct signed_int : sor< seq< ws< minus >, ws< int_lit > >, ws< int_lit > > {};
// cond/expr categories allow parentheses
struct cond_expr;
struct paren_expr : seq< ws< lparen >, ws< cond_expr >, ws< rparen > > {};
struct expr_stmt : seq< ws< cond_expr >, ws< semi > > {};
// Looser capture for return expressions: accept any text until ';'
struct expr_text : until< at< semi >, any > {};
struct return_stmt : seq< ws< kw_return >, opt< ws< expr_text > >, ws< semi > > {};
// if-statement: if <cond> block (else if <cond> block)* (else block)?
struct block_rule; // forward declaration
struct cond_expr : tao::pegtl::sor< paren_expr, signed_int, ident_or_call > {};
// Looser textual condition: consume until the next block begins. This allows operators like &&, || without full expr grammar.
struct cond_text : until< at< lbrace >, any > {};
// Wrap condition positions so actions can capture them distinctly
struct if_cond : ws< cond_text > {};
struct else_if_cond : ws< cond_text > {};
struct while_cond : ws< cond_text > {};
struct else_if_arm : seq< ws< kw_else >, ws< kw_if >, else_if_cond, block_rule > {};
struct else_tail : seq< star< else_if_arm >,
                                    opt< seq< ws< kw_else >, block_rule > > > {};
struct if_stmt : seq< ws< kw_if >, if_cond, block_rule,
                                  opt< else_tail > > {};
struct while_stmt : seq< ws< kw_while >, while_cond, block_rule > {};
struct loop_stmt : seq< ws< kw_loop >, block_rule > {};
struct break_stmt : seq< ws< kw_break >, ws< semi > > {};
struct continue_stmt : seq< ws< kw_continue >, ws< semi > > {};
// minimal type rule for annotation: accept an identifier as a type token (e.g., i32)
struct type_name : ws< ident > {};
// minimal: let [mut] ident (: type_name)? = <expr>;
struct let_stmt : seq< ws< kw_let >,
                                   opt< ws< kw_mut > >,
                                   ws< ident >,
                                   opt< seq< ws< colon >, type_name > >,
                                   ws< equal >, ws< expr_text >, ws< semi > > {};
// assignment: ident = <expr>;
struct assign_stmt : seq< ws< ident >, ws< equal >, ws< expr_text >, ws< semi > > {};
// compound assignments: ident (+=|-=|*=|/=) <expr>;
struct plus_equal : seq< plus_tok, equal > {};
struct minus_equal : seq< minus, equal > {};
struct star_equal : seq< star_tok, equal > {};
struct slash_equal : seq< slash_tok, equal > {};
struct compound_assign_stmt : seq< ws< ident >, ws< sor< plus_equal, minus_equal, star_equal, slash_equal > >, ws< expr_text >, ws< semi > > {};
struct stmt : sor< if_stmt, while_stmt, loop_stmt, return_stmt, break_stmt, continue_stmt, let_stmt, compound_assign_stmt, assign_stmt, block_rule, expr_stmt > {};
// Padded braces so we can hook actions reliably
struct lb_padded : ws< lbrace > {};
struct rb_padded : ws< rbrace > {};
// Tail expression (no trailing semicolon) allowed at end of block
struct tail_expr : ws< cond_expr > {};
struct block_rule : seq< lb_padded,
                                     star< space_or_comment >,
                                     star< stmt >,
                                     opt< tail_expr >,
                                     star< space_or_comment >,
                                     rb_padded > {};

// item: fn ident(params) (-> type)? { ... }
struct param_decl; // fwd (defined below as optional mut ident)
struct params_inner : seq< ws< param_decl >, star< seq< ws< comma >, ws< param_decl > > > > {};
struct params_list : seq< ws< lparen >, opt< params_inner >, ws< rparen > > {};
// Capture function name with a dedicated rule so actions can find it easily
struct fn_name : ident {};
struct ret_type_rule : seq< ws< arrow >, ws< type_name > > {};
// Parameter declaration: optional 'mut' then identifier and optional ': Type'
struct param_mut : kw_mut {};
struct param_decl : seq< opt< ws< param_mut > >, ws< ident >, opt< seq< ws< colon >, type_name > > > {};
struct fn_item : seq< ws< kw_fn >, ws< fn_name >, params_list, opt< ret_type_rule >, block_rule > {};

struct module_rule : must< star< space_or_comment >, star< fn_item >, star< space_or_comment >, eof > {};

// State carried during parsing to collect high-level info for lowering
struct build_state {
    struct fn_emit {
        std::string name;
    std::vector<std::string> body; // EDN instruction list entries as strings (top-level fn body)
        bool has_zero{false};
        std::string zero_sym{"%__rl_zero"};
        int tmp_counter{0};
        std::string gensym(const char* base){ return std::string("%__rl_") + base + std::to_string(++tmp_counter); }
    // Nested block management
    std::vector<std::vector<std::string>> block_stack;   // open blocks being filled
    std::vector<std::vector<std::string>> block_results; // closed blocks captured (LIFO)
    std::vector<std::string>& sink(){ return block_stack.empty()? body : block_stack.back(); }
    // Captured condition symbols for recent if/else-if/while conditions (LIFO)
    std::vector<std::string> cond_results;
    // Function signature
    std::vector<std::pair<std::string,std::string>> params; // (type,name)
    std::string ret_type{"i32"};
    bool saw_ret{false};
    };
    std::vector<fn_emit> fns;
    fn_emit* current(){ return fns.empty()? nullptr : &fns.back(); }
};

template<typename Rule>
struct action : nothing<Rule> {};

// Forward declarations for helper parsers used in early action specializations
static inline void ltrim(const std::string& s, size_t& i);
static inline std::string parse_ident(const std::string& s, size_t& i);

// fn_name action provided later (unified version)

// (Old typed param form removed; simple param_decl action added later.)

// Return type
// ret_type_rule action provided later (unified version)

// Small helpers to parse substrings for simple statements
static inline void ltrim(const std::string& s, size_t& i){ while(i<s.size() && isspace((unsigned char)s[i])) ++i; }
static inline std::string parse_ident(const std::string& s, size_t& i){ size_t b=i; if(b<s.size() && (isalpha((unsigned char)s[b])||s[b]=='_')){ ++i; while(i<s.size()){ char c=s[i]; if(isalnum((unsigned char)c)||c=='_') ++i; else break; } return s.substr(b, i-b);} return std::string(); }
static inline std::string parse_number(const std::string& s, size_t& i){ size_t b=i; if(i<s.size() && (s[i]=='-'||s[i]=='+')) ++i; size_t d=i; while(i<s.size() && isdigit((unsigned char)s[i])) ++i; if(i==d) { i=b; return std::string(); } return s.substr(b, i-b); }
static inline std::vector<std::string> split_args_commas(const std::string& s){
    std::vector<std::string> out; int depth=0; size_t start=0; for(size_t i=0;i<s.size();++i){ char c=s[i]; if(c=='(') ++depth; else if(c==')') --depth; else if(c==',' && depth==0){ out.push_back(std::string(s.begin()+start, s.begin()+i)); start=i+1; } }
    if(start < s.size()) out.push_back(s.substr(start));
    // trim each
    for(auto& a : out){ size_t i=0; ltrim(a,i); a = a.substr(i); size_t j=a.size(); while(j>0 && isspace((unsigned char)a[j-1])) --j; a = a.substr(0,j); }
    return out;
}

// Lower a bare expression (subset): signed int literal or ident -> returns pair(type, valueSymOrName) and possibly emits const
// Try to split at top-level operator of given set, returning left/right and operator symbol index
static inline bool split_top_level(const std::string& s, const std::vector<std::string>& ops, size_t& opPos, std::string& op, std::string& lhs, std::string& rhs){
    int depth=0; // paren depth
    // scan right-to-left for right-associative-ish lower precedence handling when chained (we choose positions carefully per precedence tier when calling)
    for(size_t i=s.size(); i-- > 0;){ char c=s[i]; if(c==')') ++depth; else if(c=='(') { if(depth>0) --depth; }
        if(depth!=0) continue;
        // check multi-char ops first
        for(const auto& cand : ops){ size_t L=cand.size(); if(L>0 && i+1>=L){ if(s.compare(i+1-L, L, cand)==0){ op = cand; opPos = i+1-L; lhs = s.substr(0, opPos); rhs = s.substr(opPos+L); return true; } } }
    }
    return false;
}

static inline std::pair<std::string,std::string> lower_simple_expr_into(build_state::fn_emit& fn, const std::string& expr_src){
    // Trim and peel outer parentheses pairs
    size_t i=0; ltrim(expr_src,i);
    std::string s = expr_src.substr(i);
    // strip trailing spaces
    while(!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
    // peel balanced outer parens repeatedly
    auto peel_parens = [](const std::string& in)->std::string{
        std::string t = in;
        while(t.size() >= 2 && t.front()=='(' && t.back()==')'){
            int depth=0; bool encloses=false; bool early_close=false;
            for(size_t k=0;k<t.size();++k){
                char c=t[k];
                if(c=='(') ++depth; else if(c==')') --depth;
                if(depth<0){ early_close=true; break; }
                if(depth==0){
                    if(k==t.size()-1){ encloses=true; }
                    else { early_close=true; break; }
                }
            }
            if(!early_close && encloses){
                t = t.substr(1, t.size()-2);
                // trim
                size_t a=0; while(a<t.size() && isspace((unsigned char)t[a])) ++a; size_t b=t.size(); while(b>0 && isspace((unsigned char)t[b-1])) --b; t = t.substr(a, b-a);
            }else{
                break;
            }
        }
        return t;
    };
    s = peel_parens(s);
    size_t si=0; ltrim(s,si);
    // Operator precedence tiers (lowest to highest)
    // 0: logical OR with short-circuit
    {
        size_t pos=0; std::string oper, L, R; if(split_top_level(s, {"||"}, pos, oper, L, R)){
            // Lower LHS collecting its instructions first
            // Use block_stack to temporarily capture instructions for L and R
            fn.block_stack.emplace_back();
            auto lv_pair = lower_simple_expr_into(fn, L);
            auto L_insts = std::move(fn.block_stack.back()); fn.block_stack.pop_back();
            fn.block_stack.emplace_back();
            auto rv_pair = lower_simple_expr_into(fn, R);
            auto R_insts = std::move(fn.block_stack.back()); fn.block_stack.pop_back();
            auto lhs = lv_pair.second; auto rhs = rv_pair.second;
            // Emit LHS instructions first
            for(const auto& ins : L_insts) fn.sink().push_back(ins);
            // Prepare result temp
            auto res = fn.gensym("lor");
            auto zero = fn.gensym("c");
            fn.sink().push_back("(const " + zero + " i1 0)");
            fn.sink().push_back("(as " + res + " i1 " + zero + ")");
            // then: assign res to 1
            auto one = fn.gensym("c"); fn.sink().push_back("(const " + one + " i1 1)");
            auto serialize = [](const std::vector<std::string>& v){ std::ostringstream bv; bv << "["; bool first=true; for(const auto& ins : v){ if(!first) bv << ' '; first=false; bv << ins; } bv << "]"; return bv.str(); };
            std::vector<std::string> then_body; then_body.push_back("(assign " + res + " " + one + ")");
            // else: R_insts; then assign res = rhs
            std::vector<std::string> else_body = R_insts; else_body.push_back("(assign " + res + " " + rhs + ")");
            std::ostringstream rif; rif << "(rif " << lhs << " :then " << serialize(then_body) << " :else " << serialize(else_body) << ")";
            fn.sink().push_back(rif.str());
            return {"i1", res};
        }
    }
    // 0.5: logical AND with short-circuit
    {
        size_t pos=0; std::string oper, L, R; if(split_top_level(s, {"&&"}, pos, oper, L, R)){
            fn.block_stack.emplace_back();
            auto lv_pair = lower_simple_expr_into(fn, L);
            auto L_insts = std::move(fn.block_stack.back()); fn.block_stack.pop_back();
            fn.block_stack.emplace_back();
            auto rv_pair = lower_simple_expr_into(fn, R);
            auto R_insts = std::move(fn.block_stack.back()); fn.block_stack.pop_back();
            auto lhs = lv_pair.second; auto rhs = rv_pair.second;
            // Emit LHS instructions first
            for(const auto& ins : L_insts) fn.sink().push_back(ins);
            // Prepare result temp
            auto res = fn.gensym("land");
            auto zero = fn.gensym("c");
            fn.sink().push_back("(const " + zero + " i1 0)");
            fn.sink().push_back("(as " + res + " i1 " + zero + ")");
            auto serialize = [](const std::vector<std::string>& v){ std::ostringstream bv; bv << "["; bool first=true; for(const auto& ins : v){ if(!first) bv << ' '; first=false; bv << ins; } bv << "]"; return bv.str(); };
            // then: evaluate R, then assign res = rhs
            std::vector<std::string> then_body = R_insts; then_body.push_back("(assign " + res + " " + rhs + ")");
            // else: ensure res=0 (already 0), leave empty or explicit assign
            std::vector<std::string> else_body; // empty
            std::ostringstream rif; rif << "(rif " << lhs << " :then " << serialize(then_body) << " :else " << serialize(else_body) << ")";
            fn.sink().push_back(rif.str());
            return {"i1", res};
        }
    }
    // 1: equality
    {
        size_t pos=0; std::string oper, L, R; if(split_top_level(s, {"==","!="}, pos, oper, L, R)){
            auto lv = lower_simple_expr_into(fn, L).second;
            auto rv = lower_simple_expr_into(fn, R).second;
            auto dst = fn.gensym("cmp");
            // For comparisons, the type marker should be the operand type (i32 for our subset); result is i1.
            std::ostringstream oss; oss << "(rcall " << dst << " i32 " << (oper=="=="? "eq" : "ne") << " " << lv << " " << rv << ")";
            fn.sink().push_back(oss.str());
            return {"i1", dst};
        }
    }
    // 2: relational
    {
        size_t pos=0; std::string oper, L, R; if(split_top_level(s, {"<=", ">=", "<", ">"}, pos, oper, L, R)){
            auto lv = lower_simple_expr_into(fn, L).second;
            auto rv = lower_simple_expr_into(fn, R).second;
            auto dst = fn.gensym("cmp");
            std::string name = (oper=="<")? "lt" : (oper==">")? "gt" : (oper=="<=")? "le" : "ge";
            // For relational ops, pass operand type i32; result is i1.
            std::ostringstream oss; oss << "(rcall " << dst << " i32 " << name << " " << lv << " " << rv << ")";
            fn.sink().push_back(oss.str());
            return {"i1", dst};
        }
    }
    // 3: additive
    {
        size_t pos=0; std::string oper, L, R; if(split_top_level(s, {"+","-"}, pos, oper, L, R)){
            auto lv = lower_simple_expr_into(fn, L).second;
            auto rv = lower_simple_expr_into(fn, R).second;
            auto dst = fn.gensym("add");
            std::ostringstream oss; oss << "(rcall " << dst << " i32 " << (oper=="+"? "add" : "sub") << " " << lv << " " << rv << ")";
            fn.sink().push_back(oss.str());
            return {"i32", dst};
        }
    }
    // 4: multiplicative
    {
        size_t pos=0; std::string oper, L, R; if(split_top_level(s, {"*","/"}, pos, oper, L, R)){
            auto lv = lower_simple_expr_into(fn, L).second;
            auto rv = lower_simple_expr_into(fn, R).second;
            auto dst = fn.gensym("mul");
            std::ostringstream oss; oss << "(rcall " << dst << " i32 " << (oper=="*"? "mul" : "div") << " " << lv << " " << rv << ")";
            fn.sink().push_back(oss.str());
            return {"i32", dst};
        }
    }
    // Unary minus over non-numeric expression (e.g., -x, -(a+b))
    if(!s.empty() && s[0]=='-'){
        // If it's a numeric literal, the number case below will handle it, so check next char
        if(!(s.size()>1 && (isdigit((unsigned char)s[1])))){
            std::string inner = s.substr(1);
            auto v = lower_simple_expr_into(fn, inner).second;
            auto zero = fn.gensym("c");
            fn.sink().push_back("(const " + zero + " i32 0)");
            auto dst = fn.gensym("neg");
            std::ostringstream oss; oss << "(rcall " << dst << " i32 sub " << zero << " " << v << ")";
            fn.sink().push_back(oss.str());
            return {"i32", dst};
        }
    }
    // Try number
    {
        size_t j=si; auto num = parse_number(s, j);
        if(!num.empty()){
            auto tmp = fn.gensym("c");
            fn.sink().push_back("(const " + tmp + " i32 " + num + ")");
            return {"i32", tmp};
        }
    }
    // Try ident
    {
        size_t j=si; auto id = parse_ident(s, j);
        if(!id.empty()){
            // boolean literals
            if(id == "true" || id == "false"){
                auto tmp = fn.gensym("c");
                fn.sink().push_back(std::string("(const ") + tmp + " i1 " + (id=="true"? "1" : "0") + ")");
                return {"i1", tmp};
            }
            // Check for call form ident(...)
            size_t k=j; while(k<s.size() && isspace((unsigned char)s[k])) ++k;
            if(k<s.size() && s[k]=='('){
                // parse args inside matching paren
                int depth=0; size_t m=k; for(; m<s.size(); ++m){ char c=s[m]; if(c=='(') ++depth; else if(c==')'){ --depth; if(depth==0) break; } }
                std::string inner = (k+1<=m)? s.substr(k+1, m-(k+1)) : std::string();
                auto args = split_args_commas(inner);
                std::vector<std::string> argSyms; argSyms.reserve(args.size());
                for(const auto& a : args){ if(a.empty()) continue; auto p = lower_simple_expr_into(fn, a); argSyms.push_back(p.second); }
                auto dst = fn.gensym("call");
                std::ostringstream oss; oss << "(rcall " << dst << " i32 " << id; for(const auto& as : argSyms) oss << " " << as; oss << ")";
                fn.sink().push_back(oss.str());
                return {"i32", dst};
            } else {
                if(id[0] != '%') id = std::string("%") + id;
                return {"i32", id}; // default i32 for now
            }
        }
    }
    // Unary logical NOT
    if(!s.empty() && s[0]=='!'){
        std::string inner = s.substr(1);
        auto v = lower_simple_expr_into(fn, inner).second;
        auto dst = fn.gensym("not");
    std::ostringstream oss; oss << "(rcall " << dst << " i1 not " << v << ")";
        fn.sink().push_back(oss.str());
        return {"i1", dst};
    }
    // Fallback: synth zero and return it
    if(!fn.has_zero){ fn.sink().push_back("(const " + fn.zero_sym + " i32 0)"); fn.has_zero=true; }
    return {"i32", fn.zero_sym};
}

// let statement lowering: let <name> (: type)? = <expr>;
template<>
struct action< let_stmt > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){
        auto* fn = st.current(); if(!fn) return;
        std::string s = in.string();
        // strip trailing ';'
        if(!s.empty() && s.back()==';') s.pop_back();
        size_t i=0; ltrim(s,i);
        // consume 'let'
        if(s.compare(i,3,"let")!=0) return; i+=3; ltrim(s,i);
        // optional 'mut'
        if(i+3 <= s.size() && s.compare(i,3,"mut")==0){
            // 'mut' currently parsed but unused; advance position only
            i += 3; ltrim(s,i);
        }
        // name
        auto name = parse_ident(s,i); if(name.empty()) return; if(name[0] != '%') name = std::string("%")+name; ltrim(s,i);
        // optional type
        std::string ty = "i32";
        if(i<s.size() && s[i]==':'){ ++i; ltrim(s,i); auto tname = parse_ident(s,i); if(!tname.empty()) ty = tname; ltrim(s,i); }
        // '='
        if(i>=s.size() || s[i] != '=') return; ++i; ltrim(s,i);
    // rest is expr; lower into a temporary buffer, then emit before the declaration
    auto expr = s.substr(i);
    fn->block_stack.emplace_back();
    auto [ety, val] = lower_simple_expr_into(*fn, expr);
    auto expr_insts = std::move(fn->block_stack.back()); fn->block_stack.pop_back();
    for(const auto& ins : expr_insts){ fn->sink().push_back(ins); }
    // Directly declare the variable with 'as' (mut handled at higher levels; EDN core doesn't enforce immutability)
    fn->sink().push_back(std::string("(as ") + name + " " + ty + " " + val + ")");
    }
};

// assignment: ident = expr;
template<>
struct action< assign_stmt > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){
        auto* fn = st.current(); if(!fn) return;
        std::string s = in.string(); if(!s.empty() && s.back()==';') s.pop_back();
        size_t i=0; ltrim(s,i); auto name = parse_ident(s,i); if(name.empty()) return; if(name[0] != '%') name = std::string("%")+name; ltrim(s,i); if(i>=s.size()||s[i]!='=') return; ++i; ltrim(s,i);
        auto expr = s.substr(i);
        auto [ety, val] = lower_simple_expr_into(*fn, expr);
    fn->sink().push_back("(assign " + name + " " + val + ")");
    }
};

// compound assignment: ident op= expr; -> tmp = (rcall i32 op ident expr); assign ident tmp
template<>
struct action< compound_assign_stmt > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){
        auto* fn = st.current(); if(!fn) return;
        std::string s = in.string(); if(!s.empty() && s.back()==';') s.pop_back();
        size_t i=0; ltrim(s,i);
        auto name = parse_ident(s,i); if(name.empty()) return; if(name[0] != '%') name = std::string("%")+name; ltrim(s,i);
        // parse operator before '='
        std::string op;
        if(i<s.size()){
            if(s[i]=='+') { ++i; if(i<s.size() && s[i]=='='){ ++i; op = "add"; } }
            else if(s[i]=='-') { ++i; if(i<s.size() && s[i]=='='){ ++i; op = "sub"; } }
            else if(s[i]=='*') { ++i; if(i<s.size() && s[i]=='='){ ++i; op = "mul"; } }
            else if(s[i]=='/') { ++i; if(i<s.size() && s[i]=='='){ ++i; op = "div"; } }
        }
        if(op.empty()) return; ltrim(s,i);
        auto expr = s.substr(i);
        auto rhs = lower_simple_expr_into(*fn, expr).second;
        auto tmp = fn->gensym("op");
        std::ostringstream oss; oss << "(rcall " << tmp << " i32 " << op << " " << name << " " << rhs << ")";
        fn->sink().push_back(oss.str());
        fn->sink().push_back("(assign " + name + " " + tmp + ")");
    }
};

// return [expr];
template<>
struct action< return_stmt > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){
        auto* fn = st.current(); if(!fn) return;
    fn->saw_ret = true;
        std::string s = in.string(); // includes 'return' and ';'
        size_t i=0; ltrim(s,i);
        if(s.compare(i,6,"return")!=0) return; i+=6; ltrim(s,i);
        std::string valSym;
        if(i<s.size() && s[i] == ';'){
            // Synthesize a zero of i32 and cast to function return type to keep types consistent for non-i32 returns.
            if(!fn->has_zero){ fn->sink().push_back("(const " + fn->zero_sym + " i32 0)"); fn->has_zero = true; }
            auto cast = fn->gensym("retz");
            fn->sink().push_back("(as " + cast + " " + (fn->ret_type.empty()? std::string("i32") : fn->ret_type) + " " + fn->zero_sym + ")");
            valSym = cast;
        } else {
            // parse simple expr to symbol
            auto expr = s.substr(i);
            auto [ety, v] = lower_simple_expr_into(*fn, expr);
            valSym = v;
        }
        auto rty = (fn->ret_type.empty()? std::string("i32") : fn->ret_type);
        fn->sink().push_back("(ret " + rty + " " + valSym + ")");
    }
};

// expression statements: either function call "ident(args...);" or ignore
template<>
struct action< expr_stmt > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){
        auto* fn = st.current(); if(!fn) return;
        std::string s = in.string(); if(!s.empty() && s.back()==';') s.pop_back();
        size_t i=0; ltrim(s,i);
        // parse leading ident
        auto callee = parse_ident(s,i);
        if(callee.empty()) return; // ignore other expr forms for now
        ltrim(s,i);
        if(i>=s.size() || s[i] != '(') return; // not a call
        // find matching ')'
        int depth=0; size_t j=i; for(; j<s.size(); ++j){ char c=s[j]; if(c=='(') ++depth; else if(c==')'){ --depth; if(depth==0){ break; } } }
        if(j>=s.size() || s[j]!=')') return; // malformed
        auto inner = s.substr(i+1, j-(i+1));
        auto args = split_args_commas(inner);
        std::vector<std::string> argSyms; argSyms.reserve(args.size());
        for(const auto& a : args){ if(a.empty()) continue; auto p = lower_simple_expr_into(*fn, a); argSyms.push_back(p.second); }
        auto dst = fn->gensym("call");
        // ensure callee is symbol name, not % prefixed (calls use raw symbol for function name)
        std::string calleeSym = callee;
        // emit: (rcall %dst i32 callee argSyms...)
        std::ostringstream oss; oss << "(rcall " << dst << " i32 " << calleeSym;
        for(const auto& as : argSyms) oss << " " << as;
        oss << ")";
        fn->sink().push_back(oss.str());
    }
};

// Block enter/leave
template<>
struct action< lb_padded > {
    template<typename Input>
    static void apply(const Input&, build_state& st){ auto* fn=st.current(); if(!fn) return; fn->block_stack.emplace_back(); }
};
template<>
struct action< rb_padded > {
    template<typename Input>
    static void apply(const Input&, build_state& st){ auto* fn=st.current(); if(!fn) return; if(fn->block_stack.empty()) return; auto blk = std::move(fn->block_stack.back()); fn->block_stack.pop_back(); fn->block_results.push_back(std::move(blk)); }
};

// Completed block_rule: when closing a function's top-level block, if no explicit return emitted
// and the function body ends with a simple expression statement lacking semicolon semantics,
// we could synthesize an implicit return; current grammar models only terminated statements,
// so just move collected instructions into sink when associated with an enclosing construct.
template<>
struct action< block_rule > {
    template<typename Input>
    static void apply(const Input&, build_state& st){
        auto* fn = st.current(); if(!fn) return;
        if(st.fns.empty()) return;
        // If this block belongs to a function item (top-level after lb_padded/rb_padded), its
        // serialized instructions were already pushed via block_results and consumed by fn_item action.
        // No extra logic needed now; placeholder for future tail expression return.
    }
};

// Tail expression implicit return
template<>
struct action< tail_expr > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){
        auto* fn = st.current(); if(!fn) return; if(fn->saw_ret) return; // already have explicit return
        std::string expr = in.string();
        auto [ty, sym] = lower_simple_expr_into(*fn, expr);
        auto rty = (fn->ret_type.empty()? std::string("i32") : fn->ret_type);
        if(ty != rty){
            auto cast = fn->gensym("tail");
            fn->sink().push_back("(as " + cast + " " + rty + " " + sym + ")");
            sym = cast;
        }
        fn->sink().push_back("(ret " + rty + " " + sym + ")");
        fn->saw_ret = true;
    }
};

// while <cond> <block>
template<>
struct action< while_stmt > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){ auto* fn=st.current(); if(!fn) return; if(fn->block_results.empty()) return; auto bodyVec = std::move(fn->block_results.back()); fn->block_results.pop_back();
        // Prefer captured condition from action<while_cond>, fallback to substring parse
        std::string condSym;
        if(!fn->cond_results.empty()){ condSym = std::move(fn->cond_results.back()); fn->cond_results.pop_back(); }
        else {
            std::string s=in.string(); auto pos=s.find('{'); std::string cond_src = pos==std::string::npos? std::string() : s.substr(0,pos);
            size_t i=0; ltrim(cond_src,i); if(cond_src.compare(i,5,"while")==0) { i+=5; } ltrim(cond_src,i); cond_src = cond_src.substr(i);
            auto p = lower_simple_expr_into(*fn, cond_src); condSym = p.second;
        }
        // Serialize body
        std::ostringstream bv; bv << "["; bool first=true; for(const auto& ins : bodyVec){ if(!first) bv << ' '; first=false; bv << ins; } bv << "]";
        std::ostringstream oss; oss << "(rwhile " << condSym << " :body " << bv.str() << ")"; fn->sink().push_back(oss.str());
    }
};

// loop <block>
template<>
struct action< loop_stmt > {
    template<typename Input>
    static void apply(const Input&, build_state& st){ auto* fn=st.current(); if(!fn) return; if(fn->block_results.empty()) return; auto bodyVec = std::move(fn->block_results.back()); fn->block_results.pop_back(); std::ostringstream bv; bv << "["; bool first=true; for(const auto& ins : bodyVec){ if(!first) bv << ' '; first=false; bv << ins; } bv << "]"; fn->sink().push_back(std::string("(rloop :body ") + bv.str() + ")"); }
};

// break/continue
template<>
struct action< break_stmt > {
    template<typename Input>
    static void apply(const Input&, build_state& st){ auto* fn=st.current(); if(!fn) return; fn->sink().push_back("(rbreak)"); }
};
template<>
struct action< continue_stmt > {
    template<typename Input>
    static void apply(const Input&, build_state& st){ auto* fn=st.current(); if(!fn) return; fn->sink().push_back("(rcontinue)"); }
};

// if <cond> <then-block> [else if <cond> <block>]* [else <block>]
template<>
struct action< if_stmt > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){ auto* fn=st.current(); if(!fn) return; std::string s=in.string();
        // Count branches via source text so we know how many blocks/conds to pop.
        size_t count_else_if = 0; size_t pos_search = 0; while(true){ auto p = s.find("else if", pos_search); if(p==std::string::npos) break; ++count_else_if; pos_search = p + 1; }
        bool hasElse = s.find(" else ") != std::string::npos || s.rfind("else{", std::string::npos) != std::string::npos;
        size_t blocks_needed = 1 + count_else_if + (hasElse ? 1 : 0);
        if(fn->block_results.size() < blocks_needed) return;
        size_t conds_needed = 1 + count_else_if;
        if(fn->cond_results.size() < conds_needed){
            // Fallback: derive first cond from substring
            auto pos = s.find('{'); std::string head = pos==std::string::npos? std::string() : s.substr(0,pos); size_t i=0; ltrim(head,i); if(head.compare(i,2,"if")==0) { i+=2; } ltrim(head,i); head = head.substr(i);
            auto p = lower_simple_expr_into(*fn, head);
            fn->cond_results.push_back(p.second);
        }
        // Collect bodies in source order
        std::vector<std::vector<std::string>> bodies; bodies.reserve(blocks_needed);
        for(size_t k=0; k<blocks_needed; ++k){ bodies.push_back(std::move(fn->block_results.back())); fn->block_results.pop_back(); }
        std::reverse(bodies.begin(), bodies.end());
        // Pop conditions corresponding to this chain (LIFO capture order)
        std::vector<std::string> conds; conds.reserve(conds_needed);
        for(size_t k=0; k<conds_needed; ++k){ conds.push_back(std::move(fn->cond_results.back())); fn->cond_results.pop_back(); }
        std::reverse(conds.begin(), conds.end()); // now in source order
        auto serialize = [](const std::vector<std::string>& v){ std::ostringstream bv; bv << "["; bool first=true; for(const auto& ins : v){ if(!first) bv << ' '; first=false; bv << ins; } bv << "]"; return bv.str(); };
        // Build nested rif chain
        auto build_chain = [&](auto&& self, size_t idx)->std::string{
            if(idx >= conds.size()){
                // No more conds; return else body or empty
                if(hasElse) return serialize(bodies.back());
                return std::string("[]");
            }
            std::ostringstream out;
            out << "(rif " << conds[idx] << " :then " << serialize(bodies[idx]) << " :else " << self(self, idx+1) << ")";
            return out.str();
        };
        fn->sink().push_back(build_chain(build_chain, 0));
    }
};

// Capture conditions when they appear in specific contexts
template<>
struct action< if_cond > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){ auto* fn=st.current(); if(!fn) return; auto p = lower_simple_expr_into(*fn, in.string()); fn->cond_results.push_back(p.second); }
};
template<>
struct action< else_if_cond > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){ auto* fn=st.current(); if(!fn) return; auto p = lower_simple_expr_into(*fn, in.string()); fn->cond_results.push_back(p.second); }
};
template<>
struct action< while_cond > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){ auto* fn=st.current(); if(!fn) return; auto p = lower_simple_expr_into(*fn, in.string()); fn->cond_results.push_back(p.second); }
};

// Capture function name
template<>
struct action< fn_name > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){ build_state::fn_emit f; f.name=in.string(); st.fns.push_back(std::move(f)); }
};

// Return type rule unified action
template<>
struct action< ret_type_rule > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){ auto* fn=st.current(); if(!fn) return; std::string s=in.string(); auto pos=s.find('>'); if(pos!=std::string::npos){ std::string t=s.substr(pos+1); size_t i=0; ltrim(t,i); auto ty=parse_ident(t,i); if(!ty.empty()) fn->ret_type=ty; } }
};

// Parameter capture: supports [mut] name [: Type]
template<>
struct action< param_decl > {
    template<typename Input>
    static void apply(const Input& in, build_state& st){ auto* fn=st.current(); if(!fn) return; std::string s=in.string(); size_t i=0; ltrim(s,i); if(s.compare(i,3,"mut")==0 && (i+3==s.size()||isspace((unsigned char)s[i+3]))){ i+=3; ltrim(s,i);} auto name=parse_ident(s,i); ltrim(s,i); std::string ty="i32"; if(i<s.size() && s[i]==':'){ ++i; ltrim(s,i); auto t=parse_ident(s,i); if(!t.empty()) ty=t; }
        if(!name.empty()){ if(name[0] != '%') name = std::string("%") + name; fn->params.emplace_back(ty, name); }
    }
};

// At end of a function item, if the body was parsed as a block, move it into fn.body
template<>
struct action< fn_item > {
    template<typename Input>
    static void apply(const Input&, build_state& st){
        auto* fn=st.current(); if(!fn) return;
        if(!fn->block_results.empty()){
            auto blk = std::move(fn->block_results.back()); fn->block_results.pop_back();
            // Append parsed top-level block into body
            for(auto &ins : blk){ fn->body.push_back(std::move(ins)); }
        }
    }
};

ParseResult Parser::parse_string(std::string_view src, std::string_view filename) const {
    ParseResult r; r.success = false;
    try {
        // Strip UTF-8 BOM if present to avoid parse-at-EOF issues on some editors
        std::string src_copy(src);
        if(src_copy.size() >= 3 && static_cast<unsigned char>(src_copy[0]) == 0xEF && static_cast<unsigned char>(src_copy[1]) == 0xBB && static_cast<unsigned char>(src_copy[2]) == 0xBF){
            src_copy.erase(0, 3);
        }
        memory_input in(src_copy, std::string(filename));
        build_state st;
        if(!parse< module_rule, action >(in, st)) {
            r.success = false;
            r.error_message = "parse failed";
            return r;
        }
        // Minimal lowering: emit a module with functions populated by lowered statements when available.
        std::ostringstream oss;
        oss << "(module";
        for(const auto& fn : st.fns){
            oss << " (fn :name \"" << fn.name << "\" :ret " << (fn.ret_type.empty()? "i32" : fn.ret_type) << " :params [ ";
            bool firstp=true; for(const auto& p : fn.params){ if(!firstp) oss << ' '; firstp=false; oss << "(param " << p.first << " " << p.second << ")"; }
            oss << " ] :body [ ";
            if(fn.body.empty()){
                // default to returning zero
                oss << "(const %__rl_zero i32 0) (ret i32 %__rl_zero)";
            } else {
                bool hasRet=false; for(const auto& inst : fn.body){ if(inst.rfind("(ret ",0)==0) { hasRet=true; break; } }
                bool first=true;
                for(const auto& inst : fn.body){ if(!first) oss << ' '; first=false; oss << inst; }
                if(!hasRet){
                    if(!fn.has_zero){
                        // add zero const before ret
                        if(!fn.body.empty()) oss << ' ';
                        oss << "(const %__rl_zero i32 0) ";
                    }
                    // Cast to function return type if not i32
                    if(fn.ret_type != "" && fn.ret_type != "i32"){ oss << "(as %__rl_ret0 " << fn.ret_type << " %__rl_zero) (ret " << fn.ret_type << " %__rl_ret0)"; }
                    else { oss << "(ret i32 %__rl_zero)"; }
                }
            }
            oss << " ])";
        }
        oss << ")";
        r.success = true;
        r.edn = oss.str();
        return r;
    } catch (const tao::pegtl::parse_error& e) {
        r.success = false;
        r.error_message = e.what();
        if(!e.positions().empty()){
            const auto& p = e.positions().front();
            r.line = static_cast<int>(p.line);
            r.column = static_cast<int>(p.column);
        } else {
            r.line = 0;
            r.column = 0;
        }
        return r;
    }
}

} // namespace rustlite
