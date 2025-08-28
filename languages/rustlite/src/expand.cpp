// NOTE: This file is being refactored to dispatch to per-category macro registration
// units under src/macros_*.cpp. Only a subset (literals) has been migrated so far.
// Subsequent commits will move the remaining monolithic macro bodies into separate
// translation units to reduce merge/edit risk.
#include "rustlite/expand.hpp"
#include "rustlite/macros/context.hpp"
#include "rustlite/macros/helpers.hpp"
#include "rustlite/features.hpp" // feature flags (bounds checks, capture inference)
#include "edn/transform.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>

// (Capture inference gating via rustlite::infer_captures_enabled from features.hpp)

// Restored macros: rcstr, rbytes, rextern-global, rextern-const, refactored rindex* helper, ematch metadata tagging.
// (Build stamp v2) Adjusted to support E1600 non-exhaustive enum match diagnostic.

using namespace edn;

namespace rustlite {

using rustlite::rl_make_sym; using rustlite::rl_make_kw; using rustlite::rl_make_i64; using rustlite::rl_gensym;

// (Previous inline fallback removed; single source of truth is features.hpp)

edn::node_ptr expand_rustlite(const edn::node_ptr& module_ast){
    // Pre-walk: rewrite variant constructor surface forms of shape (Type::Variant %dst [payload*])
    // to an internal generic macro form: (enum-ctor %dst Type Variant payload...)
    // Assumption: destination SSA symbol always provided as first argument (unlike earlier prose examples
    // which omitted it). This keeps lowering consistent with existing sum-new op which requires %dst.
    std::function<void(node_ptr&)> prewalk;
    prewalk = [&](node_ptr& n){
        if(!n) return;
        if(std::holds_alternative<vector_t>(n->data)){
            for(auto &e : std::get<vector_t>(n->data).elems) prewalk(e);
            return;
        }
        if(!std::holds_alternative<list>(n->data)) return;
        auto &L = std::get<list>(n->data).elems;
        if(L.empty() || !std::holds_alternative<symbol>(L[0]->data)){
            for(auto &e : L) prewalk(e);
            return;
        }
        std::string head = std::get<symbol>(L[0]->data).name;
        auto pos = head.find("::");
        if(pos!=std::string::npos && L.size()>=2 && std::holds_alternative<symbol>(L[1]->data) && std::get<symbol>(L[1]->data).name.rfind('%',0)==0){
            std::string typeName = head.substr(0,pos);
            std::string variantName = head.substr(pos+2);
            // Build new list: enum-ctor %dst Type Variant <payload...>
            list repl; repl.elems.push_back(rl_make_sym("enum-ctor"));
            repl.elems.push_back(L[1]);
            repl.elems.push_back(rl_make_sym(typeName));
            repl.elems.push_back(rl_make_sym(variantName));
            for(size_t i=2;i<L.size();++i){ repl.elems.push_back(L[i]); }
            n->data = repl; // replace in-place
            // Recurse into payload forms if any
            for(size_t i=4;i<repl.elems.size();++i) prewalk(repl.elems[i]);
            return;
        }
        for(auto &e : L) prewalk(e);
    };
    auto ast_copy = module_ast; // operate on provided AST (shared_ptr semantics)
    prewalk(ast_copy);

    // Optional pre-expansion rewrite: closure capture inference.
    // If RUSTLITE_INFER_CAPS=1 and an (rclosure %c callee ...) form lacks a :captures vector,
    // heuristically capture the symbol defined immediately prior in the same block (vector sequence).
    if(rustlite::infer_captures_enabled()){
        std::function<void(node_ptr&)> closure_walk;
        closure_walk = [&](node_ptr &n){
            if(!n) return;
            if(std::holds_alternative<vector_t>(n->data)){
                auto &vec = std::get<vector_t>(n->data);
                // Walk with index to inspect previous sibling
                for(size_t i=0;i<vec.elems.size(); ++i){
                    auto &elem = vec.elems[i];
                    if(elem && std::holds_alternative<list>(elem->data)){
                        auto &L = std::get<list>(elem->data).elems;
                        if(!L.empty() && std::holds_alternative<symbol>(L[0]->data) && std::get<symbol>(L[0]->data).name=="rclosure"){
                            bool hasCaptures=false; for(size_t j=1;j+1<L.size(); j+=2){ if(std::holds_alternative<keyword>(L[j]->data) && std::get<keyword>(L[j]->data).name=="captures"){ hasCaptures=true; break; } else if(!std::holds_alternative<keyword>(L[j]->data)) break; }
                            if(!hasCaptures){
                                // candidate: previous sibling list defines symbol via (const %sym Ty ...) or (as %sym Ty ...)
                                std::string capSymName;
                                if(i>0){
                                    auto &prev = vec.elems[i-1];
                                    if(prev && std::holds_alternative<list>(prev->data)){
                                        auto &PL = std::get<list>(prev->data).elems;
                                        if(PL.size()>=4 && std::holds_alternative<symbol>(PL[0]->data)){
                                            std::string op = std::get<symbol>(PL[0]->data).name;
                                            if((op=="const"||op=="as") && std::holds_alternative<symbol>(PL[1]->data)) capSymName = std::get<symbol>(PL[1]->data).name;
                                        }
                                    }
                                }
                                if(!capSymName.empty()){
                                    // Insert :captures [ %sym ] just after callee symbol (expected order: head %dst callee ...)
                                    // Form: (rclosure %c callee :captures [ %capt ])
                                    // Find insertion point before first keyword argument.
                                    size_t insertPos = L.size();
                                    for(size_t k=1;k<L.size(); ++k){ if(std::holds_alternative<keyword>(L[k]->data)){ insertPos = k; break; } }
                                    vector_t capVec; capVec.elems.push_back(rustlite::rl_make_sym(capSymName));
                                    auto capVecNode = std::make_shared<node>( node{ capVec, {} } );
                                    auto it = std::next(L.begin(), static_cast<long>(insertPos));
                                    it = L.insert(it, rl_make_kw("captures"));
                                    L.insert(std::next(it), capVecNode);
                                }
                            }
                        }
                    }
                    if(vec.elems[i]) closure_walk(vec.elems[i]);
                }
                return;
            }
            if(std::holds_alternative<list>(n->data)){
                auto &L = std::get<list>(n->data).elems; for(auto &c : L) closure_walk(c); return; }
        };
        closure_walk(ast_copy);
    }
    Transformer tx;
    // Shared macro context (enum counts, tuple arities, etc.)
    auto macroCtx = std::make_shared<MacroContext>();
    // Register migrated macro groups first (others remain inline below until moved)
    register_literal_macros(tx, macroCtx);
    register_extern_macros(tx, macroCtx);
    register_var_control_macros(tx, macroCtx);
    register_sum_enum_macros(tx, macroCtx);
    register_field_index_macros(tx, macroCtx); // field/index early so later macros can assume core ops
    register_tuple_array_macros(tx, macroCtx); // tuple arities collected before closures/calls
    register_struct_trait_macros(tx, macroCtx);
    register_closure_macros(tx, macroCtx);
    register_call_macros(tx, macroCtx);
    register_assert_macros(tx, macroCtx);
    register_alias_macros(tx, macroCtx);
    // All macros now registered via modular sources. Removed legacy inline macro definitions.

    // First expand macros to Core-like EDN
    edn::node_ptr expanded = tx.expand(ast_copy);

    // Inject struct declarations for each used tuple arity if missing.
    // Attempt lightweight field type inference by scanning preceding const/as instructions
    // for each struct-lit usage of a tuple. Fallback to i32 if any field ambiguous.
    if(expanded && std::holds_alternative<list>(expanded->data)){
        auto &modList = std::get<list>(expanded->data);
        if(!modList.elems.empty() && std::holds_alternative<symbol>(modList.elems[0]->data) && std::get<symbol>(modList.elems[0]->data).name=="module"){
            // First pass: infer field types per arity
            std::unordered_map<size_t, std::vector<node_ptr>> inferred;
            std::unordered_map<size_t, std::vector<bool>> complete;
            for(size_t ar : macroCtx->tupleArities){ inferred[ar] = std::vector<node_ptr>(ar, nullptr); complete[ar] = std::vector<bool>(ar, false); }
            auto try_infer_from_symbol = [&](const std::vector<node_ptr>& body, size_t uptoIdx, const std::string& sym)->node_ptr{
                // Scan backwards (excluding instruction at uptoIdx) for defining const / as of %sym.
                if(uptoIdx == 0) return nullptr;
                for(size_t i = uptoIdx; i-- > 0; ){ // i runs: uptoIdx-1 ... 0
                    auto &n = body[i];
                    if(!n || !std::holds_alternative<list>(n->data)) continue;
                    auto &L = std::get<list>(n->data).elems;
                    if(L.empty() || !std::holds_alternative<symbol>(L[0]->data)) continue;
                    const std::string op = std::get<symbol>(L[0]->data).name;
                    if(op=="const" && L.size()>=4 && std::holds_alternative<symbol>(L[1]->data) && std::get<symbol>(L[1]->data).name==sym){
                        // (const %a <Ty> ...)
                        return L[2];
                    }
                    if(op=="as" && L.size()>=4 && std::holds_alternative<symbol>(L[1]->data) && std::get<symbol>(L[1]->data).name==sym){
                        // (as %a <Ty> ...)
                        return L[2];
                    }
                }
                return nullptr;
            };
            // Iterate module nodes to find fn/rfn forms
            for(auto &top : modList.elems){
                if(!top || !std::holds_alternative<list>(top->data)) continue;
                auto &fnL = std::get<list>(top->data).elems; if(fnL.empty() || !std::holds_alternative<symbol>(fnL[0]->data)) continue;
                std::string head = std::get<symbol>(fnL[0]->data).name; if(head!="fn" && head!="rfn") continue;
                // locate :body vector
                node_ptr bodyVec=nullptr;
                for(size_t i=1;i+1<fnL.size(); i+=2){ if(!std::holds_alternative<keyword>(fnL[i]->data)) break; if(std::get<keyword>(fnL[i]->data).name=="body") { bodyVec=fnL[i+1]; break; } }
                if(!bodyVec || !std::holds_alternative<vector_t>(bodyVec->data)) continue;
                auto &instrs = std::get<vector_t>(bodyVec->data).elems;
                for(size_t idx=0; idx<instrs.size(); ++idx){
                    auto &inst = instrs[idx]; if(!inst || !std::holds_alternative<list>(inst->data)) continue;
                    auto &L = std::get<list>(inst->data).elems; if(L.size()!=4) continue;
                    if(!std::holds_alternative<symbol>(L[0]->data)) continue;
                    if(std::get<symbol>(L[0]->data).name!="struct-lit") continue;
                    if(!std::holds_alternative<symbol>(L[2]->data)) continue;
                    std::string sname = std::get<symbol>(L[2]->data).name; if(sname.rfind("__Tuple",0)!=0) continue;
                    size_t arity = 0; try { arity = (size_t)std::stoul(sname.substr(8)); } catch(...) { continue; }
                    if(!macroCtx->tupleArities.count(arity)) continue;
                    if(!std::holds_alternative<vector_t>(L[3]->data)) continue; auto &fields = std::get<vector_t>(L[3]->data).elems;
                    // fields vector pattern: _0 %a _1 %b ... pairs
                    for(size_t fi=0, fieldIndex=0; fi+1<fields.size(); fi+=2, ++fieldIndex){
                        if(fieldIndex>=arity) break;
                        auto &valNode = fields[fi+1]; if(!valNode || !std::holds_alternative<symbol>(valNode->data)) continue;
                        std::string vsym = std::get<symbol>(valNode->data).name;
                        if(!inferred[arity][fieldIndex]){
                            auto tyNode = try_infer_from_symbol(instrs, idx, vsym);
                            if(tyNode){ inferred[arity][fieldIndex] = tyNode; complete[arity][fieldIndex]=true; }
                        }
                    }
                }
            }
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
            for(size_t ar : macroCtx->tupleArities){
                std::string sname = "__Tuple"+std::to_string(ar);
                if(existing.count(sname)) continue;
                // Determine if we have complete inferred types
                bool allResolved = true;
                if(inferred.count(ar)){
                    for(size_t i=0;i<ar;++i){ if(!complete[ar][i] || !inferred[ar][i]) { allResolved=false; break; } }
                } else allResolved=false;
                // Build fields vector: [ (field :name _i :type <Ty>) ... ]
                vector_t fieldsV;
                for(size_t i=0;i<ar;++i){
                    node_ptr tyNode = (allResolved? inferred[ar][i] : nullptr);
                    if(!tyNode) tyNode = rl_make_sym("i32"); // fallback
                    list fld; fld.elems = { rl_make_sym("field"), rl_make_kw("name"), rl_make_sym("_"+std::to_string(i)), rl_make_kw("type"), tyNode };
                    fieldsV.elems.push_back(std::make_shared<node>( node{ fld, {} } ));
                }
                list structL; structL.elems.push_back(rl_make_sym("struct"));
                structL.elems.push_back(rl_make_kw("name")); structL.elems.push_back(rl_make_sym(sname));
                structL.elems.push_back(rl_make_kw("fields")); structL.elems.push_back(std::make_shared<node>( node{ fieldsV, {} } ));
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
    auto process_nested_vec = [&](size_t idx)->void{
        if(idx<l.elems.size() && l.elems[idx] && std::holds_alternative<vector_t>(l.elems[idx]->data)){
            auto envCopy = env;
            rewrite_seq(std::get<vector_t>(l.elems[idx]->data), envCopy);
        }
        return; // explicit to satisfy -Wreturn-type (defensive)
    };
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
