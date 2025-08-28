#include "rustlite/macros/context.hpp"
#include "rustlite/expand.hpp"
#include "edn/transform.hpp"
#include "rustlite/macros/helpers.hpp"

using namespace edn;
namespace rustlite {
using rustlite::rl_make_sym;

void register_struct_trait_macros(edn::Transformer& tx, const std::shared_ptr<MacroContext>&){
    // Simple renames: preserve all original arguments (keywords, vectors, bodies, etc.).
    tx.add_macro("rstruct", [](const list& form)->std::optional<node_ptr>{
        auto &el=form.elems; if(el.size()<2) return std::nullopt;
        // Copy list, then post-process :fields vector entries converting (name Type) -> (field :name name :type Type)
        list out; out.elems.reserve(el.size()); out.elems.push_back(rl_make_sym("struct"));
        for(size_t i=1;i<el.size(); ++i){ out.elems.push_back(el[i]); }
        // Scan for :fields keyword
        for(size_t i=1;i+1<out.elems.size(); ++i){
            if(!out.elems[i] || !std::holds_alternative<keyword>(out.elems[i]->data)) continue;
            if(std::get<keyword>(out.elems[i]->data).name!="fields") continue;
            auto vecNode = out.elems[i+1]; if(!vecNode || !std::holds_alternative<vector_t>(vecNode->data)) continue;
            auto &vf = std::get<vector_t>(vecNode->data).elems;
            vector_t norm; norm.elems.reserve(vf.size());
            for(auto &f : vf){
                if(!f || !std::holds_alternative<list>(f->data)){ norm.elems.push_back(f); continue; }
                auto fl = std::get<list>(f->data);
                // Expect (name <type>) where name is symbol and type is node
                if(fl.elems.size()==2 && fl.elems[0] && std::holds_alternative<symbol>(fl.elems[0]->data)){
                    list fld; fld.elems = { rl_make_sym("field"), edn::n_kw("name"), fl.elems[0], edn::n_kw("type"), fl.elems[1] };
                    norm.elems.push_back(std::make_shared<node>( node{ fld, f->metadata } ));
                } else if(fl.elems.size()>=1 && fl.elems[0] && std::holds_alternative<symbol>(fl.elems[0]->data)){
                    // If already expanded (field ...), keep
                    if(std::get<symbol>(fl.elems[0]->data).name=="field") norm.elems.push_back(f);
                    else norm.elems.push_back(f); // fallback
                } else {
                    norm.elems.push_back(f);
                }
            }
            out.elems[i+1] = std::make_shared<node>( node{ norm, vecNode->metadata } );
        }
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });
    // rextern-fn: external function declaration sugar -> (fn ... :external true)
    tx.add_macro("rextern-fn", [](const list& form)->std::optional<node_ptr>{
        auto &el=form.elems; if(el.size()<2) return std::nullopt;
        list out; out.elems.reserve(el.size()+2); // possible extra :external true
        out.elems.push_back(rl_make_sym("fn"));
        bool sawExternal=false;
        for(size_t i=1;i<el.size(); ++i){
            out.elems.push_back(el[i]);
            if(el[i] && std::holds_alternative<keyword>(el[i]->data) && std::get<keyword>(el[i]->data).name=="external") sawExternal=true;
        }
        if(!sawExternal){ out.elems.push_back(edn::n_kw("external")); out.elems.push_back(detail::make_node(true)); }
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });
    tx.add_macro("rtrait", [](const list& form)->std::optional<node_ptr>{ auto &el=form.elems; if(el.size()<2) return std::nullopt; list out; out.elems.reserve(el.size()); out.elems.push_back(rl_make_sym("trait")); for(size_t i=1;i<el.size(); ++i) out.elems.push_back(el[i]); return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); });
    tx.add_macro("rimpl", [](const list& form)->std::optional<node_ptr>{ auto &el=form.elems; if(el.size()<3) return std::nullopt; list out; out.elems.reserve(el.size()); out.elems.push_back(rl_make_sym("impl")); for(size_t i=1;i<el.size(); ++i) out.elems.push_back(el[i]); return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); });
    tx.add_macro("rmethod", [](const list& form)->std::optional<node_ptr>{ auto &el=form.elems; if(el.size()<2) return std::nullopt; list out; out.elems.reserve(el.size()); out.elems.push_back(rl_make_sym("method")); for(size_t i=1;i<el.size(); ++i) out.elems.push_back(el[i]); return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); });
    tx.add_macro("rfn", [](const list& form)->std::optional<node_ptr>{ auto &el=form.elems; if(el.size()<2) return std::nullopt; list out; out.elems.reserve(el.size()); out.elems.push_back(rl_make_sym("fn")); for(size_t i=1;i<el.size(); ++i) out.elems.push_back(el[i]); return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); });
    tx.add_macro("rfnptr", [](const list& form)->std::optional<node_ptr>{ auto &el=form.elems; if(el.size()<2) return std::nullopt; list out; out.elems.reserve(el.size()); out.elems.push_back(rl_make_sym("fnptr")); for(size_t i=1;i<el.size(); ++i) out.elems.push_back(el[i]); return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); });
    // rdot sugar:
    //   Trait dispatch form: (rdot %dst <RetTy> Trait %obj method %args*) -> (trait-call %dst <RetTy> Trait %obj method %args*)
    //   Direct call   form: (rdot %dst <RetTy> callee %args*)            -> (call %dst <RetTy> callee %args*)
    // Distinguish by arity and positional pattern. Trait form needs at least 7 elems: head,%dst,Ret,Trait,%obj,method,(arg...)
    // where %obj symbol name starts with '%'. If pattern not matched, fall back to direct call lowering.
    tx.add_macro("rdot", [](const list& form)->std::optional<node_ptr>{
        auto &el=form.elems; if(el.size()<5) return std::nullopt; // need at least head,%dst,Ret,callee,arg-or-more
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt; // %dst
        // Trait pattern candidate: size>=7 and indices 3,4,5 are symbols with el[4] starting '%'
        bool traitShape = false;
        if(el.size() >= 7 &&
           std::holds_alternative<symbol>(el[3]->data) &&
           std::holds_alternative<symbol>(el[4]->data) &&
           std::holds_alternative<symbol>(el[5]->data)){
            auto objName = std::get<symbol>(el[4]->data).name;
            if(!objName.empty() && objName[0]=='%') traitShape = true;
        }
        list out;
        if(traitShape){
            out.elems.push_back(rl_make_sym("trait-call"));
            for(size_t i=1;i<el.size(); ++i) out.elems.push_back(el[i]);
        } else {
            // Direct call form: (call %dst <RetTy> callee %args*)
            out.elems.push_back(rl_make_sym("call"));
            // %dst, <RetTy>, callee, args...
            for(size_t i=1;i<el.size(); ++i) out.elems.push_back(el[i]);
        }
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });
    tx.add_macro("rtrait-call", [](const list& form)->std::optional<node_ptr>{ auto &el=form.elems; if(el.size()<6) return std::nullopt; list out; out.elems.reserve(el.size()); out.elems.push_back(rl_make_sym("trait-call")); for(size_t i=1;i<el.size(); ++i) out.elems.push_back(el[i]); return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); });
    tx.add_macro("rmake-trait-obj", [](const list& form)->std::optional<node_ptr>{ auto &el=form.elems; if(el.size()<5) return std::nullopt; list out; out.elems.reserve(el.size()); out.elems.push_back(rl_make_sym("make-trait-obj")); for(size_t i=1;i<el.size(); ++i) out.elems.push_back(el[i]); return std::make_shared<node>( node{ out, form.elems.front()->metadata } ); });
}

} // namespace rustlite
