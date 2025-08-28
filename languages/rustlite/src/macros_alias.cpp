#include "rustlite/macros/context.hpp"
#include "rustlite/expand.hpp"
#include "edn/transform.hpp"
#include "rustlite/macros/helpers.hpp"

using namespace edn;
namespace rustlite {
using rustlite::rl_make_sym;

void register_alias_macros(edn::Transformer& tx, const std::shared_ptr<MacroContext>&){
    tx.add_macro("rtypedef", [](const list& form)->std::optional<node_ptr>{ auto &e=form.elems; if(e.size()!=3) return std::nullopt; list out; out.elems={ rl_make_sym("typedef"), e[1], e[2] }; return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); });
    tx.add_macro("rassign", [](const list& form)->std::optional<node_ptr>{ auto &e=form.elems; if(e.size()!=3) return std::nullopt; list out; out.elems={ rl_make_sym("assign"), e[1], e[2] }; return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); });
    tx.add_macro("rret", [](const list& form)->std::optional<node_ptr>{
        auto &e=form.elems; // Accept (rret <ty> %val) or (rret %val)
        if(e.size()==3){ // (rret Ty %v)
            list out; out.elems={ rl_make_sym("ret"), e[1], e[2] }; return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
        } else if(e.size()==2){ // (rret %v) legacy
            list out; out.elems={ rl_make_sym("ret"), e[1] }; return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
        }
        return std::nullopt;
    });
}
} // namespace rustlite
