// Assert / panic related Rustlite surface macros.
// Provides:
//   (rpanic <msg>) -> (panic <msg>)
//   (rassert <pred>) -> (block :body [ (if <pred> [] [ (panic) ]) ])
//   (rassert <a> <b>) -> (assert <a> <b>)
//   (rassert-*- a b) variants forwarding to underlying assert-* core ops.

#include "rustlite/macros/context.hpp"
#include "rustlite/expand.hpp"
#include "edn/transform.hpp"
#include "rustlite/macros/helpers.hpp"

using namespace edn;
namespace rustlite {
using rustlite::rl_make_sym; using rustlite::rl_make_i64; using rustlite::rl_gensym; // future use

void register_assert_macros(edn::Transformer& tx, const std::shared_ptr<MacroContext>&){
    // rpanic just forwards (second argument is optional message / code symbol)
    tx.add_macro("rpanic", [](const list& form)->std::optional<node_ptr>{
        const auto &e = form.elems; if(e.size()<1) return std::nullopt; // allow zero operands? require at least name
        if(e.size()==1){ list out; out.elems = { rl_make_sym("panic") }; return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); }
        list out; out.elems = { rl_make_sym("panic"), e[1] }; return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });

    tx.add_macro("rassert", [](const list& form)->std::optional<node_ptr>{
        const auto &e = form.elems; if(e.size()<2) return std::nullopt; // need predicate or (a b)
        if(e.size()==2){ // unary predicate form
            if(!std::holds_alternative<symbol>(e[1]->data)) return std::nullopt;
            // Build (block :body [ (if pred [] [ (panic) ]) ])
            vector_t emptyThen;
            list panicL; panicL.elems = { rl_make_sym("panic") };
            vector_t elseVec; elseVec.elems.push_back(std::make_shared<node>( node{ panicL, {} } ));
            list ifL; ifL.elems = { rl_make_sym("if"), e[1], std::make_shared<node>( node{ emptyThen, {} } ), std::make_shared<node>( node{ elseVec, {} } ) };
            vector_t body; body.elems.push_back(std::make_shared<node>( node{ ifL, {} } ));
            list blockL; blockL.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ body, {} } ) };
            return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
        }
        if(e.size()==3){ // binary form becomes core (assert a b)
            list out; out.elems = { rl_make_sym("assert"), e[1], e[2] };
            return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
        }
        return std::nullopt; // arities >3 not supported
    });

    // Comparator variants: expand (rassert-eq a b) into block with (eq %tmp i1 a b) + inline unary assert lowering
    auto cmp_variant = [&](const std::string& name, const std::string& op){
        tx.add_macro(name, [op](const list& form)->std::optional<node_ptr>{
            const auto &e = form.elems; if(e.size()!=3) return std::nullopt;
            // Build (const predicate via op) then if structure identical to unary rassert path
            auto a = e[1]; auto b = e[2];
            auto tmpSym = rl_make_sym(rl_gensym("acmp"));
            vector_t body;
            { list cmpL; cmpL.elems = { rl_make_sym(op), tmpSym, rl_make_sym("i1"), a, b }; body.elems.push_back(std::make_shared<node>( node{ cmpL, {} } )); }
            // if tmpSym [] [ (panic) ]
            vector_t thenV; // success empty
            list panicL; panicL.elems = { rl_make_sym("panic") };
            vector_t elseV; elseV.elems.push_back(std::make_shared<node>( node{ panicL, {} } ));
            list ifL; ifL.elems = { rl_make_sym("if"), tmpSym, std::make_shared<node>( node{ thenV, {} } ), std::make_shared<node>( node{ elseV, {} } ) };
            body.elems.push_back(std::make_shared<node>( node{ ifL, {} } ));
            list blockL; blockL.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ body, {} } ) };
            return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
        });
    };
    cmp_variant("rassert-eq", "eq");
    cmp_variant("rassert-ne", "ne");
    cmp_variant("rassert-lt", "lt");
    cmp_variant("rassert-le", "le");
    cmp_variant("rassert-gt", "gt");
    cmp_variant("rassert-ge", "ge");
}
} // namespace rustlite
