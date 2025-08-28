#include "rustlite/macros/context.hpp"
#include "rustlite/expand.hpp"
#include "edn/transform.hpp"
#include "rustlite/macros/helpers.hpp"
#include <optional>
#include <string>
#include <vector>

using namespace edn;
namespace rustlite {
using rustlite::rl_make_sym;

void register_tuple_array_macros(edn::Transformer& tx, const std::shared_ptr<MacroContext>& ctx){
    // (tuple %dst [ %a %b ... ]) -> (struct-lit %dst __TupleN [ _0 %a _1 %b ... ])
    tx.add_macro("tuple", [ctx](const list& form)->std::optional<node_ptr>{
        auto &el=form.elems; if(el.size()!=3) return std::nullopt; // head, %dst, vector
        if(!std::holds_alternative<symbol>(el[1]->data)) return std::nullopt;
        if(!std::holds_alternative<vector_t>(el[2]->data)) return std::nullopt;
        auto &vals = std::get<vector_t>(el[2]->data).elems; size_t arity = vals.size();
        ctx->tupleArities.insert(arity);
        // Record dst var arity for later tget lowering.
        std::string dstName = std::get<symbol>(el[1]->data).name;
        if(!dstName.empty() && dstName[0]=='%') ctx->tupleVarArity[dstName.substr(1)] = arity;
        std::string sname = "__Tuple"+std::to_string(arity);
        vector_t fieldsV; // [ _0 %a _1 %b ... ]
        for(size_t i=0;i<vals.size(); ++i){
            fieldsV.elems.push_back(rl_make_sym("_"+std::to_string(i)));
            fieldsV.elems.push_back(vals[i]);
        }
        list out; out.elems = { rl_make_sym("struct-lit"), el[1], rl_make_sym(sname), std::make_shared<node>( node{ fieldsV, {} } ) };
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });
    // (tget %dst <ElemTy> %tuple <index>) -> (member %dst __TupleN %tuple _<index>)
    tx.add_macro("tget", [ctx](const list& form)->std::optional<node_ptr>{
        auto &e=form.elems; if(e.size()!=5) return std::nullopt; // head %dst <Ty> %tuple <idx>
        if(!std::holds_alternative<symbol>(e[1]->data)) return std::nullopt; // %dst
        if(!std::holds_alternative<symbol>(e[3]->data)) return std::nullopt; // %tuple
        if(!std::holds_alternative<int64_t>(e[4]->data)) return std::nullopt; // index literal
        std::string tup = std::get<symbol>(e[3]->data).name;
        if(tup.empty()||tup[0] != '%') return std::nullopt;
        auto it = ctx->tupleVarArity.find(tup.substr(1)); if(it==ctx->tupleVarArity.end()) return std::nullopt;
        size_t arity = it->second; int64_t idx = std::get<int64_t>(e[4]->data); if(idx<0 || (size_t)idx>=arity) return std::nullopt;
        std::string sname = "__Tuple"+std::to_string(arity);
        list out; out.elems = { rl_make_sym("member"), e[1], rl_make_sym(sname), e[3], rl_make_sym("_"+std::to_string(idx)) };
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });
    // (arr %dst <ElemTy> [ %v0 %v1 ... ]) -> (array-lit %dst <ElemTy> N [ %v0 %v1 ... ])
    tx.add_macro("arr", [ctx](const list& form)->std::optional<node_ptr>{
        auto &e=form.elems; if(e.size()!=4) return std::nullopt;
        if(!std::holds_alternative<symbol>(e[1]->data)) return std::nullopt; // %dst
        auto elemTy = e[2];
        if(!std::holds_alternative<vector_t>(e[3]->data)) return std::nullopt;
        auto &vec = std::get<vector_t>(e[3]->data).elems;
        // Record length for bounds inference
        std::string dstName = std::get<symbol>(e[1]->data).name;
        if(!dstName.empty() && dstName[0]=='%') ctx->arrayLengths[dstName.substr(1)] = vec.size();
        vector_t copy; for(auto &v: vec) copy.elems.push_back(v);
        list out; out.elems = { rl_make_sym("array-lit"), e[1], elemTy, rl_make_i64((int64_t)vec.size()), std::make_shared<node>( node{ copy, {} } ) };
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });
    // (rarray %dst <ElemTy> <size-int>) -> (alloca %dst (array :elem <ElemTy> :size <size-int>))
    tx.add_macro("rarray", [ctx](const list& form)->std::optional<node_ptr>{
        auto &e=form.elems; if(e.size()!=4) return std::nullopt;
        if(!std::holds_alternative<symbol>(e[1]->data)) return std::nullopt;
        if(!std::holds_alternative<int64_t>(e[3]->data)) return std::nullopt;
        // Record declared size
        std::string dstName = std::get<symbol>(e[1]->data).name;
        if(!dstName.empty() && dstName[0]=='%') ctx->arrayLengths[dstName.substr(1)] = (size_t)std::get<int64_t>(e[3]->data);
        list arrTy; arrTy.elems = { rl_make_sym("array"), rl_make_kw("elem"), e[2], rl_make_kw("size"), e[3] };
        list out; out.elems = { rl_make_sym("alloca"), e[1], std::make_shared<node>( node{ arrTy, {} } ) };
        return std::make_shared<node>( node{ out, form.elems.front()->metadata } );
    });
}
} // namespace rustlite
