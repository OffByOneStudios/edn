// RL_FIX_MARK_1: rewritten file to remove stale duplicate bodies & BOM issues.
#include "rustlite/macros/context.hpp"
#include "rustlite/expand.hpp"
#include "edn/transform.hpp"
#include "rustlite/macros/helpers.hpp"

using namespace edn;
namespace rustlite {

void register_closure_macros(edn::Transformer& tx, const std::shared_ptr<MacroContext>&){
    tx.add_macro("rclosure", [](const list& form)->std::optional<node_ptr>{
        auto &el = form.elems; if(el.size()<3) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt;
        node_ptr dst = el[1]; node_ptr callee = el[2]; node_ptr capV = nullptr;
        for(size_t i=3;i+1<el.size(); i+=2){ if(!std::holds_alternative<keyword>(el[i]->data)) break; if(std::get<keyword>(el[i]->data).name=="captures") capV=el[i+1]; }
        if(!capV || !std::holds_alternative<vector_t>(capV->data)) return std::nullopt;
        list l; l.elems = { rl_make_sym("make-closure"), dst, callee, capV };
        return std::make_shared<node>( node{ l, form.elems.front()->metadata } );
    });
    tx.add_macro("rcall-closure", [](const list& form)->std::optional<node_ptr>{
        auto &el=form.elems; if(el.size()<4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt;
        node_ptr dst=el[1]; node_ptr retTy=el[2]; node_ptr clos=el[3];
        list out; out.elems={ rl_make_sym("call-closure"), dst, retTy, clos };
        for(size_t i=4;i<el.size(); ++i) out.elems.push_back(el[i]);
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });
}

void register_call_macros(edn::Transformer& tx, const std::shared_ptr<MacroContext>&){
    tx.add_macro("rcall", [](const list& form)->std::optional<node_ptr>{
        auto &el=form.elems; if(el.size()<4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt;
        node_ptr dst=el[1]; node_ptr retTy=el[2]; node_ptr callee=el[3];
        std::vector<node_ptr> args;
        if(el.size()>=5){
            if(std::holds_alternative<vector_t>(el[4]->data)){
                for(auto &a : std::get<vector_t>(el[4]->data).elems) args.push_back(a);
            } else { for(size_t i=4;i<el.size(); ++i) args.push_back(el[i]); }
        }
        if(std::holds_alternative<symbol>(callee->data)){
            std::string op = std::get<symbol>(callee->data).name;
            auto bin = [&](const std::string& bop) -> std::optional<node_ptr>{
                if(args.size()!=2) return std::nullopt;
                list out; out.elems = { rl_make_sym(bop), dst, retTy, args[0], args[1] };
                return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
            };
            if(op=="add"||op=="sub"||op=="mul"||op=="div"||op=="eq"||op=="ne"||op=="lt"||op=="le"||op=="gt"||op=="ge") return bin(op);
            if(op=="not"){
                if(args.size()!=1) return std::nullopt;
                vector_t body;
                auto z = rl_make_sym(rl_gensym("z"));
                { list c; c.elems={ rl_make_sym("const"), z, rl_make_sym("i1"), rl_make_i64(0) }; body.elems.push_back(std::make_shared<node>( node{ c, {} } )); }
                { list cmp; cmp.elems={ rl_make_sym("eq"), dst, rl_make_sym("i1"), args[0], z }; body.elems.push_back(std::make_shared<node>( node{ cmp, {} } )); }
                list blockL; blockL.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ body, {} } ) };
                return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
            }
        }
        list out; out.elems = { rl_make_sym("call"), dst, retTy, callee }; for(auto &a: args) out.elems.push_back(a);
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });

    tx.add_macro("rand", [](const list& form)->std::optional<node_ptr>{
        auto &el=form.elems; if(el.size()!=4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt;
        node_ptr dst=el[1]; node_ptr a=el[2]; node_ptr b=el[3];
        vector_t body;
        { list c; c.elems={ rl_make_sym("const"), rl_make_sym(rl_gensym("f")), rl_make_sym("i1"), rl_make_i64(0) }; auto cnode=std::make_shared<node>( node{ c, {} } ); body.elems.push_back(cnode); auto fSym=std::get<list>(cnode->data).elems[1]; list asL; asL.elems={ rl_make_sym("as"), dst, rl_make_sym("i1"), fSym }; body.elems.push_back(std::make_shared<node>( node{ asL, {} } )); }
        vector_t thenV; { list asg; asg.elems={ rl_make_sym("assign"), dst, b }; thenV.elems.push_back(std::make_shared<node>( node{ asg, {} } )); }
        vector_t elseV; // empty
        list ifL; ifL.elems = { rl_make_sym("if"), a, std::make_shared<node>( node{ thenV, {} } ) };
        body.elems.push_back(std::make_shared<node>( node{ ifL, {} } ));
        list blockL; blockL.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });

    tx.add_macro("ror", [](const list& form)->std::optional<node_ptr>{
        auto &el=form.elems; if(el.size()!=4) return std::nullopt;
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt;
        node_ptr dst=el[1]; node_ptr a=el[2]; node_ptr b=el[3];
        vector_t body;
        { list asL; asL.elems={ rl_make_sym("as"), dst, rl_make_sym("i1"), b }; body.elems.push_back(std::make_shared<node>( node{ asL, {} } )); }
        vector_t thenV; { list oneC; oneC.elems={ rl_make_sym("const"), rl_make_sym(rl_gensym("t")), rl_make_sym("i1"), rl_make_i64(1) }; auto oneN=std::make_shared<node>( node{ oneC, {} } ); thenV.elems.push_back(oneN); auto oneSym=std::get<list>(oneN->data).elems[1]; list asg; asg.elems={ rl_make_sym("assign"), dst, oneSym }; thenV.elems.push_back(std::make_shared<node>( node{ asg, {} } )); }
        list ifL; ifL.elems = { rl_make_sym("if"), a, std::make_shared<node>( node{ thenV, {} } ) };
        body.elems.push_back(std::make_shared<node>( node{ ifL, {} } ));
        list blockL; blockL.elems = { rl_make_sym("block"), edn::n_kw("body"), std::make_shared<node>( node{ body, {} } ) };
        return std::make_shared<node>( node{ blockL, form.elems.front()->metadata } );
    });
}

} // namespace rustlite
