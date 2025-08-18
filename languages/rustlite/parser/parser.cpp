#include "parser.hpp"
#include <tao/pegtl.hpp>

namespace rustlite {

// Grammar v0.1:
// - Skip spaces and comments
// - Accept zero or more empty function items:  fn ident() { /* empty body */ }
// - Module := ws item* ws EOF
struct comment_line : tao::pegtl::seq< tao::pegtl::two<'/'>, tao::pegtl::until< tao::pegtl::eolf > > {};
struct block_comment : tao::pegtl::seq< tao::pegtl::one<'/'>, tao::pegtl::one<'*'>, tao::pegtl::until< tao::pegtl::seq< tao::pegtl::one<'*'>, tao::pegtl::one<'/'> > > > {};
struct space_or_comment : tao::pegtl::sor< tao::pegtl::space, comment_line, block_comment > {};

// tokens
struct kw_fn : tao::pegtl::string<'f','n'> {};
struct kw_return : tao::pegtl::string<'r','e','t','u','r','n'> {};
struct kw_if : tao::pegtl::string<'i','f'> {};
struct kw_else : tao::pegtl::string<'e','l','s','e'> {};
struct kw_while : tao::pegtl::string<'w','h','i','l','e'> {};
struct kw_loop : tao::pegtl::string<'l','o','o','p'> {};
struct kw_let : tao::pegtl::string<'l','e','t'> {};
struct kw_break : tao::pegtl::string<'b','r','e','a','k'> {};
struct kw_continue : tao::pegtl::string<'c','o','n','t','i','n','u','e'> {};
struct ident_first : tao::pegtl::ranges<'a','z','A','Z','_','_'> {};
struct ident_rest : tao::pegtl::ranges<'a','z','A','Z','0','9','_','_'> {};
struct ident : tao::pegtl::seq< ident_first, tao::pegtl::star< ident_rest > > {};

template<typename Rule>
using ws = tao::pegtl::pad< Rule, space_or_comment >;

// Minimal block with optional integer literal statements: { (int ';')* }
struct lbrace : tao::pegtl::one<'{'> {};
struct rbrace : tao::pegtl::one<'}'> {};
struct lparen : tao::pegtl::one<'('> {};
struct rparen : tao::pegtl::one<')'> {};
struct semi : tao::pegtl::one<';'> {};
struct colon : tao::pegtl::one<':'> {};
struct equal : tao::pegtl::one<'='> {};
struct minus : tao::pegtl::one<'-'> {};
struct int_lit : tao::pegtl::plus< tao::pegtl::digit > {};
// Simple primary forms for statements: identifier, and optional call suffix with args
struct comma : tao::pegtl::one<','> {};
// forward decls so we can reference before definitions
struct signed_int;
struct paren_expr;
struct arg_expr : tao::pegtl::sor< ws< signed_int >, ws< ident >, ws< paren_expr > > {};
struct call_args_inner : tao::pegtl::seq< ws< arg_expr >, tao::pegtl::star< tao::pegtl::seq< ws< comma >, ws< arg_expr > > > > {};
struct call_args : tao::pegtl::seq< ws< lparen >, tao::pegtl::opt< call_args_inner >, ws< rparen > > {};
struct ident_or_call : tao::pegtl::seq< ws< ident >, tao::pegtl::opt< call_args > > {};
// signed int for expr statements: allow optional leading '-'
struct signed_int : tao::pegtl::sor< tao::pegtl::seq< ws< minus >, ws< int_lit > >, ws< int_lit > > {};
// cond/expr categories allow parentheses
struct cond_expr;
struct paren_expr : tao::pegtl::seq< ws< lparen >, ws< cond_expr >, ws< rparen > > {};
struct expr_stmt : tao::pegtl::seq< ws< cond_expr >, ws< semi > > {};
struct return_stmt : tao::pegtl::seq< ws< kw_return >, tao::pegtl::opt< ws< int_lit > >, ws< semi > > {};
// if-statement: if <cond> block (else if <cond> block)* (else block)?
struct block_rule; // forward declaration
struct cond_expr : tao::pegtl::sor< paren_expr, signed_int, ident_or_call > {};
struct else_if_arm : tao::pegtl::seq< ws< kw_else >, ws< kw_if >, ws< cond_expr >, block_rule > {};
struct else_tail : tao::pegtl::seq< tao::pegtl::star< else_if_arm >,
                                    tao::pegtl::opt< tao::pegtl::seq< ws< kw_else >, block_rule > > > {};
struct if_stmt : tao::pegtl::seq< ws< kw_if >, ws< cond_expr >, block_rule,
                                  tao::pegtl::opt< else_tail > > {};
struct while_stmt : tao::pegtl::seq< ws< kw_while >, ws< cond_expr >, block_rule > {};
struct loop_stmt : tao::pegtl::seq< ws< kw_loop >, block_rule > {};
struct break_stmt : tao::pegtl::seq< ws< kw_break >, ws< semi > > {};
struct continue_stmt : tao::pegtl::seq< ws< kw_continue >, ws< semi > > {};
// minimal type rule for annotation: accept an identifier as a type token (e.g., i32)
struct type_name : ws< ident > {};
// minimal: let ident (: type_name)? = <expr>;
struct let_stmt : tao::pegtl::seq< ws< kw_let >,
                                   ws< ident >,
                                   tao::pegtl::opt< tao::pegtl::seq< ws< colon >, type_name > >,
                                   ws< equal >, ws< cond_expr >, ws< semi > > {};
// assignment: ident = <expr>;
struct assign_stmt : tao::pegtl::seq< ws< ident >, ws< equal >, ws< cond_expr >, ws< semi > > {};
struct stmt : tao::pegtl::sor< if_stmt, while_stmt, loop_stmt, return_stmt, break_stmt, continue_stmt, let_stmt, assign_stmt, block_rule, expr_stmt > {};
struct block_rule : tao::pegtl::seq< ws< lbrace >,
                                     tao::pegtl::star< space_or_comment >,
                                     tao::pegtl::star< stmt >,
                                     tao::pegtl::star< space_or_comment >,
                                     ws< rbrace > > {};

// item: fn ident() { ... }
struct empty_params : tao::pegtl::seq< ws< lparen >, tao::pegtl::star< space_or_comment >, ws< rparen > > {};
struct fn_item : tao::pegtl::seq< ws< kw_fn >, ws< ident >, empty_params, block_rule > {};

struct module_rule : tao::pegtl::must< tao::pegtl::star< space_or_comment >, tao::pegtl::star< fn_item >, tao::pegtl::star< space_or_comment >, tao::pegtl::eof > {};

ParseResult Parser::parse_string(std::string_view src, std::string_view filename) const {
    ParseResult r; r.success = false;
    try {
        tao::pegtl::memory_input in(src, std::string(filename));
        tao::pegtl::parse< module_rule >(in); // no actions yet
        // v0 lower: empty module
        r.success = true;
        r.edn = "(module)";
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
