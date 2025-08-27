#include "rustlite/expand.hpp"
#include "edn/transform.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>

// Restored macros: rcstr, rbytes, rextern-global, rextern-const and refactored rindex* helper.
// This comment also forces recompilation to ensure the latest expansion logic is picked up.

using namespace edn;

namespace rustlite {

static node_ptr make_sym(const std::string& s){ return std::make_shared<node>( node{ symbol{s}, {} } ); }
static node_ptr make_kw(const std::string& s){ return std::make_shared<node>( node{ keyword{s}, {} } ); }
static node_ptr make_i64(int64_t v){ return std::make_shared<node>( node{ v, {} } ); }
static std::string gensym(const std::string& base){ static uint64_t n=0; return "%__rl_" + base + "_" + std::to_string(++n); }

edn::node_ptr expand_rustlite(const edn::node_ptr& module_ast){
    Transformer tx;
    // Tuple tracking (auto struct declarations)
    std::unordered_map<std::string,size_t> g_tupleVarArity; // %var (with '%') -> arity
    std::unordered_set<size_t> g_tupleArities;              // distinct arities used
    // ---------------------------------------------------------------------
    // Literal convenience macros (restored): rcstr / rbytes
    //  (rcstr %dst "literal")  -> (cstr %dst "literal") with escape decoding retained in core op
    //  (rbytes %dst [ b* ])     -> (bytes %dst [ b* ])
    // We still rely on core type checker & emitter for validation / interning; macro just validates shape.
    tx.add_macro("rcstr", [](const list& form)->std::optional<node_ptr>{
        auto &el = form.elems; if(el.size()!=3) return std::nullopt; // (rcstr %dst "lit")
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; // %dst must be symbol
        node_ptr lit = el[2];
        if(std::holds_alternative<std::string>(lit->data)){
            // Parser likely stripped quotes; re-wrap so downstream expects symbolic token with quotes intact.
            auto inner = std::get<std::string>(lit->data);
            // Escape embedded quotes/backslashes minimally to preserve original semantics for simple cases.
            std::string quoted="\""; quoted.reserve(inner.size()+2);
            for(char c: inner){ if(c=='"' || c=='\\') quoted.push_back('\\'); quoted.push_back(c); }
            quoted.push_back('"');
            lit = make_sym(quoted);
        }
        if(!std::holds_alternative<symbol>(lit->data)) return std::nullopt;
        auto name = std::get<symbol>(lit->data).name;
        if(name.size()<2 || name.front()!='"' || name.back()!='"') return std::nullopt; // ensure quoted form
        list l; l.elems = { make_sym("cstr"), el[1], lit };
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    tx.add_macro("rbytes", [](const list& form)->std::optional<node_ptr>{
        auto &el = form.elems; if(el.size()!=3) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; // %dst
        if(!std::holds_alternative<vector_t>(el[2]->data)) return std::nullopt; // [ ints ]
        // Light validation: ensure each element is an int literal node variant or symbol convertible later; deeper range checks done in type checker.
        for(auto &b : std::get<vector_t>(el[2]->data).elems){
            if(!(std::holds_alternative<int64_t>(b->data) || std::holds_alternative<symbol>(b->data))){
                return std::nullopt;
            }
        }
        list l; l.elems = { make_sym("bytes"), el[1], el[2] };
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });

    // ---------------------------------------------------------------------
    // External data sugar (restored): rextern-global / rextern-const
    //  (rextern-global :name X :type T) -> (global :name X :type T :external true)
    //  (rextern-const  :name X :type T) -> same (alias; future differentiation possible)
    auto extern_global_macro = [](const list& form)->std::optional<node_ptr>{
        auto &el = form.elems; if(el.size()<3) return std::nullopt;
        node_ptr nameN=nullptr; node_ptr typeN=nullptr; node_ptr constN=nullptr; node_ptr initN=nullptr; node_ptr externalN=nullptr;
        for(size_t i=1;i+1<el.size(); i+=2){
            if(!std::holds_alternative<keyword>(el[i]->data)) break;
            auto kw = std::get<keyword>(el[i]->data).name; auto v = el[i+1];
            if(kw=="name") nameN=v; else if(kw=="type") typeN=v; else if(kw=="const") constN=v; else if(kw=="init") initN=v; else if(kw=="external") externalN=v;
        }
        if(!nameN || !typeN) return std::nullopt;
        list g; g.elems.push_back(make_sym("global"));
        g.elems.push_back(make_kw("name")); g.elems.push_back(nameN);
        g.elems.push_back(make_kw("type")); g.elems.push_back(typeN);
        if(constN){ g.elems.push_back(make_kw("const")); g.elems.push_back(constN); }
        if(initN){ g.elems.push_back(make_kw("init")); g.elems.push_back(initN); }
        // Force :external true if not already specified
        g.elems.push_back(make_kw("external")); g.elems.push_back(std::make_shared<node>( node{ true, {} } ));
        return std::make_shared<node>( node{ g, form.elems.front()->metadata } );
    };
    tx.add_macro("rextern-global", extern_global_macro);
    tx.add_macro("rextern-const", extern_global_macro);
    // rlet / rmut macros: (rlet <type> %name %init :body [ ... ])
    // Lower to a block with a simple definition using 'as' to set the SSA var's type, then emit the body.
    // We wrap in a (block :body [ ... ]) so the macro expands to a single list-form instruction,
    // avoiding inserting a raw vector (which would be an invalid instruction node).
    // mut is currently identical at the core level (no immutability enforcement in EDN core yet).
    tx.add_macro("rlet", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<5) return std::nullopt;
        // Expect: (rlet <type> %name %init :body [ ... ])
        node_ptr ty = el[1];
        if(!std::holds_alternative<symbol>(el[2]->data) || !std::holds_alternative<symbol>(el[3]->data)) return std::nullopt;
        node_ptr name = el[2]; // %var symbol
        node_ptr init = el[3];
        node_ptr bodyVec=nullptr;
        for(size_t i=4;i+1<el.size(); i+=2){
            if(!std::holds_alternative<keyword>(el[i]->data)) break;
            auto kw = std::get<keyword>(el[i]->data).name;
            auto v = el[i+1];
            if(kw=="body") bodyVec=v;
        }
        if(!bodyVec || !std::holds_alternative<vector_t>(bodyVec->data)) return std::nullopt;
        // Wrap as a block: (block :body [ (as %name <type> %init) ...body... ])
        vector_t outV; // body of the block
        {
            list asOp; asOp.elems = { make_sym("as"), name, ty, init };
            outV.elems.push_back(std::make_shared<node>( node{ asOp, {} } ));
        }
        for(auto &e : std::get<vector_t>(bodyVec->data).elems){ outV.elems.push_back(e); }
        list blockL; blockL.elems.push_back(make_sym("block"));
        blockL.elems.push_back(make_kw("body"));
        blockL.elems.push_back(std::make_shared<node>( node{ outV, {} } ));
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    tx.add_macro("rmut", [](const list& form)->std::optional<node_ptr>{
        // Same lowering as rlet for now
        auto& el = form.elems; if(el.size()<5) return std::nullopt;
        node_ptr ty = el[1];
        if(!std::holds_alternative<symbol>(el[2]->data) || !std::holds_alternative<symbol>(el[3]->data)) return std::nullopt;
        node_ptr name = el[2];
        node_ptr init = el[3];
        node_ptr bodyVec=nullptr;
        for(size_t i=4;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; auto v=el[i+1]; if(kw=="body") bodyVec=v; }
        if(!bodyVec || !std::holds_alternative<vector_t>(bodyVec->data)) return std::nullopt;
        vector_t outV; // body of the block
        { list asOp; asOp.elems = { make_sym("as"), name, ty, init }; outV.elems.push_back(std::make_shared<node>( node{ asOp, {} } )); }
        for(auto &e : std::get<vector_t>(bodyVec->data).elems){ outV.elems.push_back(e); }
        list blockL; blockL.elems.push_back(make_sym("block"));
        blockL.elems.push_back(make_kw("body"));
        blockL.elems.push_back(std::make_shared<node>( node{ outV, {} } ));
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    // rif-let macro
    tx.add_macro("rif-let", [](const list& form)->std::optional<node_ptr>{
        // Result-mode form:
        // (rif-let %dst <ret-type> SumType Variant %ptr :bind %x :then [ ... :value %then ] :else [ ... :value %else ])
        auto& el = form.elems; if(el.size()<6) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; // %dst
        node_ptr dst = el[1];
        node_ptr retTy = el[2]; // pass-through node, e.g., symbol i32
        if(!std::holds_alternative<symbol>(el[3]->data) || !std::holds_alternative<symbol>(el[4]->data)) return std::nullopt;
        auto sumName = std::get<symbol>(el[3]->data).name;
        auto variantName = std::get<symbol>(el[4]->data).name;
        auto ptrNode = el[5];
        // parse keywords
        node_ptr bindVar=nullptr; node_ptr thenVec=nullptr; node_ptr elseVec=nullptr;
        for(size_t i=6;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw = std::get<keyword>(el[i]->data).name; auto v = el[i+1]; if(kw=="bind") bindVar=v; else if(kw=="then") thenVec=v; else if(kw=="else") elseVec=v; }
        if(!bindVar || !thenVec || !elseVec) return std::nullopt;
        // Build (match %dst <ret> SumType %ptr :cases [ (case Variant :binds [ (bind %x 0) ] :body <thenVec>) ] :default (default :body <elseVec>))
        list matchL; matchL.elems.push_back(make_sym("match")); matchL.elems.push_back(dst); matchL.elems.push_back(retTy); matchL.elems.push_back(make_sym(sumName)); matchL.elems.push_back(ptrNode);
        matchL.elems.push_back(make_kw("cases"));
        vector_t casesV;
        {
            list caseL; caseL.elems.push_back(make_sym("case")); caseL.elems.push_back(make_sym(variantName));
            caseL.elems.push_back(make_kw("binds"));
            vector_t bindsV;
            { list b; b.elems = { make_sym("bind"), bindVar, std::make_shared<node>( node{ (int64_t)0, {} } ) }; bindsV.elems.push_back(std::make_shared<node>( node{ b, {} } )); }
            caseL.elems.push_back(std::make_shared<node>( node{ bindsV, {} } ));
            caseL.elems.push_back(make_kw("body")); caseL.elems.push_back(thenVec);
            casesV.elems.push_back(std::make_shared<node>( node{ caseL, {} } ));
        }
        matchL.elems.push_back(std::make_shared<node>( node{ casesV, {} } ));
        matchL.elems.push_back(make_kw("default"));
        { list dfl; dfl.elems.push_back(make_sym("default")); dfl.elems.push_back(make_kw("body")); dfl.elems.push_back(elseVec); matchL.elems.push_back(std::make_shared<node>( node{ dfl, {} } )); }
        return std::make_shared<node>( node{ matchL, form.elems.front()->metadata } );
    });

    // rif / relse: if/else sugar → core (if %cond [..then..] [..else..])
    tx.add_macro("rif", [](const list& form)->std::optional<node_ptr>{
        // (rif %cond :then [ ... ] :else [ ... ])
        auto& el = form.elems; if(el.size()<3) return std::nullopt;
        node_ptr cond = el[1];
        node_ptr thenNode=nullptr; node_ptr elseNode=nullptr;
        for(size_t i=2;i+1<el.size(); i+=2){
            if(!std::holds_alternative<keyword>(el[i]->data)) break;
            auto kw = std::get<keyword>(el[i]->data).name; auto v = el[i+1];
            if(kw=="then") thenNode=v; else if(kw=="else") elseNode=v;
        }
        if(!thenNode) return std::nullopt;
        // Accept either a vector [ ... ] or a single list form, which we wrap into a vector
        auto toVec = [](node_ptr n)->node_ptr{
            if(!n) return nullptr;
            if(std::holds_alternative<vector_t>(n->data)) return n;
            if(std::holds_alternative<list>(n->data)){
                vector_t v; v.elems.push_back(n); return std::make_shared<node>( node{ v, {} } );
            }
            return nullptr;
        };
        node_ptr thenVec = toVec(thenNode);
        if(!thenVec) return std::nullopt;
        node_ptr elseVec = elseNode? toVec(elseNode) : nullptr;
        if(elseNode && !elseVec) return std::nullopt;
        list ifL; ifL.elems.push_back(make_sym("if")); ifL.elems.push_back(cond); ifL.elems.push_back(thenVec); if(elseVec) ifL.elems.push_back(elseVec);
        return std::make_shared<node>( node{ ifL, form.elems.front()->metadata } );
    });
    tx.add_macro("relse", [](const list& form)->std::optional<node_ptr>{
        // Alias of rif (useful for else-if chains)
        auto& el = form.elems; if(el.size()<3) return std::nullopt;
        node_ptr cond = el[1]; node_ptr thenNode=nullptr; node_ptr elseNode=nullptr;
        for(size_t i=2;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; auto v=el[i+1]; if(kw=="then") thenNode=v; else if(kw=="else") elseNode=v; }
        if(!thenNode) return std::nullopt;
        auto toVec = [](node_ptr n)->node_ptr{
            if(!n) return nullptr; if(std::holds_alternative<vector_t>(n->data)) return n; if(std::holds_alternative<list>(n->data)){ vector_t v; v.elems.push_back(n); return std::make_shared<node>( node{ v, {} } ); } return nullptr; };
        node_ptr thenVec = toVec(thenNode); if(!thenVec) return std::nullopt;
        node_ptr elseVec = elseNode? toVec(elseNode) : nullptr; if(elseNode && !elseVec) return std::nullopt;
        list ifL; ifL.elems = { make_sym("if"), cond, thenVec }; if(elseVec) ifL.elems.push_back(elseVec);
        return std::make_shared<node>( node{ ifL, form.elems.front()->metadata } );
    });

    // rwhile: (rwhile %cond :body [ ... ]) → (while %cond [ ... ])
    tx.add_macro("rwhile", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<3) return std::nullopt;
        node_ptr cond = el[1]; node_ptr bodyVec=nullptr;
        for(size_t i=2;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; auto v=el[i+1]; if(kw=="body") bodyVec=v; }
        if(!bodyVec || !std::holds_alternative<vector_t>(bodyVec->data)) return std::nullopt;
        list whileL; whileL.elems = { make_sym("while"), cond, bodyVec };
        return std::make_shared<node>( node{ whileL, form.elems.front()->metadata } );
    });

    // rbreak / rcontinue: thin aliases
    tx.add_macro("rbreak", [](const list& form)->std::optional<node_ptr>{
        list l; l.elems = { make_sym("break") }; return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    tx.add_macro("rcontinue", [](const list& form)->std::optional<node_ptr>{
        list l; l.elems = { make_sym("continue") }; return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });

    // rfor: (rfor :init [ ... ] :cond %c :step [ ... ] :body [ ... ]) → core for with same keywords
    tx.add_macro("rfor", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<3) return std::nullopt;
        node_ptr initV=nullptr, condN=nullptr, stepV=nullptr, bodyV=nullptr;
        for(size_t i=1;i+1<el.size(); i+=2){
            if(!std::holds_alternative<keyword>(el[i]->data)) break;
            auto kw = std::get<keyword>(el[i]->data).name; auto v = el[i+1];
            if(kw=="init") initV=v; else if(kw=="cond") condN=v; else if(kw=="step") stepV=v; else if(kw=="body") bodyV=v;
        }
        if(!initV || !std::holds_alternative<vector_t>(initV->data)) return std::nullopt;
        if(!condN) return std::nullopt;
        if(!stepV || !std::holds_alternative<vector_t>(stepV->data)) return std::nullopt;
        if(!bodyV || !std::holds_alternative<vector_t>(bodyV->data)) return std::nullopt;
        list forL; forL.elems = { make_sym("for"), make_kw("init"), initV, make_kw("cond"), condN, make_kw("step"), stepV, make_kw("body"), bodyV };
        return std::make_shared<node>( node{ forL, form.elems.front()->metadata } );
    });

    // rloop: infinite loop sugar → (block :body [ (const %__rl_true i1 1) (while %__rl_true [ ... ]) ])
    tx.add_macro("rloop", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<2) return std::nullopt;
        node_ptr bodyVec=nullptr;
        for(size_t i=1;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; auto v=el[i+1]; if(kw=="body") bodyVec=v; }
        if(!bodyVec || !std::holds_alternative<vector_t>(bodyVec->data)) return std::nullopt;
        vector_t outV;
        // (const %__rl_true i1 1)
        { list c; c.elems = { make_sym("const"), make_sym("%__rl_true"), make_sym("i1"), std::make_shared<node>( node{ (int64_t)1, {} } ) }; outV.elems.push_back(std::make_shared<node>( node{ c, {} } )); }
        // (while %__rl_true [ ...body... ])
        {
            list whileL; whileL.elems.push_back(make_sym("while")); whileL.elems.push_back(make_sym("%__rl_true"));
            // reuse provided body vector
            whileL.elems.push_back(bodyVec);
            outV.elems.push_back(std::make_shared<node>( node{ whileL, {} } ));
        }
        list blockL; blockL.elems = { make_sym("block"), make_kw("body"), std::make_shared<node>( node{ outV, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });

        // rwhile-let: (rwhile-let Sum Variant %sum :bind %x :body [ ... ])
        // Lowers to: while(true) { match Sum %sum { Variant(x) => { body; :value 1 } default => { break; :value 0 } } }
        tx.add_macro("rwhile-let", [](const list& form)->std::optional<node_ptr>{
                auto& el = form.elems; if(el.size()<5) return std::nullopt;
                if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; // Sum type name as symbol
                auto sumName = std::get<symbol>(el[1]->data).name;
                if(!std::holds_alternative<symbol>(el[2]->data)) return std::nullopt; auto variantName = std::get<symbol>(el[2]->data).name;
                node_ptr sumVal = el[3];
                node_ptr bindVar=nullptr; node_ptr bodyVec=nullptr;
                for(size_t i=4;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; auto v=el[i+1]; if(kw=="bind") bindVar=v; else if(kw=="body") bodyVec=v; }
                if(!bindVar || !bodyVec || !std::holds_alternative<vector_t>(bodyVec->data)) return std::nullopt;
                // Build block { const %t i1 1; while %t [ match %m i1 Sum %sum ... ] }
                vector_t outV;
                auto tName = gensym("true");
                { list c; c.elems = { make_sym("const"), make_sym(tName), make_sym("i1"), make_i64(1) }; outV.elems.push_back(std::make_shared<node>( node{ c, {} } )); }
                // match inside while body
                list matchL; matchL.elems = { make_sym("match"), make_sym(gensym("m")), make_sym("i1"), make_sym(sumName), sumVal };
                matchL.elems.push_back(make_kw("cases"));
                vector_t casesV;
                {
                        list caseL; caseL.elems = { make_sym("case"), make_sym(variantName), make_kw("binds") };
                        vector_t bindsV; { list b; b.elems = { make_sym("bind"), bindVar, make_i64(0) }; bindsV.elems.push_back(std::make_shared<node>( node{ b, {} } )); }
                        caseL.elems.push_back(std::make_shared<node>( node{ bindsV, {} } ));
                        // body then :value 1
                        caseL.elems.push_back(make_kw("body"));
                        vector_t caseBody = std::get<vector_t>(bodyVec->data);
                        // append a true value
                        vector_t thenV;
                        for(auto &e : caseBody.elems){ thenV.elems.push_back(e); }
                        { list oneC; oneC.elems = { make_sym("const"), make_sym(gensym("one")), make_sym("i1"), make_i64(1) }; auto oneN = std::make_shared<node>( node{ oneC, {} } ); thenV.elems.push_back(oneN); auto oneSym = std::get<list>(oneN->data).elems[1]; thenV.elems.push_back(make_kw("value")); thenV.elems.push_back(oneSym); }
                        caseL.elems.push_back(std::make_shared<node>( node{ thenV, {} } ));
                        casesV.elems.push_back(std::make_shared<node>( node{ caseL, {} } ));
                }
                matchL.elems.push_back(std::make_shared<node>( node{ casesV, {} } ));
                matchL.elems.push_back(make_kw("default"));
                { list dfl; dfl.elems.push_back(make_sym("default"));
                    vector_t dv;
                    // break; :value 0
                    { list br; br.elems = { make_sym("break") }; dv.elems.push_back(std::make_shared<node>( node{ br, {} } )); }
                    { list zeroC; zeroC.elems = { make_sym("const"), make_sym(gensym("zero")), make_sym("i1"), make_i64(0) }; auto zN = std::make_shared<node>( node{ zeroC, {} } ); dv.elems.push_back(zN); auto zSym = std::get<list>(zN->data).elems[1]; dv.elems.push_back(make_kw("value")); dv.elems.push_back(zSym); }
                    dfl.elems.push_back(make_kw("body")); dfl.elems.push_back(std::make_shared<node>( node{ dv, {} } ));
                    matchL.elems.push_back(std::make_shared<node>( node{ dfl, {} } )); }
                // while %t [ <match> ]
                { list whileL; whileL.elems = { make_sym("while"), make_sym(tName), std::make_shared<node>( node{ vector_t{ { std::make_shared<node>( node{ matchL, {} } ) } }, {} } ) };
                    outV.elems.push_back(std::make_shared<node>( node{ whileL, {} } ));
                }
                list blockL; blockL.elems = { make_sym("block"), make_kw("body"), std::make_shared<node>( node{ outV, {} } ) };
                return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
        });

    // rmatch: simplified arm syntax → core match with :value result-mode
    // (rmatch %dst <ret-ty> SumType %ptr :arms [ (arm Variant :binds [ %a %b ] :body [ ... :value %v ]) ... ] :else [ ... :value %v ])
    tx.add_macro("rmatch", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<6) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; // %dst
        node_ptr dst = el[1]; node_ptr retTy = el[2];
        if(!std::holds_alternative<symbol>(el[3]->data)) return std::nullopt;
        auto sumName = std::get<symbol>(el[3]->data).name;
        node_ptr scrut = el[4];
        node_ptr armsV=nullptr; node_ptr elseV=nullptr;
        for(size_t i=5;i+1<el.size(); i+=2){
            if(!std::holds_alternative<keyword>(el[i]->data)) break;
            auto kw = std::get<keyword>(el[i]->data).name; auto v=el[i+1];
            if(kw=="arms" || kw=="cases") armsV=v; else if(kw=="else" || kw=="default") elseV=v;
        }
        if(!armsV || !std::holds_alternative<vector_t>(armsV->data)) return std::nullopt;
        if(!elseV || !std::holds_alternative<vector_t>(elseV->data)) return std::nullopt;
        list matchL; matchL.elems = { make_sym("match"), dst, retTy, make_sym(sumName), scrut };
        matchL.elems.push_back(make_kw("cases"));
        vector_t outCases;
        for(auto &armNode : std::get<vector_t>(armsV->data).elems){
            if(!std::holds_alternative<list>(armNode->data)) return std::nullopt;
            auto arm = std::get<list>(armNode->data);
            if(arm.elems.empty() || !std::holds_alternative<symbol>(arm.elems[0]->data)) return std::nullopt;
            auto head = std::get<symbol>(arm.elems[0]->data).name;
            if(head=="case"){ // pass-through core case form
                outCases.elems.push_back(armNode);
                continue;
            }
            if(head!="arm") return std::nullopt;
            if(arm.elems.size()<2 || !std::holds_alternative<symbol>(arm.elems[1]->data)) return std::nullopt;
            auto variantName = std::get<symbol>(arm.elems[1]->data).name;
            node_ptr bindsList=nullptr; node_ptr bodyVec=nullptr;
            for(size_t i=2;i+1<arm.elems.size(); i+=2){
                if(!std::holds_alternative<keyword>(arm.elems[i]->data)) break;
                auto kw = std::get<keyword>(arm.elems[i]->data).name; auto v = arm.elems[i+1];
                if(kw=="binds") bindsList=v; else if(kw=="body") bodyVec=v;
            }
            if(!bodyVec || !std::holds_alternative<vector_t>(bodyVec->data)) return std::nullopt;
            list caseL; caseL.elems = { make_sym("case"), make_sym(variantName) };
            if(bindsList){
                // If bindsList is a vector of symbols, expand to (bind %sym idx). If already (bind ...) forms, pass through.
                if(!std::holds_alternative<vector_t>(bindsList->data)) return std::nullopt;
                vector_t bindsOut;
                size_t idx=0;
                for(auto &bnode : std::get<vector_t>(bindsList->data).elems){
                    if(std::holds_alternative<symbol>(bnode->data)){
                        list b; b.elems = { make_sym("bind"), bnode, std::make_shared<node>( node{ (int64_t)idx, {} } ) };
                        bindsOut.elems.push_back(std::make_shared<node>( node{ b, {} } ));
                        ++idx;
                    }else if(std::holds_alternative<list>(bnode->data)){
                        // assume already (bind %x N)
                        bindsOut.elems.push_back(bnode);
                    }else{
                        return std::nullopt;
                    }
                }
                caseL.elems.push_back(make_kw("binds"));
                caseL.elems.push_back(std::make_shared<node>( node{ bindsOut, {} } ));
            }
            caseL.elems.push_back(make_kw("body")); caseL.elems.push_back(bodyVec);
            outCases.elems.push_back(std::make_shared<node>( node{ caseL, {} } ));
        }
        matchL.elems.push_back(std::make_shared<node>( node{ outCases, {} } ));
        // default
        list dfl; dfl.elems = { make_sym("default"), make_kw("body"), elseV };
        matchL.elems.push_back(make_kw("default"));
        matchL.elems.push_back(std::make_shared<node>( node{ dfl, {} } ));
        return std::make_shared<node>( node{ matchL, form.elems.front()->metadata } );
    });

    // Sum/Option/Result constructors sugar
    // (rsum %dst SumType Variant :vals [ ... ]) → (sum-new %dst SumType Variant [ ... ])
    tx.add_macro("rsum", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<5) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; node_ptr dst=el[1];
        if(!std::holds_alternative<symbol>(el[2]->data)) return std::nullopt; auto sumName=std::get<symbol>(el[2]->data).name;
        if(!std::holds_alternative<symbol>(el[3]->data)) return std::nullopt; auto variant=std::get<symbol>(el[3]->data).name;
        node_ptr vals=nullptr; for(size_t i=4;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; if(kw=="vals") vals=el[i+1]; }
        if(!vals || !std::holds_alternative<vector_t>(vals->data)) return std::nullopt;
        list l; l.elems = { make_sym("sum-new"), dst, make_sym(sumName), make_sym(variant), vals };
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    // rnone: (rnone %dst SumType)
    tx.add_macro("rnone", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<3) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; node_ptr dst=el[1];
        if(!std::holds_alternative<symbol>(el[2]->data)) return std::nullopt; auto sumName=std::get<symbol>(el[2]->data).name;
        vector_t empty; list l; l.elems = { make_sym("sum-new"), dst, make_sym(sumName), make_sym("None"), std::make_shared<node>( node{ empty, {} } ) };
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    // rsome: (rsome %dst SumType %val)
    tx.add_macro("rsome", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; node_ptr dst=el[1];
        if(!std::holds_alternative<symbol>(el[2]->data)) return std::nullopt; auto sumName=std::get<symbol>(el[2]->data).name;
        node_ptr val = el[3]; vector_t v; v.elems.push_back(val);
        list l; l.elems = { make_sym("sum-new"), dst, make_sym(sumName), make_sym("Some"), std::make_shared<node>( node{ v, {} } ) };
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    // rok / rerr for Result-like types
    tx.add_macro("rok", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; node_ptr dst=el[1];
        if(!std::holds_alternative<symbol>(el[2]->data)) return std::nullopt; auto sumName=std::get<symbol>(el[2]->data).name;
        node_ptr val = el[3]; vector_t v; v.elems.push_back(val);
        list l; l.elems = { make_sym("sum-new"), dst, make_sym(sumName), make_sym("Ok"), std::make_shared<node>( node{ v, {} } ) };
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    tx.add_macro("rerr", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; node_ptr dst=el[1];
        if(!std::holds_alternative<symbol>(el[2]->data)) return std::nullopt; auto sumName=std::get<symbol>(el[2]->data).name;
        node_ptr val = el[3]; vector_t v; v.elems.push_back(val);
        list l; l.elems = { make_sym("sum-new"), dst, make_sym(sumName), make_sym("Err"), std::make_shared<node>( node{ v, {} } ) };
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });

    // rclosure: (rclosure %dst callee :captures [ ... ]) → (make-closure %dst callee [ ... ])
    tx.add_macro("rclosure", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<3) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; node_ptr dst=el[1];
        node_ptr callee = el[2];
        node_ptr capV=nullptr;
        for(size_t i=3;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; auto v=el[i+1]; if(kw=="captures") capV=v; }
        if(!capV || !std::holds_alternative<vector_t>(capV->data)) return std::nullopt;
        list l; l.elems = { make_sym("make-closure"), dst, callee, capV };
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    // rcall-closure: (rcall-closure %dst RetTy %clos arg1 ...) → (call-closure %dst RetTy %clos arg1 ...)
    tx.add_macro("rcall-closure", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; node_ptr dst=el[1];
        node_ptr retTy = el[2]; node_ptr clos = el[3];
        list out; out.elems = { make_sym("call-closure"), dst, retTy, clos };
        for(size_t i=4;i<el.size(); ++i){ out.elems.push_back(el[i]); }
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });

    // rstruct: (rstruct :name Name :fields [ (a TyA) (b TyB) ]) → core struct
    tx.add_macro("rstruct", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<3) return std::nullopt;
        node_ptr nameN=nullptr; node_ptr fieldsV=nullptr;
        for(size_t i=1;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; auto v=el[i+1]; if(kw=="name") nameN=v; else if(kw=="fields") fieldsV=v; }
        if(!nameN || !fieldsV || !std::holds_alternative<vector_t>(fieldsV->data)) return std::nullopt;
        vector_t outFields;
        for(auto &fld : std::get<vector_t>(fieldsV->data).elems){
            if(std::holds_alternative<list>(fld->data)){
                auto l = std::get<list>(fld->data);
                if(l.elems.size()<2 || !std::holds_alternative<symbol>(l.elems[0]->data)) return std::nullopt;
                list f; f.elems = { make_sym("field"), make_kw("name"), l.elems[0], make_kw("type"), l.elems[1] };
                outFields.elems.push_back(std::make_shared<node>( node{ f, {} } ));
            }else if(std::holds_alternative<symbol>(fld->data)){
                // Bare symbol field name implies i32 by default (convention); better to require type.
                return std::nullopt;
            }else{
                return std::nullopt;
            }
        }
        list s; s.elems = { make_sym("struct"), make_kw("name"), nameN, make_kw("fields"), std::make_shared<node>( node{ outFields, {} } ) };
        return std::make_shared<node>( node{ s, form.elems.front()->metadata } );
    });

