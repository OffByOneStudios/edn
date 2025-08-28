#include "rustlite/macros/context.hpp"
#include "rustlite/expand.hpp"
#include "edn/transform.hpp"
#include <optional>
#include "rustlite/macros/helpers.hpp"

using namespace edn;
namespace rustlite {

// Using shared helper utilities from helpers.hpp
using rustlite::rl_make_sym; using rustlite::rl_make_kw; using rustlite::rl_make_i64; using rustlite::rl_gensym;

void register_extern_macros(edn::Transformer& tx, const std::shared_ptr<MacroContext>&){
    // rextern-global / rextern-const -> (global ... :external true)
    auto extern_global_macro = [](const list& form)->std::optional<node_ptr>{
        auto &el = form.elems; if(el.size()<3) return std::nullopt;
        node_ptr nameN=nullptr; node_ptr typeN=nullptr; node_ptr constN=nullptr; node_ptr initN=nullptr; node_ptr externalN=nullptr;
        for(size_t i=1;i+1<el.size(); i+=2){
            if(!std::holds_alternative<keyword>(el[i]->data)) break;
            auto kw = std::get<keyword>(el[i]->data).name; auto v = el[i+1];
            if(kw=="name") nameN=v; else if(kw=="type") typeN=v; else if(kw=="const") constN=v; else if(kw=="init") initN=v; else if(kw=="external") externalN=v;
        }
        if(!nameN || !typeN) return std::nullopt;
    list g; g.elems.push_back(rl_make_sym("global"));
    g.elems.push_back(rl_make_kw("name")); g.elems.push_back(nameN);
    g.elems.push_back(rl_make_kw("type")); g.elems.push_back(typeN);
    if(constN){ g.elems.push_back(rl_make_kw("const")); g.elems.push_back(constN); }
    if(initN){ g.elems.push_back(rl_make_kw("init")); g.elems.push_back(initN); }
    g.elems.push_back(rl_make_kw("external")); g.elems.push_back(std::make_shared<node>( node{ true, {} } ));
        return std::make_shared<node>( node{ g, form.elems.front()->metadata } );
    };
    tx.add_macro("rextern-global", extern_global_macro);
    tx.add_macro("rextern-const", extern_global_macro);
}

void register_var_control_macros(edn::Transformer& tx, const std::shared_ptr<MacroContext>&){
    // rlet / rmut
    tx.add_macro("rlet", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<5) return std::nullopt;
        node_ptr ty = el[1];
        if(!std::holds_alternative<symbol>(el[2]->data) || !std::holds_alternative<symbol>(el[3]->data)) return std::nullopt;
        node_ptr name = el[2]; node_ptr init = el[3]; node_ptr bodyVec=nullptr;
        for(size_t i=4;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; if(std::get<keyword>(el[i]->data).name=="body") bodyVec=el[i+1]; }
        if(!bodyVec || !std::holds_alternative<vector_t>(bodyVec->data)) return std::nullopt;
    vector_t outV; { list asOp; asOp.elems = { rl_make_sym("as"), name, ty, init }; outV.elems.push_back(std::make_shared<node>( node{ asOp, {} } )); }
        for(auto &e : std::get<vector_t>(bodyVec->data).elems) outV.elems.push_back(e);
    list blockL; blockL.elems = { rl_make_sym("block"), rl_make_kw("body"), std::make_shared<node>( node{ outV, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    tx.add_macro("rmut", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<5) return std::nullopt; node_ptr ty = el[1];
        if(!std::holds_alternative<symbol>(el[2]->data) || !std::holds_alternative<symbol>(el[3]->data)) return std::nullopt;
        node_ptr name = el[2]; node_ptr init = el[3]; node_ptr bodyVec=nullptr;
        for(size_t i=4;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; if(std::get<keyword>(el[i]->data).name=="body") bodyVec=el[i+1]; }
        if(!bodyVec || !std::holds_alternative<vector_t>(bodyVec->data)) return std::nullopt;
    vector_t outV; { list asOp; asOp.elems = { rl_make_sym("as"), name, ty, init }; outV.elems.push_back(std::make_shared<node>( node{ asOp, {} } )); }
        for(auto &e : std::get<vector_t>(bodyVec->data).elems) outV.elems.push_back(e);
    list blockL; blockL.elems = { rl_make_sym("block"), rl_make_kw("body"), std::make_shared<node>( node{ outV, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    // rif-let
    tx.add_macro("rif-let", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<6) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; node_ptr dst=el[1]; node_ptr retTy=el[2];
        if(!std::holds_alternative<symbol>(el[3]->data) || !std::holds_alternative<symbol>(el[4]->data)) return std::nullopt;
        auto sumName = std::get<symbol>(el[3]->data).name; auto variantName = std::get<symbol>(el[4]->data).name; node_ptr ptrNode = el[5];
        node_ptr bindVar=nullptr; node_ptr thenVec=nullptr; node_ptr elseVec=nullptr;
        for(size_t i=6;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; auto v=el[i+1]; if(kw=="bind") bindVar=v; else if(kw=="then") thenVec=v; else if(kw=="else") elseVec=v; }
        if(!bindVar || !thenVec || !elseVec) return std::nullopt;
        list matchL; matchL.elems = { rl_make_sym("match"), dst, retTy, rl_make_sym(sumName), ptrNode, rl_make_kw("cases") };
        vector_t casesV; {
            list caseL; caseL.elems = { rl_make_sym("case"), rl_make_sym(variantName), rl_make_kw("binds") };
            vector_t bindsV; { list b; b.elems = { rl_make_sym("bind"), bindVar, rl_make_i64(0) }; bindsV.elems.push_back(std::make_shared<node>( node{ b, {} } )); }
            caseL.elems.push_back(std::make_shared<node>( node{ bindsV, {} } ));
            caseL.elems.push_back(rl_make_kw("body")); caseL.elems.push_back(thenVec);
            casesV.elems.push_back(std::make_shared<node>( node{ caseL, {} } ));
        }
        matchL.elems.push_back(std::make_shared<node>( node{ casesV, {} } ));
        // Provide :default body directly as a vector (no wrapping (default ...)) per IR expectation
        matchL.elems.push_back(rl_make_kw("default")); matchL.elems.push_back(elseVec);
        return std::make_shared<node>( node{ matchL, form.elems.front()->metadata } );
    });
    // rif / relse
    auto toVec = [](node_ptr n)->node_ptr{ if(!n) return nullptr; if(std::holds_alternative<vector_t>(n->data)) return n; if(std::holds_alternative<list>(n->data)){ vector_t v; v.elems.push_back(n); return std::make_shared<node>( node{ v, {} } ); } return nullptr; };
    tx.add_macro("rif", [toVec](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<3) return std::nullopt; node_ptr cond=el[1]; node_ptr thenNode=nullptr; node_ptr elseNode=nullptr;
        for(size_t i=2;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; auto v=el[i+1]; if(kw=="then") thenNode=v; else if(kw=="else") elseNode=v; }
        if(!thenNode) return std::nullopt; node_ptr thenVec = toVec(thenNode); if(!thenVec) return std::nullopt; node_ptr elseVec = elseNode? toVec(elseNode):nullptr; if(elseNode && !elseVec) return std::nullopt;
    list ifL; ifL.elems = { rl_make_sym("if"), cond, thenVec }; if(elseVec) ifL.elems.push_back(elseVec); return std::make_shared<node>( node{ ifL, form.elems.front()->metadata } );
    });
    tx.add_macro("relse", [toVec](const list& form)->std::optional<node_ptr>{
        auto& el=form.elems; if(el.size()<3) return std::nullopt; node_ptr cond=el[1]; node_ptr thenNode=nullptr; node_ptr elseNode=nullptr;
        for(size_t i=2;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; auto v=el[i+1]; if(kw=="then") thenNode=v; else if(kw=="else") elseNode=v; }
        if(!thenNode) return std::nullopt; node_ptr thenVec = toVec(thenNode); if(!thenVec) return std::nullopt; node_ptr elseVec = elseNode? toVec(elseNode):nullptr; if(elseNode && !elseVec) return std::nullopt;
    list ifL; ifL.elems = { rl_make_sym("if"), cond, thenVec }; if(elseVec) ifL.elems.push_back(elseVec); return std::make_shared<node>( node{ ifL, form.elems.front()->metadata } );
    });
    // rwhile
    tx.add_macro("rwhile", [](const list& form)->std::optional<node_ptr>{
        auto& el=form.elems; if(el.size()<3) return std::nullopt; node_ptr cond=el[1]; node_ptr bodyVec=nullptr;
        for(size_t i=2;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; if(std::get<keyword>(el[i]->data).name=="body") bodyVec=el[i+1]; }
    if(!bodyVec || !std::holds_alternative<vector_t>(bodyVec->data)) return std::nullopt; list whileL; whileL.elems = { rl_make_sym("while"), cond, bodyVec }; return std::make_shared<node>( node{ whileL, form.elems.front()->metadata } );
    });
    // rbreak / rcontinue
    tx.add_macro("rbreak", [](const list& form)->std::optional<node_ptr>{ list l; l.elems = { rl_make_sym("break") }; return std::make_shared<node>( node{ l, form.elems.front()->metadata } ); });
    tx.add_macro("rcontinue", [](const list& form)->std::optional<node_ptr>{ list l; l.elems = { rl_make_sym("continue") }; return std::make_shared<node>( node{ l, form.elems.front()->metadata } ); });
    // rfor
    tx.add_macro("rfor", [](const list& form)->std::optional<node_ptr>{
        auto& el=form.elems; if(el.size()<3) return std::nullopt; node_ptr initV=nullptr, condN=nullptr, stepV=nullptr, bodyV=nullptr;
        for(size_t i=1;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; auto v=el[i+1]; if(kw=="init") initV=v; else if(kw=="cond") condN=v; else if(kw=="step") stepV=v; else if(kw=="body") bodyV=v; }
        if(!initV || !std::holds_alternative<vector_t>(initV->data) || !condN || !stepV || !std::holds_alternative<vector_t>(stepV->data) || !bodyV || !std::holds_alternative<vector_t>(bodyV->data)) return std::nullopt;
    list forL; forL.elems = { rl_make_sym("for"), rl_make_kw("init"), initV, rl_make_kw("cond"), condN, rl_make_kw("step"), stepV, rl_make_kw("body"), bodyV }; return std::make_shared<node>( node{ forL, form.elems.front()->metadata } );
    });
    // rloop: infinite loop sugar
    tx.add_macro("rloop", [](const list& form)->std::optional<node_ptr>{
        auto& el=form.elems; if(el.size()<2) return std::nullopt; node_ptr bodyVec=nullptr; for(size_t i=1;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; if(std::get<keyword>(el[i]->data).name=="body") bodyVec=el[i+1]; }
    if(!bodyVec || !std::holds_alternative<vector_t>(bodyVec->data)) return std::nullopt; vector_t outV; { list c; c.elems = { rl_make_sym("const"), rl_make_sym("%__rl_true"), rl_make_sym("i1"), rl_make_i64(1) }; outV.elems.push_back(std::make_shared<node>( node{ c, {} } )); }
    { list whileL; whileL.elems = { rl_make_sym("while"), rl_make_sym("%__rl_true"), bodyVec }; outV.elems.push_back(std::make_shared<node>( node{ whileL, {} } )); }
    list blockL; blockL.elems = { rl_make_sym("block"), rl_make_kw("body"), std::make_shared<node>( node{ outV, {} } ) }; return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    // rwhile-let
    tx.add_macro("rwhile-let", [](const list& form)->std::optional<node_ptr>{
        auto& el=form.elems; if(el.size()<5) return std::nullopt; if(!std::holds_alternative<symbol>(el[1]->data) || !std::holds_alternative<symbol>(el[2]->data)) return std::nullopt;
        auto sumName = std::get<symbol>(el[1]->data).name; auto variantName = std::get<symbol>(el[2]->data).name; node_ptr sumVal = el[3]; node_ptr bindVar=nullptr; node_ptr bodyVec=nullptr;
        for(size_t i=4;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; auto v=el[i+1]; if(kw=="bind") bindVar=v; else if(kw=="body") bodyVec=v; }
        if(!bindVar || !bodyVec || !std::holds_alternative<vector_t>(bodyVec->data)) return std::nullopt;
        vector_t outV; auto tName = rl_gensym("true"); { list c; c.elems = { rl_make_sym("const"), rl_make_sym(tName), rl_make_sym("i1"), rl_make_i64(1) }; outV.elems.push_back(std::make_shared<node>( node{ c, {} } )); }
        list matchL; matchL.elems = { rl_make_sym("match"), rl_make_sym(rl_gensym("m")), rl_make_sym("i1"), rl_make_sym(sumName), sumVal, rl_make_kw("cases") };
        vector_t casesV; {
            list caseL; caseL.elems = { rl_make_sym("case"), rl_make_sym(variantName), rl_make_kw("binds") };
            vector_t bindsV; { list b; b.elems = { rl_make_sym("bind"), bindVar, rl_make_i64(0) }; bindsV.elems.push_back(std::make_shared<node>( node{ b, {} } )); }
            caseL.elems.push_back(std::make_shared<node>( node{ bindsV, {} } ));
            caseL.elems.push_back(rl_make_kw("body")); vector_t thenV; for(auto &e : std::get<vector_t>(bodyVec->data).elems) thenV.elems.push_back(e);
            { list oneC; oneC.elems = { rl_make_sym("const"), rl_make_sym(rl_gensym("one")), rl_make_sym("i1"), rl_make_i64(1) }; auto oneN = std::make_shared<node>( node{ oneC, {} } ); thenV.elems.push_back(oneN); auto oneSym = std::get<list>(oneN->data).elems[1]; thenV.elems.push_back(rl_make_kw("value")); thenV.elems.push_back(oneSym); }
            caseL.elems.push_back(std::make_shared<node>( node{ thenV, {} } ));
            casesV.elems.push_back(std::make_shared<node>( node{ caseL, {} } ));
        }
        matchL.elems.push_back(std::make_shared<node>( node{ casesV, {} } ));
        // Provide :default directly as vector body (break + zero value) to signal termination path
        vector_t dv; {
            list br; br.elems = { rl_make_sym("break") }; dv.elems.push_back(std::make_shared<node>( node{ br, {} } ));
        } {
            list zeroC; zeroC.elems = { rl_make_sym("const"), rl_make_sym(rl_gensym("zero")), rl_make_sym("i1"), rl_make_i64(0) }; auto zN = std::make_shared<node>( node{ zeroC, {} } ); dv.elems.push_back(zN); auto zSym = std::get<list>(zN->data).elems[1]; dv.elems.push_back(rl_make_kw("value")); dv.elems.push_back(zSym);
        }
        matchL.elems.push_back(rl_make_kw("default")); matchL.elems.push_back(std::make_shared<node>( node{ dv, {} } ));
        { list whileL; whileL.elems = { rl_make_sym("while"), rl_make_sym(tName), std::make_shared<node>( node{ vector_t{ { std::make_shared<node>( node{ matchL, {} } ) } }, {} } ) }; outV.elems.push_back(std::make_shared<node>( node{ whileL, {} } )); }
        list blockL; blockL.elems = { rl_make_sym("block"), rl_make_kw("body"), std::make_shared<node>( node{ outV, {} } ) }; return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
}

} // namespace rustlite
