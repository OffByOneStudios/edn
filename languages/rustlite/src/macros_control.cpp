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
    // rfor-range: sugar for counted for loop: (rfor-range %i Ty <start-int> <end-int> :body [ ... ])
    tx.add_macro("rfor-range", [](const list& form)->std::optional<node_ptr>{
        auto &el = form.elems; if(el.size() < 5) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data) || !std::holds_alternative<symbol>(el[2]->data)) return std::nullopt; // %i Ty
        auto loopVar = el[1]; auto tySym = el[2];
        // Two patterns:
        //   (rfor-range %i Ty <start-int> <end-int> :body [...])
        //   (rfor-range %i Ty %range :body [...]) where %range is a tuple (start,end,inclusive)
        bool rangeTupleMode = false;
        node_ptr bodyVec = nullptr;
        int64_t startV=0,endV=0; // for literal mode
        node_ptr rangeSym=nullptr;
        size_t idxBodySearchStart = 0;
        bool inclusiveLiteral=false;
        if(el.size() >= 6 && std::holds_alternative<int64_t>(el[3]->data) && std::holds_alternative<int64_t>(el[4]->data)){
            startV = std::get<int64_t>(el[3]->data); endV = std::get<int64_t>(el[4]->data); idxBodySearchStart = 5;
            // Optional keyword scanning for literal form (only :inclusive <bool>)
            for(size_t i=5;i+1<el.size(); i+=2){
                if(!std::holds_alternative<keyword>(el[i]->data)) break;
                auto kw = std::get<keyword>(el[i]->data).name;
                if(kw=="inclusive"){
                    if(std::holds_alternative<bool>(el[i+1]->data)) inclusiveLiteral = std::get<bool>(el[i+1]->data);
                    else if(std::holds_alternative<int64_t>(el[i+1]->data)) inclusiveLiteral = (std::get<int64_t>(el[i+1]->data)!=0);
                }
            }
        } else if(el.size() >= 5 && std::holds_alternative<symbol>(el[3]->data)) {
            rangeTupleMode = true; rangeSym = el[3]; idxBodySearchStart = 4; }
        else return std::nullopt;
        for(size_t i=idxBodySearchStart;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; if(std::get<keyword>(el[i]->data).name=="body") bodyVec = el[i+1]; }
        if(!bodyVec || !std::holds_alternative<vector_t>(bodyVec->data)) return std::nullopt;
        // Gensyms
        auto endSym = rl_make_sym(rl_gensym("end"));
        auto oneSym = rl_make_sym(rl_gensym("one"));
        auto condSym = rl_make_sym(rl_gensym("cond"));
        auto tmpSym = rl_make_sym(rl_gensym("next"));
        vector_t prologue; // only for tuple mode: extract start/end (ignore inclusive for now -> exclusive semantics)
        if(rangeTupleMode){
            // tget start
            auto startTmp = rl_make_sym(rl_gensym("rstart"));
            { list tgetL; tgetL.elems = { rl_make_sym("tget"), startTmp, tySym, rangeSym, rl_make_i64(0) }; prologue.elems.push_back(std::make_shared<node>( node{ tgetL, {} } )); }
            // tget end
            { list tgetL; tgetL.elems = { rl_make_sym("tget"), endSym, tySym, rangeSym, rl_make_i64(1) }; prologue.elems.push_back(std::make_shared<node>( node{ tgetL, {} } )); }
            // assign loop var from startTmp
            { list asL; asL.elems = { rl_make_sym("assign"), loopVar, startTmp }; prologue.elems.push_back(std::make_shared<node>( node{ asL, {} } )); }
            // const one
            { list c; c.elems = { rl_make_sym("const"), oneSym, tySym, rl_make_i64(1) }; prologue.elems.push_back(std::make_shared<node>( node{ c, {} } )); }
            // initial cond
            { list cmp; cmp.elems = { rl_make_sym("lt"), condSym, rl_make_sym("i1"), loopVar, endSym }; prologue.elems.push_back(std::make_shared<node>( node{ cmp, {} } )); }
            // Build step
            vector_t stepV;
            { list addL; addL.elems = { rl_make_sym("add"), tmpSym, tySym, loopVar, oneSym }; stepV.elems.push_back(std::make_shared<node>( node{ addL, {} } )); }
            { list asg; asg.elems = { rl_make_sym("assign"), loopVar, tmpSym }; stepV.elems.push_back(std::make_shared<node>( node{ asg, {} } )); }
            { list cmp; cmp.elems = { rl_make_sym("lt"), condSym, rl_make_sym("i1"), loopVar, endSym }; stepV.elems.push_back(std::make_shared<node>( node{ cmp, {} } )); }
            list forL; forL.elems = { rl_make_sym("for"), rl_make_kw("init"), std::make_shared<node>( node{ prologue, {} } ), rl_make_kw("cond"), condSym, rl_make_kw("step"), std::make_shared<node>( node{ stepV, {} } ), rl_make_kw("body"), bodyVec };
            return std::make_shared<node>( node{ forL, form.elems.front()->metadata } );
        }
        // Literal start/end mode
        vector_t initV;
        { list c; c.elems = { rl_make_sym("const"), loopVar, tySym, rl_make_i64(startV) }; initV.elems.push_back(std::make_shared<node>( node{ c, {} } )); }
        { list c; c.elems = { rl_make_sym("const"), endSym, tySym, rl_make_i64(endV) }; initV.elems.push_back(std::make_shared<node>( node{ c, {} } )); }
        { list c; c.elems = { rl_make_sym("const"), oneSym, tySym, rl_make_i64(1) }; initV.elems.push_back(std::make_shared<node>( node{ c, {} } )); }
    // Decide comparison op based on inclusive flag for literal form
    auto cmpOp = inclusiveLiteral ? std::string("le") : std::string("lt");
    { list cmp; cmp.elems = { rl_make_sym(cmpOp), condSym, rl_make_sym("i1"), loopVar, endSym }; initV.elems.push_back(std::make_shared<node>( node{ cmp, {} } )); }
        vector_t stepV;
        { list addL; addL.elems = { rl_make_sym("add"), tmpSym, tySym, loopVar, oneSym }; stepV.elems.push_back(std::make_shared<node>( node{ addL, {} } )); }
        { list asg; asg.elems = { rl_make_sym("assign"), loopVar, tmpSym }; stepV.elems.push_back(std::make_shared<node>( node{ asg, {} } )); }
    { list cmp; cmp.elems = { rl_make_sym(cmpOp), condSym, rl_make_sym("i1"), loopVar, endSym }; stepV.elems.push_back(std::make_shared<node>( node{ cmp, {} } )); }
        list forL; forL.elems = { rl_make_sym("for"), rl_make_kw("init"), std::make_shared<node>( node{ initV, {} } ), rl_make_kw("cond"), condSym, rl_make_kw("step"), std::make_shared<node>( node{ stepV, {} } ), rl_make_kw("body"), bodyVec };
        return std::make_shared<node>( node{ forL, form.elems.front()->metadata } );
    });
    // rrange: (rrange %dst Ty <start> <end> :inclusive <bool>) -> tuple [start end inclusive]
    tx.add_macro("rrange", [](const list& form)->std::optional<node_ptr>{
        auto &e = form.elems; if(e.size() < 5) return std::nullopt; // head %dst Ty start end ...
        if(!std::holds_alternative<symbol>(e[1]->data) || !std::holds_alternative<symbol>(e[2]->data)) return std::nullopt;
        auto dst = e[1]; auto tySym = e[2]; auto startNode = e[3]; auto endNode = e[4];
        // Optional :inclusive flag
        bool inclusive = false; // default exclusive
        for(size_t i=5;i+1<e.size(); i+=2){ if(!std::holds_alternative<keyword>(e[i]->data)) break; auto kw=std::get<keyword>(e[i]->data).name; if(kw=="inclusive"){ if(std::holds_alternative<bool>(e[i+1]->data)) inclusive = std::get<bool>(e[i+1]->data); else if(std::holds_alternative<int64_t>(e[i+1]->data)) inclusive = (std::get<int64_t>(e[i+1]->data)!=0); } }
        // Ensure start/end are symbols (emit const if literal ints)
        vector_t body;
        auto materialize = [&](node_ptr n)->node_ptr{
            if(std::holds_alternative<symbol>(n->data)) return n;
            if(std::holds_alternative<int64_t>(n->data)){
                auto tmp = rl_make_sym(rl_gensym("rval")); list c; c.elems = { rl_make_sym("const"), tmp, tySym, n }; body.elems.push_back(std::make_shared<node>( node{ c, {} } )); return tmp; }
            return n; // fallback: assume usable symbol-like
        };
        auto startSym = materialize(startNode);
        auto endSym = materialize(endNode);
        auto incSym = rl_make_sym(rl_gensym("inc")); { list c; c.elems = { rl_make_sym("const"), incSym, rl_make_sym("i1"), rl_make_i64(inclusive?1:0) }; body.elems.push_back(std::make_shared<node>( node{ c, {} } ) ); }
        { list tup; tup.elems = { rl_make_sym("tuple"), dst, std::make_shared<node>( node{ vector_t{ { startSym, endSym, incSym } }, {} } ) }; body.elems.push_back(std::make_shared<node>( node{ tup, {} } )); }
        list blockL; blockL.elems = { rl_make_sym("block"), rl_make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
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
    // rtry (Result / Option early return sugar)
    tx.add_macro("rtry", [](const list& form)->std::optional<node_ptr>{
        auto &el=form.elems; if(el.size()<4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data) || !std::holds_alternative<symbol>(el[2]->data)) return std::nullopt;
        auto bindNameSym = el[1]; auto sumTypeName = std::get<symbol>(el[2]->data).name; node_ptr sumExpr = el[3];
        bool isOption = sumTypeName.rfind("Option", 0) == 0; bool isResult = sumTypeName.rfind("Result", 0) == 0; if(!isOption && !isResult) return std::nullopt;
        std::string okV = isOption? "Some" : "Ok"; std::string errV = isOption? "None" : "Err";
        // Build match returning dummy i1 flag we ignore; early return on err/none.
        list matchL; matchL.elems = { rl_make_sym("match"), rl_make_sym(rl_gensym("rtry_dummy")), rl_make_sym("i1"), rl_make_sym(sumTypeName), sumExpr, rl_make_kw("cases") };
        vector_t casesV; {
            list caseL; caseL.elems = { rl_make_sym("case"), rl_make_sym(okV), rl_make_kw("binds") };
            vector_t bindsV; { list b; b.elems = { rl_make_sym("bind"), bindNameSym, rl_make_i64(0) }; bindsV.elems.push_back(std::make_shared<node>( node{ b, {} } )); }
            caseL.elems.push_back(std::make_shared<node>( node{ bindsV, {} } ));
            caseL.elems.push_back(rl_make_kw("body")); vector_t bodyV; { list oneC; oneC.elems = { rl_make_sym("const"), rl_make_sym(rl_gensym("one")), rl_make_sym("i1"), rl_make_i64(1) }; auto oneN=std::make_shared<node>( node{ oneC, {} } ); bodyV.elems.push_back(oneN); bodyV.elems.push_back(rl_make_kw("value")); bodyV.elems.push_back(std::get<list>(oneN->data).elems[1]); }
            caseL.elems.push_back(std::make_shared<node>( node{ bodyV, {} } )); casesV.elems.push_back(std::make_shared<node>( node{ caseL, {} } ));
        }
        matchL.elems.push_back(std::make_shared<node>( node{ casesV, {} } ));
        vector_t defV; {
            // Early return path: dereference the sum pointer value before returning so function ret expects value type.
            auto retTmpSym = rl_make_sym(rl_gensym("rtry.ret"));
            { list derefL; derefL.elems = { rl_make_sym("rderef"), retTmpSym, rl_make_sym(sumTypeName), sumExpr }; defV.elems.push_back(std::make_shared<node>( node{ derefL, {} } )); }
            { list retL; retL.elems = { rl_make_sym("ret"), rl_make_sym(sumTypeName), retTmpSym }; defV.elems.push_back(std::make_shared<node>( node{ retL, {} } )); }
            list zeroC; zeroC.elems = { rl_make_sym("const"), rl_make_sym(rl_gensym("zero")), rl_make_sym("i1"), rl_make_i64(0) }; auto zN=std::make_shared<node>( node{ zeroC, {} } ); defV.elems.push_back(zN); defV.elems.push_back(rl_make_kw("value")); defV.elems.push_back(std::get<list>(zN->data).elems[1]);
        }
        matchL.elems.push_back(rl_make_kw("default")); matchL.elems.push_back(std::make_shared<node>( node{ defV, {} } ));
        vector_t outV; outV.elems.push_back(std::make_shared<node>( node{ matchL, {} } )); list blockL; blockL.elems = { rl_make_sym("block"), rl_make_kw("body"), std::make_shared<node>( node{ outV, {} } ) }; return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
}

} // namespace rustlite
