#include "rustlite/macros/context.hpp"
#include "rustlite/expand.hpp"
#include "edn/transform.hpp"
#include "rustlite/macros/helpers.hpp"

using namespace edn;

namespace rustlite {

void register_literal_macros(edn::Transformer& tx, const std::shared_ptr<MacroContext>&){
    // rcstr: (rcstr %dst "literal") -> (cstr %dst "literal")
    tx.add_macro("rcstr", [](const list& form)->std::optional<node_ptr>{
        auto &el = form.elems; if(el.size()!=3) return std::nullopt; // (rcstr %dst "lit")
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; // %dst must be symbol
        node_ptr lit = el[2];
        if(std::holds_alternative<std::string>(lit->data)){
            auto inner = std::get<std::string>(lit->data);
            std::string quoted = "\""; quoted.reserve(inner.size()+2);
            for(char c: inner){ if(c=='"' || c=='\\') quoted.push_back('\\'); quoted.push_back(c); }
            quoted.push_back('"');
            lit = rl_make_sym(quoted);
        }
        if(!std::holds_alternative<symbol>(lit->data)) return std::nullopt;
        auto name = std::get<symbol>(lit->data).name;
        if(name.size()<2 || name.front()!='"' || name.back()!='"') return std::nullopt; // ensure quoted form
        list l; l.elems = { rl_make_sym("cstr"), el[1], lit };
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    // rbytes: (rbytes %dst [ ints ]) -> (bytes %dst [ ints ])
    tx.add_macro("rbytes", [](const list& form)->std::optional<node_ptr>{
        auto &el = form.elems; if(el.size()!=3) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; // %dst
        if(!std::holds_alternative<vector_t>(el[2]->data)) return std::nullopt; // [ ints ]
        for(auto &b : std::get<vector_t>(el[2]->data).elems){
            if(!(std::holds_alternative<int64_t>(b->data) || std::holds_alternative<symbol>(b->data))) return std::nullopt;
        }
    list l; l.elems = { rl_make_sym("bytes"), el[1], el[2] };
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
}

} // namespace rustlite
