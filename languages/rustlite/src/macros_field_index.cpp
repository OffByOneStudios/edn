#include "rustlite/macros/context.hpp"
#include "rustlite/expand.hpp"
#include "edn/transform.hpp"
#include "rustlite/macros/helpers.hpp"
#include "rustlite/features.hpp"

using namespace edn;
namespace rustlite {
using rustlite::rl_make_sym;

void register_field_index_macros(edn::Transformer& tx, const std::shared_ptr<MacroContext>& ctx){
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
    // rindex-load: (rindex-load %dst <elem-ty> %base %idx [:len %lenSym])
    tx.add_macro("rindex-load", [ctx](const list& form)->std::optional<node_ptr>{
        auto &e=form.elems; if(e.size()<5) return std::nullopt;
        if(e.size()!=5 && e.size()!=7) return std::nullopt; // optional :len %sym
        for(size_t i=1;i<5; ++i){ if(!std::holds_alternative<symbol>(e[i]->data)) return std::nullopt; }
        node_ptr lenSym=nullptr;
        if(e.size()==7){
            if(!std::holds_alternative<keyword>(e[5]->data) || std::get<keyword>(e[5]->data).name!="len" || !std::holds_alternative<symbol>(e[6]->data)) return std::nullopt;
            lenSym = e[6];
        }
        auto dst=e[1], elemTy=e[2], base=e[3], idx=e[4];
        auto tmp = rl_make_sym(rl_gensym("idx"));
        vector_t body;
        { list idxL; idxL.elems = { rl_make_sym("index"), tmp, elemTy, base, idx }; body.elems.push_back(std::make_shared<node>( node{ idxL, {} } )); }
        bool doCheck = rustlite::bounds_checks_enabled();
        size_t inferredLen=0; bool haveInferred=false;
        if(doCheck){
            std::string b = std::get<symbol>(base->data).name; if(!b.empty() && b[0]=='%'){
                auto it = ctx->arrayLengths.find(b.substr(1)); if(it!=ctx->arrayLengths.end()){ inferredLen=it->second; haveInferred=true; }
            }
        }
        if(doCheck){
            node_ptr effectiveLen = lenSym ? lenSym : (haveInferred ? rl_make_i64((int64_t)inferredLen) : nullptr);
            if(effectiveLen){
                auto okSym = rl_make_sym(rl_gensym("idx.ok"));
                { list cmpL; cmpL.elems = { rl_make_sym("ult"), okSym, idx, effectiveLen }; body.elems.push_back(std::make_shared<node>( node{ cmpL, {} } )); }
                list thenB; vector_t thenBody;
                { list ld; ld.elems = { rl_make_sym("load"), dst, elemTy, tmp }; thenBody.elems.push_back(std::make_shared<node>( node{ ld, {} } )); }
                thenB.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ thenBody, {} } ) };
                list elseB; vector_t elseBody;
                { list panicL; panicL.elems = { rl_make_sym("panic") }; elseBody.elems.push_back(std::make_shared<node>( node{ panicL, {} } )); }
                { list zeroL; zeroL.elems = { rl_make_sym("const"), dst, elemTy, rl_make_i64(0) }; elseBody.elems.push_back(std::make_shared<node>( node{ zeroL, {} } )); }
                elseB.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ elseBody, {} } ) };
                { list ifL; ifL.elems = { rl_make_sym("if"), okSym, std::make_shared<node>( node{ thenB, {} } ), std::make_shared<node>( node{ elseB, {} } ) }; body.elems.push_back(std::make_shared<node>( node{ ifL, {} } )); }
                if(lenSym && haveInferred){
                    auto agreeSym = rl_make_sym(rl_gensym("len.ok"));
                    { list cmpEq; cmpEq.elems = { rl_make_sym("eq"), agreeSym, lenSym, rl_make_i64((int64_t)inferredLen) }; body.elems.push_back(std::make_shared<node>( node{ cmpEq, {} } )); }
                    { list assertL; assertL.elems = { rl_make_sym("rassert"), agreeSym }; body.elems.push_back(std::make_shared<node>( node{ assertL, {} } )); }
                }
                list blockL; blockL.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ body, {} } ) };
                return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
            }
        }
        { list ld; ld.elems = { rl_make_sym("load"), dst, elemTy, tmp }; body.elems.push_back(std::make_shared<node>( node{ ld, {} } )); }
        list blockL; blockL.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    // rindex-store: (rindex-store <elem-ty> %base %idx %src [:len %lenSym])
    tx.add_macro("rindex-store", [ctx](const list& form)->std::optional<node_ptr>{
        auto &e=form.elems; if(e.size()!=5 && e.size()!=7 && e.size()!=6) return std::nullopt; // allow legacy 6
        // Basic symbols
        for(size_t i=1;i<5 && i<e.size(); ++i){ if(!std::holds_alternative<symbol>(e[i]->data)) return std::nullopt; }
        node_ptr lenSym=nullptr; if(e.size()==7){
            if(!std::holds_alternative<keyword>(e[5]->data) || std::get<keyword>(e[5]->data).name!="len" || !std::holds_alternative<symbol>(e[6]->data)) return std::nullopt;
            lenSym = e[6];
        }
        auto elemTy=e[1], base=e[2], idx=e[3], src=e[4];
        auto tmp = rl_make_sym(rl_gensym("idx"));
        vector_t body; { list idxL; idxL.elems = { rl_make_sym("index"), tmp, elemTy, base, idx }; body.elems.push_back(std::make_shared<node>( node{ idxL, {} } )); }
        bool doCheck = rustlite::bounds_checks_enabled(); size_t inferredLen=0; bool haveInf=false;
        if(doCheck){
            std::string b = std::get<symbol>(base->data).name; if(!b.empty() && b[0]=='%'){
                auto it=ctx->arrayLengths.find(b.substr(1)); if(it!=ctx->arrayLengths.end()){ inferredLen=it->second; haveInf=true; }
            }
        }
        if(doCheck){
            node_ptr effLen = lenSym ? lenSym : (haveInf? rl_make_i64((int64_t)inferredLen) : nullptr);
            if(effLen){
                auto okSym = rl_make_sym(rl_gensym("idx.ok"));
                { list cmpL; cmpL.elems = { rl_make_sym("ult"), okSym, idx, effLen }; body.elems.push_back(std::make_shared<node>( node{ cmpL, {} } )); }
                list thenB; vector_t thenBody;
                { list st; st.elems = { rl_make_sym("store"), elemTy, tmp, src }; thenBody.elems.push_back(std::make_shared<node>( node{ st, {} } )); }
                thenB.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ thenBody, {} } ) };
                list elseB; vector_t elseBody;
                { list panicL; panicL.elems = { rl_make_sym("panic") }; elseBody.elems.push_back(std::make_shared<node>( node{ panicL, {} } )); }
                elseB.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ elseBody, {} } ) };
                { list ifL; ifL.elems = { rl_make_sym("if"), okSym, std::make_shared<node>( node{ thenB, {} } ), std::make_shared<node>( node{ elseB, {} } ) }; body.elems.push_back(std::make_shared<node>( node{ ifL, {} } )); }
                if(lenSym && haveInf){
                    auto agreeSym = rl_make_sym(rl_gensym("len.ok"));
                    { list cmpEq; cmpEq.elems = { rl_make_sym("eq"), agreeSym, lenSym, rl_make_i64((int64_t)inferredLen) }; body.elems.push_back(std::make_shared<node>( node{ cmpEq, {} } )); }
                    { list assertL; assertL.elems = { rl_make_sym("rassert"), agreeSym }; body.elems.push_back(std::make_shared<node>( node{ assertL, {} } )); }
                }
                list blockL; blockL.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ body, {} } ) };
                return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
            }
        }
        { list st; st.elems = { rl_make_sym("store"), elemTy, tmp, src }; body.elems.push_back(std::make_shared<node>( node{ st, {} } )); }
        list blockL; blockL.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    // Pointer address-of and deref are direct core ops already recognized
    tx.add_macro("raddr", [](const list& form)->std::optional<node_ptr>{ auto &e=form.elems; if(e.size()!=4) return std::nullopt; list out; out.elems={ rl_make_sym("addr"), e[1], e[2], e[3] }; return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); });
    tx.add_macro("rderef", [](const list& form)->std::optional<node_ptr>{ auto &e=form.elems; if(e.size()!=4) return std::nullopt; list out; out.elems={ rl_make_sym("deref"), e[1], e[2], e[3] }; return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); });
}
} // namespace rustlite
