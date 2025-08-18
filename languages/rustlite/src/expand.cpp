#include "rustlite/expand.hpp"
#include "edn/transform.hpp"
#include <string>

using namespace edn;

namespace rustlite {

static node_ptr make_sym(const std::string& s){ return std::make_shared<node>( node{ symbol{s}, {} } ); }
static node_ptr make_kw(const std::string& s){ return std::make_shared<node>( node{ keyword{s}, {} } ); }

edn::node_ptr expand_rustlite(const edn::node_ptr& module_ast){
    Transformer tx;
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

    return tx.expand(module_ast);
}

} // namespace rustlite
