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

// Single source of truth for feature flags lives in features.hpp (no local fallbacks here).

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

    // Generic monomorphization prototype (Phase: initial). Surface pattern:
    // (fn :name "id" :generics [ T ] :ret T :params [ (param T %x) ] :body [ (ret T %x) ])
    // Call sites use rcall-g form (not a macro) which is rewritten here:
    // (rcall-g %dst RetTy id [ ConcreteTy... ] %args...) -> (call %dst RetTy id__ConcreteTy... %args...)
    // We clone the generic fn per unique instantiation, substituting type parameter symbols in ret/param types and body.
    // Limitations: no trait bounds, no nested generics, simple symbol equality substitution only.
    if(expanded && std::holds_alternative<list>(expanded->data)){
        auto &modList = std::get<list>(expanded->data);
        if(!modList.elems.empty() && std::holds_alternative<symbol>(modList.elems[0]->data) && std::get<symbol>(modList.elems[0]->data).name=="module"){
            // Collect generic function templates and remove them from module (we'll append specializations after cloning)
            struct GenericTemplate { std::string name; std::vector<std::string> typeParams; std::vector<std::pair<std::string,std::string>> bounds; node_ptr fnList; };
            std::vector<GenericTemplate> generics;
            std::vector<node_ptr> retained; retained.reserve(modList.elems.size());
            retained.push_back(modList.elems[0]); // keep module head
            for(size_t i=1;i<modList.elems.size(); ++i){
                auto &n = modList.elems[i];
                bool isGeneric=false; std::vector<std::string> tparams; std::vector<std::pair<std::string,std::string>> bounds; std::string fname;
                if(n && std::holds_alternative<list>(n->data)){
                    auto &L = std::get<list>(n->data).elems;
                    if(!L.empty() && std::holds_alternative<symbol>(L[0]->data) && std::get<symbol>(L[0]->data).name=="fn"){
                        // scan keywords
                        for(size_t k=1;k+1<L.size(); k+=2){
                            if(!std::holds_alternative<keyword>(L[k]->data)) break;
                            std::string kw = std::get<keyword>(L[k]->data).name;
                            if(kw=="name" && std::holds_alternative<std::string>(L[k+1]->data)) fname = std::get<std::string>(L[k+1]->data); // function name stored as string
                            if(kw=="generics" && std::holds_alternative<vector_t>(L[k+1]->data)){
                                isGeneric=true;
                                auto &vec = std::get<vector_t>(L[k+1]->data).elems;
                                for(auto &tp : vec){ if(tp && std::holds_alternative<symbol>(tp->data)) tparams.push_back(std::get<symbol>(tp->data).name); }
                            }
                            if(kw=="bounds" && std::holds_alternative<vector_t>(L[k+1]->data)){
                                auto &bvec = std::get<vector_t>(L[k+1]->data).elems;
                                for(auto &b : bvec){
                                    if(!b || !std::holds_alternative<list>(b->data)) continue;
                                    auto &BL = std::get<list>(b->data).elems;
                                    // (bound T TraitName)
                                    if(BL.size()==3 && std::holds_alternative<symbol>(BL[0]->data) && std::get<symbol>(BL[0]->data).name=="bound" && std::holds_alternative<symbol>(BL[1]->data) && std::holds_alternative<symbol>(BL[2]->data)){
                                        bounds.emplace_back(std::get<symbol>(BL[1]->data).name, std::get<symbol>(BL[2]->data).name);
                                    }
                                }
                            }
                        }
                    }
                }
                if(isGeneric && !tparams.empty() && !fname.empty()){
                    generics.push_back(GenericTemplate{ fname, tparams, bounds, n });
                } else {
                    retained.push_back(n);
                }
            }
            // Helper: deep clone with type substitution
            std::function<node_ptr(const node_ptr&, const std::unordered_map<std::string,std::string>&)> clone_subst;
            clone_subst = [&](const node_ptr &n, const std::unordered_map<std::string,std::string>& subst)->node_ptr{
                if(!n) return nullptr;
                if(std::holds_alternative<symbol>(n->data)){
                    auto name = std::get<symbol>(n->data).name;
                    auto it = subst.find(name); if(it!=subst.end()) return rl_make_sym(it->second);
                    return n; // reuse
                }
                if(std::holds_alternative<list>(n->data)){
                    list out; out.elems.reserve(std::get<list>(n->data).elems.size());
                    for(auto &e : std::get<list>(n->data).elems){ out.elems.push_back(clone_subst(e, subst)); }
                    auto nn = std::make_shared<node>(*n); nn->data = out; return nn;
                }
                if(std::holds_alternative<vector_t>(n->data)){
                    vector_t v; v.elems.reserve(std::get<vector_t>(n->data).elems.size());
                    for(auto &e : std::get<vector_t>(n->data).elems){ v.elems.push_back(clone_subst(e, subst)); }
                    auto nn = std::make_shared<node>(*n); nn->data = v; return nn;
                }
                return n; // other atom types (string, integer, keyword)
            };
            // Map generic name -> template
            std::unordered_map<std::string, GenericTemplate*> gmap; for(auto &g : generics) gmap[g.name]=&g;
            // Track created specializations
            std::unordered_map<std::string, node_ptr> specializations; // specName -> fn node
            auto make_spec_name = [&](const std::string& base, const std::vector<std::string>& tys){ std::string s = base; s += "__"; for(size_t i=0;i<tys.size(); ++i){ if(i) s+="_"; s+=tys[i]; } return s; };
            // Scan retained function bodies for rcall-g forms to rewrite; collect simple errors (arity mismatch / unknown generic)
            struct PendingGenericError { node_ptr callNode; std::string code; std::string msg; };
            std::vector<PendingGenericError> genErrors;
            // Simple built-in trait satisfaction table: trait -> set of type symbols satisfying it
            std::unordered_map<std::string,std::unordered_set<std::string>> builtinTraitSatisfaction = {
                {"Addable", {"i8","i16","i32","i64","u8","u16","u32","u64","f32","f64"}},
                {"Copy", {"i8","i16","i32","i64","u8","u16","u32","u64","f32","f64","i1"}}
            };
            for(auto &n : retained){
                if(!n || !std::holds_alternative<list>(n->data)) continue;
                auto &L = std::get<list>(n->data).elems; if(L.empty()) continue;
                if(std::holds_alternative<symbol>(L[0]->data) && std::get<symbol>(L[0]->data).name=="fn"){
                    // recurse into body vectors
                    std::function<void(node_ptr&)> walk;
                    walk = [&](node_ptr &node){
                        if(!node) return;
                        if(std::holds_alternative<vector_t>(node->data)){
                            for(auto &e : std::get<vector_t>(node->data).elems) walk(e);
                            return;
                        }
                        if(!std::holds_alternative<list>(node->data)) return;
                        auto &LL = std::get<list>(node->data).elems;
                        if(LL.empty() || !std::holds_alternative<symbol>(LL[0]->data)) { for(auto &c: LL) walk(c); return; }
                        std::string op = std::get<symbol>(LL[0]->data).name;
                        if(op=="rcall-g" && LL.size()>=6){
                            // pattern: rcall-g %dst RetTy calleeSym typeArgsVec args...
                            if(!std::holds_alternative<symbol>(LL[1]->data) || !std::holds_alternative<symbol>(LL[2]->data) || !std::holds_alternative<symbol>(LL[3]->data) || !std::holds_alternative<vector_t>(LL[4]->data)) return; // malformed
                            std::string callee = std::get<symbol>(LL[3]->data).name;
                            auto itG = gmap.find(callee); if(itG==gmap.end()){
                                genErrors.push_back({ node, "E1700", std::string("unknown generic function ")+callee });
                                return;
                            }
                            auto &tvec = std::get<vector_t>(LL[4]->data).elems; if(tvec.size()!=itG->second->typeParams.size()){
                                genErrors.push_back({ node, "E1701", std::string("generic type arg arity mismatch for ")+callee });
                                return;
                            }
                            std::vector<std::string> argTypes; argTypes.reserve(tvec.size());
                            for(auto &tv : tvec){ if(tv && std::holds_alternative<symbol>(tv->data)) argTypes.push_back(std::get<symbol>(tv->data).name); else return; }
                            std::string specName = make_spec_name(callee, argTypes);
                            if(!specializations.count(specName)){
                                // create substitution map param -> concrete type
                                std::unordered_map<std::string,std::string> subst;
                                for(size_t i=0;i<argTypes.size(); ++i) subst[itG->second->typeParams[i]] = argTypes[i];
                                // (Future) bounds enforcement: verify each (T Trait) pair is satisfied by concrete type; currently skipped.
                                bool boundsOk = true;
                                for(auto &b : itG->second->bounds){
                                    auto itSub = subst.find(b.first);
                                    if(itSub==subst.end()) continue; // param missing => skip
                                    const std::string &concreteTy = itSub->second;
                                    auto traitIt = builtinTraitSatisfaction.find(b.second);
                                    if(traitIt==builtinTraitSatisfaction.end() || !traitIt->second.count(concreteTy)){
                                        genErrors.push_back({ node, "E1702", std::string("bound ")+b.first+":"+b.second+" unsatisfied by "+concreteTy });
                                        boundsOk=false; break;
                                    }
                                }
                                if(!boundsOk){
                                    // Do not create specialization; leave call as rcall-g so type checker still sees error diagnostic node.
                                } else {
                                // clone function list and patch :name & remove :generics
                                if(itG->second->fnList && std::holds_alternative<list>(itG->second->fnList->data)){
                                    auto fnClone = clone_subst(itG->second->fnList, subst);
                                    auto &F = std::get<list>(fnClone->data).elems;
                                    // Iterate keyword/value pairs; allow erasure without unsigned wraparound hacks.
                                    for(size_t k=1; k+1 < F.size(); ){
                                        if(!std::holds_alternative<keyword>(F[k]->data)) break;
                                        std::string kw = std::get<keyword>(F[k]->data).name;
                                        if(kw=="name" && std::holds_alternative<std::string>(F[k+1]->data)) {
                                            F[k+1] = edn::n_str(specName);
                                            k += 2;
                                            continue;
                                        }
                                        if(kw=="generics" || kw=="bounds"){
                                            // Erase this kw/value pair (drop surface-only metadata in specialization).
                                            // Cast k to difference_type to avoid -Wsign-conversion noise on iterator arithmetic.
                                            F.erase(F.begin()+static_cast<std::ptrdiff_t>(k), F.begin()+static_cast<std::ptrdiff_t>(k+2));
                                            // Do not advance k; next element now occupies index k.
                                            continue;
                                        }
                                        // Unhandled keyword: advance.
                                        k += 2;
                                    }
                                    specializations[specName] = fnClone;
                                }
                }
                            }
                            // Rewrite rcall-g to call specialized function
                            list callL; callL.elems.push_back(rl_make_sym("call"));
                            callL.elems.push_back(LL[1]); // %dst
                            callL.elems.push_back(LL[2]); // RetTy
                            callL.elems.push_back(rl_make_sym(specName));
                            for(size_t ai=5; ai<LL.size(); ++ai) callL.elems.push_back(LL[ai]);
                            node->data = callL;
                            return; // done
                        }
                        // generic recursion for nested structures
                        for(auto &c : LL) walk(c);
                    };
                    walk(n);
                }
            }
            // Rebuild module element list: retained + generated specializations
            modList.elems = retained;
            for(auto &kv : specializations){ modList.elems.push_back(kv.second); }
            // (Transitional) encode generic errors as metadata on the module head; downstream can surface.
            if(!genErrors.empty() && !modList.elems.empty()){
                auto &head = modList.elems[0]; // module symbol list start
                if(head && std::holds_alternative<symbol>(head->data)){
                    for(auto &ge : genErrors){
                        auto metaNode = rl_make_sym(ge.code+":"+ge.msg);
                        head->metadata["generic-error-"+ge.code+"-"+std::to_string(head->metadata.size())] = metaNode;
                    }
                }
            }
        }
    }

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