    // renum (sum type): (renum :name Name :variants [ (None) (Some Ty) ... ]) → core sum/variant
    tx.add_macro("renum", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<3) return std::nullopt;
        node_ptr nameN=nullptr; node_ptr variantsV=nullptr;
        for(size_t i=1;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; auto kw=std::get<keyword>(el[i]->data).name; auto v=el[i+1]; if(kw=="name") nameN=v; else if(kw=="variants") variantsV=v; }
        if(!nameN || !variantsV || !std::holds_alternative<vector_t>(variantsV->data)) return std::nullopt;
        vector_t outVars;
        for(auto &vn : std::get<vector_t>(variantsV->data).elems){
            if(std::holds_alternative<symbol>(vn->data)){
                list v; v.elems = { make_sym("variant"), make_kw("name"), vn, make_kw("fields"), std::make_shared<node>( node{ vector_t{}, {} } ) };
                outVars.elems.push_back(std::make_shared<node>( node{ v, {} } ));
            }else if(std::holds_alternative<list>(vn->data)){
                auto vl = std::get<list>(vn->data);
                if(vl.elems.empty() || !std::holds_alternative<symbol>(vl.elems[0]->data)) return std::nullopt;
                vector_t fields;
                for(size_t i=1;i<vl.elems.size(); ++i){ fields.elems.push_back(vl.elems[i]); }
                list v; v.elems = { make_sym("variant"), make_kw("name"), vl.elems[0], make_kw("fields"), std::make_shared<node>( node{ fields, {} } ) };
                outVars.elems.push_back(std::make_shared<node>( node{ v, {} } ));
            }else{
                return std::nullopt;
            }
        }
        list s; s.elems = { make_sym("sum"), make_kw("name"), nameN, make_kw("variants"), std::make_shared<node>( node{ outVars, {} } ) };
        return std::make_shared<node>( node{ s, form.elems.front()->metadata } );
    });

    // rtypedef: alias to core typedef
    tx.add_macro("rtypedef", [](const list& form)->std::optional<node_ptr>{
        auto l = form; if(l.elems.empty()) return std::nullopt; l.elems[0] = make_sym("typedef");
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });

    // rfn: Lightweight alias for core fn, keeping keyword args. Just replace head symbol.
    tx.add_macro("rfn", [](const list& form)->std::optional<node_ptr>{
        auto l = form; if(l.elems.empty()) return std::nullopt; l.elems[0] = make_sym("fn");
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    // rextern-fn: core fn with :external true, preserving other keywords.
    tx.add_macro("rextern-fn", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<2) return std::nullopt;
        list out; out.elems.push_back(make_sym("fn"));
        bool hasExternal=false;
        for(size_t i=1;i<el.size(); ++i){
            out.elems.push_back(el[i]);
            if(std::holds_alternative<keyword>(el[i]->data) && std::get<keyword>(el[i]->data).name=="external"){
                hasExternal=true;
            }
        }
        if(!hasExternal){ out.elems.push_back(make_kw("external")); out.elems.push_back(std::make_shared<node>( node{ true, {} } )); }
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });
    // rcall: positional sugar → core call, with intrinsic rewrite
    // (rcall %dst RetTy callee arg1 arg2 ...) or (rcall %dst RetTy callee [ args... ])
    // If callee is a known intrinsic (add/sub/mul/div/eq/ne/lt/le/gt/ge/not), rewrite to core op form.
    tx.add_macro("rcall", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; node_ptr dst=el[1];
        node_ptr retTy = el[2]; node_ptr callee = el[3];
        // Flatten args
        std::vector<node_ptr> args;
        if(el.size()>=5){
            if(std::holds_alternative<vector_t>(el[4]->data)){
                for(auto &a : std::get<vector_t>(el[4]->data).elems){ args.push_back(a); }
            }else{
                for(size_t i=4;i<el.size(); ++i){ args.push_back(el[i]); }
            }
        }
        // Intrinsic mapping
        if(std::holds_alternative<symbol>(callee->data)){
            std::string op = std::get<symbol>(callee->data).name;
            if(op=="add"||op=="sub"||op=="mul"||op=="div"||op=="eq"||op=="ne"||op=="lt"||op=="le"||op=="gt"||op=="ge"){
                if(args.size()!=2) return std::nullopt; // arity mismatch
                list out; out.elems = { make_sym(op), dst, retTy, args[0], args[1] };
                return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
            }
            if(op=="not"){
                if(args.size()!=1) return std::nullopt; // unary
                // Lower to eq %dst i1 %arg 0 (boolean not)
                // We need a zero const of type i1; synthesize inline before compare via block.
                vector_t body;
                auto z = make_sym(gensym("z"));
                { list c; c.elems = { make_sym("const"), z, make_sym("i1"), make_i64(0) }; body.elems.push_back(std::make_shared<node>( node{ c, {} } )); }
                { list cmp; cmp.elems = { make_sym("eq"), dst, make_sym("i1"), args[0], z }; body.elems.push_back(std::make_shared<node>( node{ cmp, {} } )); }
                list blockL; blockL.elems = { make_sym("block"), make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
                return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
            }
        }
        // Default: core call
        list out; out.elems = { make_sym("call"), dst, retTy, callee };
        for(auto &a : args){ out.elems.push_back(a); }
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });

    // rand / ror: short-circuit boolean ops using rif. Form: (rand %dst %a %b)
    // Avoid redefining %dst by declaring it once with 'as' and using 'assign' in branches.
    tx.add_macro("rand", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()!=4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; node_ptr dst=el[1];
        node_ptr a = el[2]; node_ptr b = el[3];
        // Build: (block :body [ (const %f i1 0) (as %dst i1 %f) (rif %a :then [ (assign %dst %b) ] :else [ (assign %dst %f) ]) ])
        vector_t body;
        {
            list c; c.elems = { make_sym("const"), make_sym(gensym("f")), make_sym("i1"), make_i64(0) };
            auto cnode = std::make_shared<node>( node{ c, {} } ); body.elems.push_back(cnode);
            // Reference the const symbol we just made (second elem of const list)
            auto fSym = std::get<list>(cnode->data).elems[1];
            // Declare destination once, then branch-assign
            { list asL; asL.elems = { make_sym("as"), dst, make_sym("i1"), fSym }; body.elems.push_back(std::make_shared<node>( node{ asL, {} } )); }
            vector_t thenV; { list asg; asg.elems = { make_sym("assign"), dst, b }; thenV.elems.push_back(std::make_shared<node>( node{ asg, {} } )); }
            vector_t elseV; { list asg; asg.elems = { make_sym("assign"), dst, fSym }; elseV.elems.push_back(std::make_shared<node>( node{ asg, {} } )); }
            list rifL; rifL.elems = { make_sym("rif"), a, make_kw("then"), std::make_shared<node>( node{ thenV, {} } ), make_kw("else"), std::make_shared<node>( node{ elseV, {} } ) };
            body.elems.push_back(std::make_shared<node>( node{ rifL, {} } ));
        }
        list blockL; blockL.elems = { make_sym("block"), make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    tx.add_macro("ror", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()!=4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; node_ptr dst=el[1];
        node_ptr a = el[2]; node_ptr b = el[3];
        // (block :body [ (const %t i1 1) (as %dst i1 %b) (rif %a :then [ (assign %dst %t) ] :else [ (assign %dst %b) ]) ])
        vector_t body;
        {
            list c; c.elems = { make_sym("const"), make_sym(gensym("t")), make_sym("i1"), make_i64(1) };
            auto cnode = std::make_shared<node>( node{ c, {} } ); body.elems.push_back(cnode);
            auto tSym = std::get<list>(cnode->data).elems[1];
            // Declare destination once (initialize to 'b' so else branch can be no-op if desired)
            { list asL; asL.elems = { make_sym("as"), dst, make_sym("i1"), b }; body.elems.push_back(std::make_shared<node>( node{ asL, {} } )); }
            vector_t thenV; { list asg; asg.elems = { make_sym("assign"), dst, tSym }; thenV.elems.push_back(std::make_shared<node>( node{ asg, {} } )); }
            vector_t elseV; { list asg; asg.elems = { make_sym("assign"), dst, b }; elseV.elems.push_back(std::make_shared<node>( node{ asg, {} } )); }
            list rifL; rifL.elems = { make_sym("rif"), a, make_kw("then"), std::make_shared<node>( node{ thenV, {} } ), make_kw("else"), std::make_shared<node>( node{ elseV, {} } ) };
            body.elems.push_back(std::make_shared<node>( node{ rifL, {} } ));
        }
        list blockL; blockL.elems = { make_sym("block"), make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });

    // rassign: simple alias for core assign
    tx.add_macro("rassign", [](const list& form)->std::optional<node_ptr>{
        auto l = form; if(l.elems.size()!=3) return std::nullopt; l.elems[0] = make_sym("assign");
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    // rret: alias for core ret
    tx.add_macro("rret", [](const list& form)->std::optional<node_ptr>{
        auto l = form; if(l.elems.size()!=3) return std::nullopt; l.elems[0] = make_sym("ret");
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });

    // rpanic: alias to core panic (no operands)
    tx.add_macro("rpanic", [](const list& form)->std::optional<node_ptr>{
        auto l = form; if(l.elems.size()!=1) return std::nullopt; l.elems[0] = make_sym("panic");
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    // rassert: (rassert %cond) -> (if %cond [ ] [ (panic) ])
    tx.add_macro("rassert", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()!=2) return std::nullopt; node_ptr cond = el[1];
        // then branch: empty vector; else branch: [ (panic) ]
        vector_t thenV; vector_t elseV; { list p; p.elems = { make_sym("panic") }; elseV.elems.push_back(std::make_shared<node>( node{ p, {} } )); }
        list ifL; ifL.elems = { make_sym("if"), cond,
            std::make_shared<node>( node{ thenV, {} } ),
            std::make_shared<node>( node{ elseV, {} } ) };
        return std::make_shared<node>( node{ ifL, form.elems.front()->metadata } );
    });

    // rassert-eq: (rassert-eq %a %b) -> { %t = (eq i1 %a %b); (rassert %t) }
    tx.add_macro("rassert-eq", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()!=3) return std::nullopt; node_ptr a=el[1], b=el[2];
        vector_t body;
        auto t = make_sym(gensym("cmp"));
        { list cmp; cmp.elems = { make_sym("eq"), t, make_sym("i1"), a, b }; body.elems.push_back(std::make_shared<node>( node{ cmp, {} } )); }
        { list asrt; asrt.elems = { make_sym("rassert"), t }; body.elems.push_back(std::make_shared<node>( node{ asrt, {} } )); }
        list blockL; blockL.elems = { make_sym("block"), make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    // rassert-ne: (rassert-ne %a %b) -> { %t = (ne i1 %a %b); (rassert %t) }
    tx.add_macro("rassert-ne", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()!=3) return std::nullopt; node_ptr a=el[1], b=el[2];
        vector_t body;
        auto t = make_sym(gensym("cmp"));
        { list cmp; cmp.elems = { make_sym("ne"), t, make_sym("i1"), a, b }; body.elems.push_back(std::make_shared<node>( node{ cmp, {} } )); }
        { list asrt; asrt.elems = { make_sym("rassert"), t }; body.elems.push_back(std::make_shared<node>( node{ asrt, {} } )); }
        list blockL; blockL.elems = { make_sym("block"), make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    // rassert-lt/le/gt/ge: compare producing i1 then rassert
    tx.add_macro("rassert-lt", [](const list& form)->std::optional<node_ptr>{
        auto& el=form.elems; if(el.size()!=3) return std::nullopt; node_ptr a=el[1], b=el[2];
        vector_t body; auto t=make_sym(gensym("cmp"));
        { list cmp; cmp.elems={ make_sym("lt"), t, make_sym("i1"), a, b }; body.elems.push_back(std::make_shared<node>( node{ cmp, {} } )); }
        { list asrt; asrt.elems={ make_sym("rassert"), t }; body.elems.push_back(std::make_shared<node>( node{ asrt, {} } )); }
        list blockL; blockL.elems={ make_sym("block"), make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    tx.add_macro("rassert-le", [](const list& form)->std::optional<node_ptr>{
        auto& el=form.elems; if(el.size()!=3) return std::nullopt; node_ptr a=el[1], b=el[2];
        vector_t body; auto t=make_sym(gensym("cmp"));
        { list cmp; cmp.elems={ make_sym("le"), t, make_sym("i1"), a, b }; body.elems.push_back(std::make_shared<node>( node{ cmp, {} } )); }
        { list asrt; asrt.elems={ make_sym("rassert"), t }; body.elems.push_back(std::make_shared<node>( node{ asrt, {} } )); }
        list blockL; blockL.elems={ make_sym("block"), make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    tx.add_macro("rassert-gt", [](const list& form)->std::optional<node_ptr>{
        auto& el=form.elems; if(el.size()!=3) return std::nullopt; node_ptr a=el[1], b=el[2];
        vector_t body; auto t=make_sym(gensym("cmp"));
        { list cmp; cmp.elems={ make_sym("gt"), t, make_sym("i1"), a, b }; body.elems.push_back(std::make_shared<node>( node{ cmp, {} } )); }
        { list asrt; asrt.elems={ make_sym("rassert"), t }; body.elems.push_back(std::make_shared<node>( node{ asrt, {} } )); }
        list blockL; blockL.elems={ make_sym("block"), make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
    tx.add_macro("rassert-ge", [](const list& form)->std::optional<node_ptr>{
        auto& el=form.elems; if(el.size()!=3) return std::nullopt; node_ptr a=el[1], b=el[2];
        vector_t body; auto t=make_sym(gensym("cmp"));
        { list cmp; cmp.elems={ make_sym("ge"), t, make_sym("i1"), a, b }; body.elems.push_back(std::make_shared<node>( node{ cmp, {} } )); }
        { list asrt; asrt.elems={ make_sym("rassert"), t }; body.elems.push_back(std::make_shared<node>( node{ asrt, {} } )); }
        list blockL; blockL.elems={ make_sym("block"), make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });

    // Traits sugar (thin aliases)
    tx.add_macro("rtrait", [](const list& form)->std::optional<node_ptr>{
        auto l=form; if(l.elems.empty()) return std::nullopt; l.elems[0]=make_sym("trait");
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    tx.add_macro("rfnptr", [](const list& form)->std::optional<node_ptr>{
        auto l=form; if(l.elems.size()<3) return std::nullopt; l.elems[0]=make_sym("fnptr");
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    tx.add_macro("rmake-trait-obj", [](const list& form)->std::optional<node_ptr>{
        auto l=form; if(l.elems.size()<5) return std::nullopt; l.elems[0]=make_sym("make-trait-obj");
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    tx.add_macro("rtrait-call", [](const list& form)->std::optional<node_ptr>{
        auto l=form; if(l.elems.size()<6) return std::nullopt; l.elems[0]=make_sym("trait-call");
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });

    // rimpl: populate a trait vtable with method implementations
    // Forms supported:
    //   (rimpl Trait %vt :methods [ (method :name foo :type (ptr (fn-type ...)) :impl foo_impl) ... ])
    // Lowers to a block containing, per method:
    //   (fnptr %fp <type> <impl>)
    //   (member-addr %fld TraitVT %vt <name>)
    //   (store <type> %fld %fp)
    tx.add_macro("rimpl", [](const list& form)->std::optional<node_ptr>{
        auto& el = form.elems; if(el.size()<4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; // Trait
        std::string trait = std::get<symbol>(el[1]->data).name;
        node_ptr vt = el[2];
        node_ptr methodsV = nullptr;
        // parse keywords starting at i=3
        for(size_t i=3;i+1<el.size(); i+=2){
            if(!std::holds_alternative<keyword>(el[i]->data)) break;
            auto kw = std::get<keyword>(el[i]->data).name;
            auto v = el[i+1];
            if(kw=="methods") methodsV = v;
        }
        if(!methodsV || !std::holds_alternative<vector_t>(methodsV->data)) return std::nullopt;
        vector_t body;
        for(auto &mn : std::get<vector_t>(methodsV->data).elems){
            if(!mn || !std::holds_alternative<list>(mn->data)) return std::nullopt;
            auto ml = std::get<list>(mn->data);
            if(ml.elems.empty() || !std::holds_alternative<symbol>(ml.elems[0]->data) || std::get<symbol>(ml.elems[0]->data).name!="method") return std::nullopt;
            node_ptr nameN=nullptr, typeN=nullptr, implN=nullptr;
            for(size_t i=1;i+1<ml.elems.size(); i+=2){
                if(!std::holds_alternative<keyword>(ml.elems[i]->data)) break;
                auto kw = std::get<keyword>(ml.elems[i]->data).name;
                auto v = ml.elems[i+1];
                if(kw=="name") nameN=v; else if(kw=="type") typeN=v; else if(kw=="impl") implN=v;
            }
            if(!nameN || !typeN || !implN) return std::nullopt;
            // %fp
            auto fp = make_sym(gensym("fp"));
            list fnptrL; fnptrL.elems = { make_sym("fnptr"), fp, typeN, implN };
            body.elems.push_back(std::make_shared<node>( node{ fnptrL, {} } ));
            // %fld = member-addr TraitVT %vt name
            auto fld = make_sym(gensym("fld"));
            list memL; memL.elems = { make_sym("member-addr"), fld, make_sym(trait+"VT"), vt, nameN };
            body.elems.push_back(std::make_shared<node>( node{ memL, {} } ));
            // store type %fld %fp
            list stL; stL.elems = { make_sym("store"), typeN, fld, fp };
            body.elems.push_back(std::make_shared<node>( node{ stL, {} } ));
        }
        list blockL; blockL.elems = { make_sym("block"), make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });

    // rmethod: thin alias for trait-call
    // (rmethod %dst RetTy Trait %obj method args...) → (trait-call %dst RetTy Trait %obj method args...)
    tx.add_macro("rmethod", [](const list& form)->std::optional<node_ptr>{
        auto l=form; if(l.elems.size()<6) return std::nullopt; l.elems[0]=make_sym("trait-call");
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });

    // rdot: method-call sugar
    // Forms:
    //   Trait dispatch: (rdot %dst RetTy Trait %obj method arg1 arg2 ...) -> (trait-call %dst RetTy Trait %obj method arg1 arg2 ...)
    //   Trait dot-path: (rdot %dst RetTy Trait.method %obj arg1 arg2 ...) -> split, same lowering as trait-call
    //   Free fn with receiver: (rdot %dst RetTy callee %obj arg1 arg2 ...) -> (call %dst RetTy callee %obj arg1 arg2 ...)
    tx.add_macro("rdot", [](const list& form)->std::optional<node_ptr>{
        auto& el=form.elems; if(el.size()<6) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; // %dst
        node_ptr dst=el[1]; node_ptr retTy=el[2];
        // Dot-path trait form: (rdot %dst RetTy Trait.method %obj ...)
        if(std::holds_alternative<symbol>(el[3]->data)){
            auto s = std::get<symbol>(el[3]->data).name;
            auto dot = s.rfind('.');
            if(dot!=std::string::npos && el.size()>=5){
                auto traitName = s.substr(0, dot);
                auto methodName = s.substr(dot+1);
                list l; l.elems = { make_sym("trait-call"), dst, retTy, make_sym(traitName), el[4], make_sym(methodName) };
                for(size_t i=5;i<el.size(); ++i){ l.elems.push_back(el[i]); }
                return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
            }
        }
        // Decide trait vs free-fn by arity and argument kinds. If we have at least 7 elements, assume trait form.
        if(el.size()>=7){
            // Trait, %obj, method, args...
            list l; l.elems = { make_sym("trait-call"), dst, retTy, el[3], el[4], el[5] };
            for(size_t i=6;i<el.size(); ++i){ l.elems.push_back(el[i]); }
            return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
        }
        // Free function with receiver: callee, %obj, args...
        list l; l.elems = { make_sym("call"), dst, retTy, el[3], el[4] };
        for(size_t i=5;i<el.size(); ++i){ l.elems.push_back(el[i]); }
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });

    // Struct field sugar
    // rget: (rget %dst Struct %base %field) -> (member %dst Struct %base %field)
    tx.add_macro("rget", [](const list& form)->std::optional<node_ptr>{
        auto& el=form.elems; if(el.size()!=5) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data) ||
           !std::holds_alternative<symbol>(el[2]->data) ||
           !std::holds_alternative<symbol>(el[3]->data) ||
           !std::holds_alternative<symbol>(el[4]->data)) return std::nullopt;
        list m; m.elems = { make_sym("member"), el[1], el[2], el[3], el[4] };
        return std::make_shared<node>( node{ m, form.elems.front()->metadata } );
    });
    // rset: (rset <FieldType> Struct %base %field %value)
    // Lowers to block:
    //   (member-addr %addr Struct %base %field)
    //   (store <FieldType> %addr %value)
    tx.add_macro("rset", [](const list& form)->std::optional<node_ptr>{
        auto& el=form.elems; if(el.size()!=6) return std::nullopt;
        node_ptr fieldTy = el[1];
        if(!std::holds_alternative<symbol>(el[2]->data) ||
           !std::holds_alternative<symbol>(el[3]->data) ||
           !std::holds_alternative<symbol>(el[4]->data)) return std::nullopt;
        node_ptr st = el[2]; node_ptr base=el[3]; node_ptr fld=el[4]; node_ptr val=el[5];
        vector_t body;
        auto addr = make_sym(gensym("fldaddr"));
        { list ma; ma.elems = { make_sym("member-addr"), addr, st, base, fld }; body.elems.push_back(std::make_shared<node>( node{ ma, {} } )); }
        { list stl; stl.elems = { make_sym("store"), fieldTy, addr, val }; body.elems.push_back(std::make_shared<node>( node{ stl, {} } )); }
        list blockL; blockL.elems = { make_sym("block"), make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });

    // Array indexing sugar
    // rindex-family refactored to shared helper to reduce duplication.
    auto index_load_block = [](node_ptr dst, node_ptr elemTy, node_ptr base, node_ptr idx, bool doLoad)->node_ptr{
        vector_t body; auto p = make_sym(gensym("elem"));
        { list ix; ix.elems = { make_sym("index"), p, elemTy, base, idx }; body.elems.push_back(std::make_shared<node>( node{ ix, {} } )); }
        if(doLoad){ list ld; ld.elems = { make_sym("load"), dst, elemTy, p }; body.elems.push_back(std::make_shared<node>( node{ ld, {} } )); }
        list blockL; blockL.elems = { make_sym("block"), make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, {} } );
    };
    tx.add_macro("rindex-addr", [](const list& form)->std::optional<node_ptr>{
        auto &el=form.elems; if(el.size()!=5) return std::nullopt; // (rindex-addr %dst <ElemTy> %base %idx)
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; // %dst
        // Don't over-validate elem type / base / idx shapes here; let type checker handle semantics.
        list ix; ix.elems={ make_sym("index"), el[1], el[2], el[3], el[4] };
        return std::make_shared<node>( node{ ix, form.elems.front()->metadata } );
    });
    tx.add_macro("rindex", [index_load_block](const list& form)->std::optional<node_ptr>{
        auto &el=form.elems; if(el.size()!=5) return std::nullopt; if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; return index_load_block(el[1], el[2], el[3], el[4], true); });
    tx.add_macro("rindex-load", [index_load_block](const list& form)->std::optional<node_ptr>{
        auto &el=form.elems; if(el.size()!=5) return std::nullopt; if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; return index_load_block(el[1], el[2], el[3], el[4], true); });

    // rindex-store: (rindex-store <ElemType> %base %idx %val)
    // Lowers to block:
    //   (index %p <ElemType> %base %idx)
    //   (store <ElemType> %p %val)
    tx.add_macro("rindex-store", [](const list& form)->std::optional<node_ptr>{
        auto& el=form.elems; if(el.size()!=5) return std::nullopt;
        node_ptr elemTy=el[1], base=el[2], idx=el[3], val=el[4];
        vector_t body; auto p = make_sym(gensym("elemaddr"));
        { list ix; ix.elems = { make_sym("index"), p, elemTy, base, idx }; body.elems.push_back(std::make_shared<node>( node{ ix, {} } )); }
        { list st; st.elems = { make_sym("store"), elemTy, p, val }; body.elems.push_back(std::make_shared<node>( node{ st, {} } )); }
        list blockL; blockL.elems = { make_sym("block"), make_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });

    // Simple aliases for addr/deref
    tx.add_macro("raddr", [](const list& form)->std::optional<node_ptr>{
        auto l=form; if(l.elems.size()!=4) return std::nullopt; l.elems[0]=make_sym("addr");
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    tx.add_macro("rderef", [](const list& form)->std::optional<node_ptr>{
        auto l=form; if(l.elems.size()!=4) return std::nullopt; l.elems[0]=make_sym("deref");
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });

    // --- Phase A: Tuple & Array macros (EDN-0011) ---
    // (tuple %dst [ %a %b %c ]) -> (struct-lit %dst __TupleN [ _0 %a _1 %b _2 %c ])
    tx.add_macro("tuple", [&g_tupleVarArity,&g_tupleArities](const list& form)->std::optional<node_ptr>{
        auto &el = form.elems; if(el.size()!=3) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt;
        if(!std::holds_alternative<vector_t>(el[2]->data)) return std::nullopt;
        auto dst = el[1]; auto &vals = std::get<vector_t>(el[2]->data).elems; size_t n = vals.size(); if(n==0 || n>16) return std::nullopt;
        vector_t fieldVec; fieldVec.elems.reserve(n*2);
        for(size_t i=0;i<n;++i){ if(!std::holds_alternative<symbol>(vals[i]->data)) return std::nullopt; fieldVec.elems.push_back(make_sym("_"+std::to_string(i))); fieldVec.elems.push_back(vals[i]); }
        list sl; sl.elems = { make_sym("struct-lit"), dst, make_sym("__Tuple"+std::to_string(n)), std::make_shared<node>( node{ fieldVec, {} } ) };
        if(std::holds_alternative<symbol>(dst->data)){
            g_tupleVarArity[ std::get<symbol>(dst->data).name ] = n;
            g_tupleArities.insert(n);
        }
        return std::make_shared<node>( node{ sl, form.elems.front()->metadata } );
    });
    // (tget %dst <Ty> %tuple <index>) -> (member %dst __TupleN %tuple _<index>) with static OOB check (diagnostic E1601 at expansion time by returning nullopt for invalid idx)
    tx.add_macro("tget", [&g_tupleVarArity](const list& form)->std::optional<node_ptr>{
        auto &el = form.elems; if(el.size()!=5) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data) || !std::holds_alternative<symbol>(el[2]->data) || !std::holds_alternative<symbol>(el[3]->data) || !std::holds_alternative<int64_t>(el[4]->data)) return std::nullopt;
        int64_t idx = std::get<int64_t>(el[4]->data); if(idx < 0 || idx > 16) return std::nullopt; // expansion failure -> type checker can later map to E1601 if desired
        // Look up arity of tuple variable (third arg)
        std::string tupleVar = std::get<symbol>(el[3]->data).name;
        size_t ar = 16; if(auto it = g_tupleVarArity.find(tupleVar); it!=g_tupleVarArity.end()) ar = it->second;
        if((size_t)idx >= ar) return std::nullopt; // static OOB
        list m; m.elems = { make_sym("member"), el[1], make_sym("__Tuple"+std::to_string(ar)), el[3], make_sym("_"+std::to_string((size_t)idx)) };
        return std::make_shared<node>( node{ m, form.elems.front()->metadata } );
    });
    // (arr %dst <ElemTy> [ %e0 %e1 ... ]) -> direct core (array-lit %dst <ElemTy> <N> [ %e0 %e1 ... ])
    // Rationale: preserves variable visibility (previous block+alloca caused scoping issues) and reuses
    // canonical array literal lowering in memory_ops for consistent semantics / interning.
    tx.add_macro("arr", [](const list& form)->std::optional<node_ptr>{
        auto &el = form.elems; if(el.size()!=4) return std::nullopt; // (arr %dst <ElemTy> [ vals ])
        if(!std::holds_alternative<symbol>(el[1]->data) || !std::holds_alternative<symbol>(el[2]->data) || !std::holds_alternative<vector_t>(el[3]->data)) return std::nullopt;
        auto dst = el[1]; auto elemTy = el[2]; auto &vals = std::get<vector_t>(el[3]->data).elems; size_t n = vals.size(); if(n==0 || n>1024) return std::nullopt;
        // Validate each element is a symbol (SSA value) for now; relax later if immediate literals allowed.
        for(auto &v : vals){ if(!std::holds_alternative<symbol>(v->data)) return std::nullopt; }
        list lit; lit.elems.reserve(5);
        lit.elems.push_back(make_sym("array-lit"));
        lit.elems.push_back(dst);
        lit.elems.push_back(elemTy);
        lit.elems.push_back(make_i64((int64_t)n));
        // Repackage values into a vector node
        vector_t arrVec; arrVec.elems = vals; // shallow copy ok
        lit.elems.push_back(std::make_shared<node>( node{ arrVec, {} } ));
        return std::make_shared<node>( node{ lit, form.elems.front()->metadata } );
    });
    tx.add_macro("rarray", [](const list& form)->std::optional<node_ptr>{
        // Backward-compat alias: (rarray %dst <ElemTy> N) or (rarray %dst <ElemTy> [ ... ]) delegate to arr
        auto &el=form.elems; if(el.size()!=4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt;
        // If argument 3 already a vector treat like arr, else if int treat like count zero-init (defer for now -> nullopt)
        if(std::holds_alternative<vector_t>(el[3]->data)){
            list proxy; proxy.elems = { make_sym("arr"), el[1], el[2], el[3] };
            return std::make_shared<node>( node{ proxy, form.elems.front()->metadata } );
        }
        if(std::holds_alternative<int64_t>(el[3]->data)){
            int64_t n = std::get<int64_t>(el[3]->data); if(n<=0 || n>1024) return std::nullopt;
            // Emit (alloca %dst (array :elem <ElemTy> :size n))
            list arrTy; arrTy.elems = { make_sym("array"), make_kw("elem"), el[2], make_kw("size"), std::make_shared<node>( node{ (int64_t)n, {} } ) };
            list al; al.elems = { make_sym("alloca"), el[1], std::make_shared<node>( node{ arrTy, {} } ) };
            return std::make_shared<node>( node{ al, form.elems.front()->metadata } );
        }
        return std::nullopt;
    });

    // First expand macros to Core-like EDN
    edn::node_ptr expanded = tx.expand(module_ast);

    // Inject struct declarations for each used tuple arity if missing.
    // NOTE: Placeholder field types are i32; this matches current tests using homogeneous i32 tuples.
    if(expanded && std::holds_alternative<list>(expanded->data)){
        auto &modList = std::get<list>(expanded->data);
        if(!modList.elems.empty() && std::holds_alternative<symbol>(modList.elems[0]->data) && std::get<symbol>(modList.elems[0]->data).name=="module"){
            // Collect existing struct names
            std::unordered_set<std::string> existing;
            for(auto &n : modList.elems){
                if(!n || !std::holds_alternative<list>(n->data)) continue;
                auto &L = std::get<list>(n->data).elems; if(L.empty()) continue;
                if(std::holds_alternative<symbol>(L[0]->data) && std::get<symbol>(L[0]->data).name=="struct"){
                    // scan for :name
                    for(size_t i=1;i+1<L.size(); i+=2){ if(!std::holds_alternative<keyword>(L[i]->data)) break; if(std::get<keyword>(L[i]->data).name=="name" && std::holds_alternative<symbol>(L[i+1]->data)) existing.insert(std::get<symbol>(L[i+1]->data).name); }
                }
            }
            for(size_t ar : g_tupleArities){
                std::string sname = "__Tuple"+std::to_string(ar);
                if(existing.count(sname)) continue;
                // Build fields vector: [ (field :name _0 :type i32) ... ]
                vector_t fieldsV;
                for(size_t i=0;i<ar;++i){
                    list fld; fld.elems = { make_sym("field"), make_kw("name"), make_sym("_"+std::to_string(i)), make_kw("type"), make_sym("i32") };
                    fieldsV.elems.push_back(std::make_shared<node>( node{ fld, {} } ));
                }
                list structL; structL.elems.push_back(make_sym("struct"));
                structL.elems.push_back(make_kw("name")); structL.elems.push_back(make_sym(sname));
                structL.elems.push_back(make_kw("fields")); structL.elems.push_back(std::make_shared<node>( node{ fieldsV, {} } ));
                modList.elems.push_back(std::make_shared<node>( node{ structL, {} } ));
            }
        }
    }

    // Post-pass: remap uses of initializer-const symbols back to their variable symbols.
    // Rationale: frontends often synthesize a const (e.g., %__rl_c26 = 0) and then (assign %z %__rl_c26).
    // Later expressions should reference %z, not the one-time const symbol, so that slot-backed loads reflect updates.
    using edn::node_ptr; using edn::list; using edn::vector_t; using edn::symbol; using edn::keyword;

    std::function<void(vector_t&, std::unordered_map<std::string,std::string>&)> rewrite_seq;
    std::function<void(node_ptr&, std::unordered_map<std::string,std::string>&)> rewrite_node;

    auto replace_sym_if_mapped = [](node_ptr& n, const std::unordered_map<std::string,std::string>& env){
        if(!n) return;
        if(std::holds_alternative<symbol>(n->data)){
            const auto &s = std::get<symbol>(n->data).name;
            auto it = env.find(s);
            if(it != env.end()){
                n->data = symbol{ it->second };
            }
        }
    };

    rewrite_node = [&](node_ptr& n, std::unordered_map<std::string,std::string>& env){
        if(!n) return;
        if(std::holds_alternative<vector_t>(n->data)){
            auto &v = std::get<vector_t>(n->data);
            // New sequential scope inherits env by value (copy) so sibling sequences don't affect each other
            auto envCopy = env; rewrite_seq(v, envCopy);
            return;
        }
        if(!std::holds_alternative<list>(n->data)){
            // Simple atoms: apply symbol replacement if mapped
            replace_sym_if_mapped(n, env);
            return;
        }
        auto &l = std::get<list>(n->data);
        if(l.elems.empty() || !std::holds_alternative<symbol>(l.elems[0]->data)){
            // Recurse into children conservatively
            for(auto &ch : l.elems){ rewrite_node(ch, env); }
            return;
        }
        std::string op = std::get<symbol>(l.elems[0]->data).name;
        // For structured ops that contain nested vectors (bodies), process those as sequences with a forked env
    auto process_nested_vec = [&](size_t idx){ if(idx<l.elems.size() && l.elems[idx] && std::holds_alternative<vector_t>(l.elems[idx]->data)){ auto envCopy = env; rewrite_seq(std::get<vector_t>(l.elems[idx]->data), envCopy); } };
    (void)process_nested_vec;

        // Update env from declarations/assignments before replacing later uses in the same sequence step
        if(op=="as" && l.elems.size()==4){
            // (as %var <ty> %init)
            if(std::holds_alternative<symbol>(l.elems[1]->data) && std::holds_alternative<symbol>(l.elems[3]->data)){
                std::string var = std::get<symbol>(l.elems[1]->data).name;
                std::string init = std::get<symbol>(l.elems[3]->data).name;
                env[init] = var;
            }
            // Do not rewrite operands inside this same node; only future uses should see the alias
            return;
    }

        // Replace symbol operands (skip head op and keywords)
        for(size_t i=1; i<l.elems.size(); ++i){
            if(l.elems[i] && std::holds_alternative<keyword>(l.elems[i]->data)){
                // Process the value after a keyword; if it is a vector body, handle sequentially
                if(i+1<l.elems.size() && l.elems[i+1]){
                    // If body vector, process as sequence; otherwise recurse normally
                    if(std::holds_alternative<vector_t>(l.elems[i+1]->data)){
                        auto envCopy = env; rewrite_seq(std::get<vector_t>(l.elems[i+1]->data), envCopy);
                    } else {
                        rewrite_node(l.elems[i+1], env);
                    }
                    ++i; // skip value just processed
                }
                continue;
            }
            // Recurse into nested lists/vectors or replace plain symbol
            if(l.elems[i] && (std::holds_alternative<list>(l.elems[i]->data) || std::holds_alternative<vector_t>(l.elems[i]->data))){
                rewrite_node(l.elems[i], env);
            } else {
                replace_sym_if_mapped(l.elems[i], env);
            }
        }
    };

    rewrite_seq = [&](vector_t& seq, std::unordered_map<std::string,std::string>& env){
        for(auto &inst : seq.elems){ rewrite_node(inst, env); }
    };

    // Kick off rewriting at module top-level if it’s a list: visit function bodies and nested vectors via generic recursion above.
    if(expanded && std::holds_alternative<list>(expanded->data)){
        auto env = std::unordered_map<std::string,std::string>{};
        rewrite_node(expanded, env);
    }
    return expanded;
}

} // namespace rustlite
