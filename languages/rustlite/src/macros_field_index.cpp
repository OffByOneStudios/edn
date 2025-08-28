#include "rustlite/macros/context.hpp"
#include "rustlite/expand.hpp"
#include "edn/transform.hpp"
#include "rustlite/macros/helpers.hpp"

using namespace edn;
namespace rustlite {
using rustlite::rl_make_sym;

void register_field_index_macros(edn::Transformer& tx, const std::shared_ptr<MacroContext>&){
    // rget: (rget %dst Struct %base field) -> (member %dst Struct %base field)
    tx.add_macro("rget", [](const list& form)->std::optional<node_ptr>{
        auto &e=form.elems; if(e.size()!=5) return std::nullopt; // name + 4 args
        // Basic shape validation
        if(!std::holds_alternative<symbol>(e[1]->data) || !std::holds_alternative<symbol>(e[2]->data) || !std::holds_alternative<symbol>(e[3]->data) || !std::holds_alternative<symbol>(e[4]->data)) return std::nullopt;
        list out; out.elems = { rl_make_sym("member"), e[1], e[2], e[3], e[4] };
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });
    // rset: (rset <elem-ty> Struct %base field %val) -> member-addr + store sequence
    tx.add_macro("rset", [](const list& form)->std::optional<node_ptr>{
        auto &e=form.elems; if(e.size()!=6) return std::nullopt; // name + 5 args
        // Expect symbols for all positions
        for(size_t i=1;i<e.size();++i){ if(!std::holds_alternative<symbol>(e[i]->data)) return std::nullopt; }
        auto elemTy = e[1]; auto structName=e[2]; auto base=e[3]; auto field=e[4]; auto val=e[5];
        auto tmp = rl_make_sym(rl_gensym("fld.addr"));
        vector_t body;
        { list maddr; maddr.elems = { rl_make_sym("member-addr"), tmp, structName, base, field }; body.elems.push_back(std::make_shared<node>( node{ maddr, {} } )); }
        { list st; st.elems = { rl_make_sym("store"), elemTy, tmp, val }; body.elems.push_back(std::make_shared<node>( node{ st, {} } )); }
        list blockL; blockL.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    // rindex-addr: pointer to element (alias) -> (index %dst <elem-ty> %base %idx)
    tx.add_macro("rindex-addr", [](const list& form)->std::optional<node_ptr>{
        auto &e=form.elems; if(e.size()!=5) return std::nullopt; // rindex-addr %dst <elem-ty> %base %idx
        if(!std::holds_alternative<symbol>(e[1]->data)) return std::nullopt;
        list out; out.elems = { rl_make_sym("index"), e[1], e[2], e[3], e[4] };
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });
    // rindex: value load form.
    //   Modern: (rindex %dst <elem-ty> %base %idx) -> (block :body [ (index %t <elem-ty> %base %idx) (load %dst <elem-ty> %t) ])
    //   Legacy: (rindex %dst %base %idx)         -> elem-ty=i32 injected.
    tx.add_macro("rindex", [](const list& form)->std::optional<node_ptr>{
        auto &e=form.elems; if(e.size()!=5 && e.size()!=4) return std::nullopt; // name + args
        if(!std::holds_alternative<symbol>(e[1]->data)) return std::nullopt; // %dst symbol
        node_ptr dst = e[1]; node_ptr elemTy=nullptr; node_ptr base=nullptr; node_ptr idx=nullptr;
        if(e.size()==5){ elemTy=e[2]; base=e[3]; idx=e[4]; }
        else { // legacy 4 arg
            elemTy = rl_make_sym("i32"); base=e[2]; idx=e[3];
        }
        if(!std::holds_alternative<symbol>(base->data) || !std::holds_alternative<symbol>(idx->data)) return std::nullopt;
        auto tmp = rl_make_sym(rl_gensym("idx"));
        vector_t body;
        { list idxL; idxL.elems = { rl_make_sym("index"), tmp, elemTy, base, idx }; body.elems.push_back(std::make_shared<node>( node{ idxL, {} } )); }
        { list ld;   ld.elems  = { rl_make_sym("load"), dst, elemTy, tmp };        body.elems.push_back(std::make_shared<node>( node{ ld, {} } )); }
        list blockL; blockL.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    // rindex-load: (rindex-load %dst <elem-ty> %base %idx)
    tx.add_macro("rindex-load", [](const list& form)->std::optional<node_ptr>{
    auto &e=form.elems; if(e.size()!=5) return std::nullopt; // name + 4 args
    // All four operands must be symbols
    for(size_t i=1;i<5; ++i){ if(!std::holds_alternative<symbol>(e[i]->data)) return std::nullopt; }
    auto dst   = e[1];
    auto elemTy= e[2];
    auto base  = e[3];
    auto idx   = e[4];
        auto tmp = rl_make_sym(rl_gensym("idx"));
        vector_t body;
        { list idxL; idxL.elems = { rl_make_sym("index"), tmp, elemTy, base, idx }; body.elems.push_back(std::make_shared<node>( node{ idxL, {} } )); }
        { list ld; ld.elems = { rl_make_sym("load"), dst, elemTy, tmp }; body.elems.push_back(std::make_shared<node>( node{ ld, {} } )); }
        list blockL; blockL.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    // rindex-store: (rindex-store <elem-ty> %base %idx %src)
    tx.add_macro("rindex-store", [](const list& form)->std::optional<node_ptr>{
        auto &e=form.elems;
        // Accept either 5 elements (name + 4 args) or 6 (historical extra placeholder arg).
        if(e.size()!=5 && e.size()!=6) return std::nullopt;
        for(size_t i=1;i<e.size(); ++i){
            // If there's an extra 6th element and it's a keyword/vector payload from an older form, allow it (ignore).
            if(i==5) break; // ignore any trailing element beyond the expected 4 args
            if(!std::holds_alternative<symbol>(e[i]->data)) return std::nullopt;
        }
        auto elemTy = e[1];
        auto base   = e[2];
        auto idx    = e[3];
        auto src    = e[4];
        auto tmp = rl_make_sym(rl_gensym("idx"));
        vector_t body;
        { list idxL; idxL.elems = { rl_make_sym("index"), tmp, elemTy, base, idx }; body.elems.push_back(std::make_shared<node>( node{ idxL, {} } )); }
        { list st; st.elems = { rl_make_sym("store"), elemTy, tmp, src }; body.elems.push_back(std::make_shared<node>( node{ st, {} } )); }
        list blockL; blockL.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    // Pointer address-of and deref are direct core ops already recognized
    tx.add_macro("raddr", [](const list& form)->std::optional<node_ptr>{ auto &e=form.elems; if(e.size()!=4) return std::nullopt; list out; out.elems={ rl_make_sym("addr"), e[1], e[2], e[3] }; return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); });
    tx.add_macro("rderef", [](const list& form)->std::optional<node_ptr>{ auto &e=form.elems; if(e.size()!=4) return std::nullopt; list out; out.elems={ rl_make_sym("deref"), e[1], e[2], e[3] }; return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); });
}
} // namespace rustlite
