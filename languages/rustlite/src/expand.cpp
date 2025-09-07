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

namespace rustlite
{

    using rustlite::rl_gensym;
    using rustlite::rl_make_i64;
    using rustlite::rl_make_kw;
    using rustlite::rl_make_sym;

    // Single source of truth for feature flags lives in features.hpp (no local fallbacks here).

    edn::node_ptr expand_rustlite(const edn::node_ptr &module_ast)
    {
        // Pre-walk: rewrite variant constructor surface forms of shape (Type::Variant %dst [payload*])
        // to an internal generic macro form: (enum-ctor %dst Type Variant payload...)
        // Assumption: destination SSA symbol always provided as first argument (unlike earlier prose examples
        // which omitted it). This keeps lowering consistent with existing sum-new op which requires %dst.
        std::function<void(node_ptr &)> prewalk;
        prewalk = [&](node_ptr &n)
        {
            if (!n)
                return;
            if (std::holds_alternative<vector_t>(n->data))
            {
                for (auto &e : std::get<vector_t>(n->data).elems)
                    prewalk(e);
                return;
            }
            if (!std::holds_alternative<list>(n->data))
                return;
            auto &L = std::get<list>(n->data).elems;
            if (L.empty() || !std::holds_alternative<symbol>(L[0]->data))
            {
                for (auto &e : L)
                    prewalk(e);
                return;
            }
            std::string head = std::get<symbol>(L[0]->data).name;
            auto pos = head.find("::");
            if (pos != std::string::npos && L.size() >= 2 && std::holds_alternative<symbol>(L[1]->data) && std::get<symbol>(L[1]->data).name.rfind('%', 0) == 0)
            {
                std::string typeName = head.substr(0, pos);
                std::string variantName = head.substr(pos + 2);
                // Build new list: enum-ctor %dst Type Variant <payload...>
                list repl;
                repl.elems.push_back(rl_make_sym("enum-ctor"));
                repl.elems.push_back(L[1]);
                repl.elems.push_back(rl_make_sym(typeName));
                repl.elems.push_back(rl_make_sym(variantName));
                for (size_t i = 2; i < L.size(); ++i)
                {
                    repl.elems.push_back(L[i]);
                }
                n->data = repl; // replace in-place
                // Recurse into payload forms if any
                for (size_t i = 4; i < repl.elems.size(); ++i)
                    prewalk(repl.elems[i]);
                return;
            }
            for (auto &e : L)
                prewalk(e);
        };
        auto ast_copy = module_ast; // operate on provided AST (shared_ptr semantics)
        prewalk(ast_copy);

        // Optional pre-expansion rewrite: closure capture inference.
        // If RUSTLITE_INFER_CAPS=1 and an (rclosure %c callee ...) form lacks a :captures vector,
        // heuristically capture the symbol defined immediately prior in the same block (vector sequence).
        if (rustlite::infer_captures_enabled())
        {
            std::function<void(node_ptr &)> closure_walk;
            closure_walk = [&](node_ptr &n)
            {
                if (!n)
                    return;
                if (std::holds_alternative<vector_t>(n->data))
                {
                    auto &vec = std::get<vector_t>(n->data);
                    // Walk with index to inspect previous sibling
                    for (size_t i = 0; i < vec.elems.size(); ++i)
                    {
                        auto &elem = vec.elems[i];
                        if (elem && std::holds_alternative<list>(elem->data))
                        {
                            auto &L = std::get<list>(elem->data).elems;
                            if (!L.empty() && std::holds_alternative<symbol>(L[0]->data) && std::get<symbol>(L[0]->data).name == "rclosure")
                            {
                                bool hasCaptures = false;
                                for (size_t j = 1; j + 1 < L.size(); j += 2)
                                {
                                    if (std::holds_alternative<keyword>(L[j]->data) && std::get<keyword>(L[j]->data).name == "captures")
                                    {
                                        hasCaptures = true;
                                        break;
                                    }
                                    else if (!std::holds_alternative<keyword>(L[j]->data))
                                        break;
                                }
                                if (!hasCaptures)
                                {
                                    // candidate: previous sibling list defines symbol via (const %sym Ty ...) or (as %sym Ty ...)
                                    std::string capSymName;
                                    if (i > 0)
                                    {
                                        auto &prev = vec.elems[i - 1];
                                        if (prev && std::holds_alternative<list>(prev->data))
                                        {
                                            auto &PL = std::get<list>(prev->data).elems;
                                            if (PL.size() >= 4 && std::holds_alternative<symbol>(PL[0]->data))
                                            {
                                                std::string op = std::get<symbol>(PL[0]->data).name;
                                                if ((op == "const" || op == "as") && std::holds_alternative<symbol>(PL[1]->data))
                                                    capSymName = std::get<symbol>(PL[1]->data).name;
                                            }
                                        }
                                    }
                                    if (!capSymName.empty())
                                    {
                                        // Insert :captures [ %sym ] just after callee symbol (expected order: head %dst callee ...)
                                        // Form: (rclosure %c callee :captures [ %capt ])
                                        // Find insertion point before first keyword argument.
                                        size_t insertPos = L.size();
                                        for (size_t k = 1; k < L.size(); ++k)
                                        {
                                            if (std::holds_alternative<keyword>(L[k]->data))
                                            {
                                                insertPos = k;
                                                break;
                                            }
                                        }
                                        vector_t capVec;
                                        capVec.elems.push_back(rustlite::rl_make_sym(capSymName));
                                        auto capVecNode = std::make_shared<node>(node{capVec, {}});
                                        auto it = std::next(L.begin(), static_cast<long>(insertPos));
                                        it = L.insert(it, rl_make_kw("captures"));
                                        L.insert(std::next(it), capVecNode);
                                    }
                                }
                            }
                        }
                        if (vec.elems[i])
                            closure_walk(vec.elems[i]);
                    }
                    return;
                }
                if (std::holds_alternative<list>(n->data))
                {
                    auto &L = std::get<list>(n->data).elems;
                    for (auto &c : L)
                        closure_walk(c);
                    return;
                }
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
        if (expanded && std::holds_alternative<list>(expanded->data))
        {
            auto &modList = std::get<list>(expanded->data);
            if (!modList.elems.empty() && std::holds_alternative<symbol>(modList.elems[0]->data) && std::get<symbol>(modList.elems[0]->data).name == "module")
            {
                // Collect generic function templates and remove them from module (we'll append specializations after cloning)
                struct GenericTemplate
                {
                    std::string name;
                    std::vector<std::string> typeParams;
                    std::vector<std::pair<std::string, std::string>> bounds;
                    node_ptr fnList;
                };
                std::vector<GenericTemplate> generics;
                std::vector<node_ptr> retained;
                retained.reserve(modList.elems.size());
                retained.push_back(modList.elems[0]); // keep module head
                for (size_t i = 1; i < modList.elems.size(); ++i)
                {
                    auto &n = modList.elems[i];
                    bool isGeneric = false;
                    std::vector<std::string> tparams;
                    std::vector<std::pair<std::string, std::string>> bounds;
                    std::string fname;
                    if (n && std::holds_alternative<list>(n->data))
                    {
                        auto &L = std::get<list>(n->data).elems;
                        if (!L.empty() && std::holds_alternative<symbol>(L[0]->data) && std::get<symbol>(L[0]->data).name == "fn")
                        {
                            // scan keywords
                            for (size_t k = 1; k + 1 < L.size(); k += 2)
                            {
                                if (!std::holds_alternative<keyword>(L[k]->data))
                                    break;
                                std::string kw = std::get<keyword>(L[k]->data).name;
                                if (kw == "name" && std::holds_alternative<std::string>(L[k + 1]->data))
                                    fname = std::get<std::string>(L[k + 1]->data); // function name stored as string
                                if (kw == "generics" && std::holds_alternative<vector_t>(L[k + 1]->data))
                                {
                                    isGeneric = true;
                                    auto &vec = std::get<vector_t>(L[k + 1]->data).elems;
                                    for (auto &tp : vec)
                                    {
                                        if (tp && std::holds_alternative<symbol>(tp->data))
                                            tparams.push_back(std::get<symbol>(tp->data).name);
                                    }
                                }
                                if (kw == "bounds" && std::holds_alternative<vector_t>(L[k + 1]->data))
                                {
                                    auto &bvec = std::get<vector_t>(L[k + 1]->data).elems;
                                    for (auto &b : bvec)
                                    {
                                        if (!b || !std::holds_alternative<list>(b->data))
                                            continue;
                                        auto &BL = std::get<list>(b->data).elems;
                                        // (bound T TraitName)
                                        if (BL.size() == 3 && std::holds_alternative<symbol>(BL[0]->data) && std::get<symbol>(BL[0]->data).name == "bound" && std::holds_alternative<symbol>(BL[1]->data) && std::holds_alternative<symbol>(BL[2]->data))
                                        {
                                            bounds.emplace_back(std::get<symbol>(BL[1]->data).name, std::get<symbol>(BL[2]->data).name);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (isGeneric && !tparams.empty() && !fname.empty())
                    {
                        generics.push_back(GenericTemplate{fname, tparams, bounds, n});
                    }
                    else
                    {
                        retained.push_back(n);
                    }
                }
                // Helper: deep clone with type substitution
                std::function<node_ptr(const node_ptr &, const std::unordered_map<std::string, std::string> &)> clone_subst;
                clone_subst = [&](const node_ptr &n, const std::unordered_map<std::string, std::string> &subst) -> node_ptr
                {
                    if (!n)
                        return nullptr;
                    if (std::holds_alternative<symbol>(n->data))
                    {
                        auto name = std::get<symbol>(n->data).name;
                        auto it = subst.find(name);
                        if (it != subst.end())
                            return rl_make_sym(it->second);
                        return n; // reuse
                    }
                    if (std::holds_alternative<list>(n->data))
                    {
                        list out;
                        out.elems.reserve(std::get<list>(n->data).elems.size());
                        for (auto &e : std::get<list>(n->data).elems)
                        {
                            out.elems.push_back(clone_subst(e, subst));
                        }
                        auto nn = std::make_shared<node>(*n);
                        nn->data = out;
                        return nn;
                    }
                    if (std::holds_alternative<vector_t>(n->data))
                    {
                        vector_t v;
                        v.elems.reserve(std::get<vector_t>(n->data).elems.size());
                        for (auto &e : std::get<vector_t>(n->data).elems)
                        {
                            v.elems.push_back(clone_subst(e, subst));
                        }
                        auto nn = std::make_shared<node>(*n);
                        nn->data = v;
                        return nn;
                    }
                    return n; // other atom types (string, integer, keyword)
                };
                // Map generic name -> template
                std::unordered_map<std::string, GenericTemplate *> gmap;
                for (auto &g : generics)
                    gmap[g.name] = &g;
                // Track created specializations
                std::unordered_map<std::string, node_ptr> specializations; // specName -> fn node
                auto make_spec_name = [&](const std::string &base, const std::vector<std::string> &tys)
                { std::string s = base; s += "__"; for(size_t i=0;i<tys.size(); ++i){ if(i) s+="_"; s+=tys[i]; } return s; };
                // Scan retained function bodies for rcall-g forms to rewrite; collect simple errors (arity mismatch / unknown generic)
                struct PendingGenericError
                {
                    node_ptr callNode;
                    std::string code;
                    std::string msg;
                };
                std::vector<PendingGenericError> genErrors;
                // Simple built-in trait satisfaction table: trait -> set of type symbols satisfying it
                std::unordered_map<std::string, std::unordered_set<std::string>> builtinTraitSatisfaction = {
                    {"Addable", {"i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64"}},
                    {"Copy", {"i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64", "i1"}}};
                for (auto &n : retained)
                {
                    if (!n || !std::holds_alternative<list>(n->data))
                        continue;
                    auto &L = std::get<list>(n->data).elems;
                    if (L.empty())
                        continue;
                    if (std::holds_alternative<symbol>(L[0]->data) && std::get<symbol>(L[0]->data).name == "fn")
                    {
                        // recurse into body vectors
                        std::function<void(node_ptr &)> walk;
                        walk = [&](node_ptr &node)
                        {
                            if (!node)
                                return;
                            if (std::holds_alternative<vector_t>(node->data))
                            {
                                for (auto &e : std::get<vector_t>(node->data).elems)
                                    walk(e);
                                return;
                            }
                            if (!std::holds_alternative<list>(node->data))
                                return;
                            auto &LL = std::get<list>(node->data).elems;
                            if (LL.empty() || !std::holds_alternative<symbol>(LL[0]->data))
                            {
                                for (auto &c : LL)
                                    walk(c);
                                return;
                            }
                            std::string op = std::get<symbol>(LL[0]->data).name;
                            if (op == "rcall-g" && LL.size() >= 6)
                            {
                                // pattern: rcall-g %dst RetTy calleeSym typeArgsVec args...
                                if (!std::holds_alternative<symbol>(LL[1]->data) || !std::holds_alternative<symbol>(LL[2]->data) || !std::holds_alternative<symbol>(LL[3]->data) || !std::holds_alternative<vector_t>(LL[4]->data))
                                    return; // malformed
                                std::string callee = std::get<symbol>(LL[3]->data).name;
                                auto itG = gmap.find(callee);
                                if (itG == gmap.end())
                                {
                                    genErrors.push_back({node, "E1700", std::string("unknown generic function ") + callee});
                                    return;
                                }
                                auto &tvec = std::get<vector_t>(LL[4]->data).elems;
                                if (tvec.size() != itG->second->typeParams.size())
                                {
                                    genErrors.push_back({node, "E1701", std::string("generic type arg arity mismatch for ") + callee});
                                    return;
                                }
                                std::vector<std::string> argTypes;
                                argTypes.reserve(tvec.size());
                                for (auto &tv : tvec)
                                {
                                    if (tv && std::holds_alternative<symbol>(tv->data))
                                        argTypes.push_back(std::get<symbol>(tv->data).name);
                                    else
                                        return;
                                }
                                std::string specName = make_spec_name(callee, argTypes);
                                if (!specializations.count(specName))
                                {
                                    // create substitution map param -> concrete type
                                    std::unordered_map<std::string, std::string> subst;
                                    for (size_t i = 0; i < argTypes.size(); ++i)
                                        subst[itG->second->typeParams[i]] = argTypes[i];
                                    // (Future) bounds enforcement: verify each (T Trait) pair is satisfied by concrete type; currently skipped.
                                    bool boundsOk = true;
                                    for (auto &b : itG->second->bounds)
                                    {
                                        auto itSub = subst.find(b.first);
                                        if (itSub == subst.end())
                                            continue; // param missing => skip
                                        const std::string &concreteTy = itSub->second;
                                        auto traitIt = builtinTraitSatisfaction.find(b.second);
                                        if (traitIt == builtinTraitSatisfaction.end() || !traitIt->second.count(concreteTy))
                                        {
                                            genErrors.push_back({node, "E1702", std::string("bound ") + b.first + ":" + b.second + " unsatisfied by " + concreteTy});
                                            boundsOk = false;
                                            break;
                                        }
                                    }
                                    if (!boundsOk)
                                    {
                                        // Do not create specialization; leave call as rcall-g so type checker still sees error diagnostic node.
                                    }
                                    else
                                    {
                                        // clone function list and patch :name & remove :generics
                                        if (itG->second->fnList && std::holds_alternative<list>(itG->second->fnList->data))
                                        {
                                            auto fnClone = clone_subst(itG->second->fnList, subst);
                                            auto &F = std::get<list>(fnClone->data).elems;
                                            // Iterate keyword/value pairs; allow erasure without unsigned wraparound hacks.
                                            for (size_t k = 1; k + 1 < F.size();)
                                            {
                                                if (!std::holds_alternative<keyword>(F[k]->data))
                                                    break;
                                                std::string kw = std::get<keyword>(F[k]->data).name;
                                                if (kw == "name" && std::holds_alternative<std::string>(F[k + 1]->data))
                                                {
                                                    F[k + 1] = edn::n_str(specName);
                                                    k += 2;
                                                    continue;
                                                }
                                                if (kw == "generics" || kw == "bounds")
                                                {
                                                    // Erase this kw/value pair (drop surface-only metadata in specialization).
                                                    // Cast k to difference_type to avoid -Wsign-conversion noise on iterator arithmetic.
                                                    F.erase(F.begin() + static_cast<std::ptrdiff_t>(k), F.begin() + static_cast<std::ptrdiff_t>(k + 2));
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
                                list callL;
                                callL.elems.push_back(rl_make_sym("call"));
                                callL.elems.push_back(LL[1]); // %dst
                                callL.elems.push_back(LL[2]); // RetTy
                                callL.elems.push_back(rl_make_sym(specName));
                                for (size_t ai = 5; ai < LL.size(); ++ai)
                                    callL.elems.push_back(LL[ai]);
                                node->data = callL;
                                return; // done
                            }
                            // generic recursion for nested structures
                            for (auto &c : LL)
                                walk(c);
                        };
                        walk(n);
                    }
                }
                // Rebuild module element list: retained + generated specializations
                modList.elems = retained;
                for (auto &kv : specializations)
                {
                    modList.elems.push_back(kv.second);
                }
                // (Transitional) encode generic errors as metadata on the module head; downstream can surface.
                if (!genErrors.empty() && !modList.elems.empty())
                {
                    auto &head = modList.elems[0]; // module symbol list start
                    if (head && std::holds_alternative<symbol>(head->data))
                    {
                        for (auto &ge : genErrors)
                        {
                            auto metaNode = rl_make_sym(ge.code + ":" + ge.msg);
                            head->metadata["generic-error-" + ge.code + "-" + std::to_string(head->metadata.size())] = metaNode;
                        }
                    }
                }
            }
        }

        // Inject struct declarations for each used tuple arity if missing.
        // Attempt lightweight field type inference by scanning preceding const/as instructions
        // for each struct-lit usage of a tuple. Fallback to i32 if any field ambiguous.
        if (expanded && std::holds_alternative<list>(expanded->data))
        {
            auto &modList = std::get<list>(expanded->data);
            if (!modList.elems.empty() && std::holds_alternative<symbol>(modList.elems[0]->data) && std::get<symbol>(modList.elems[0]->data).name == "module")
            {
                // --- Tuple pattern diagnostics (E1454/E1455) ---
                // We approximate a tuple pattern destructure as a contiguous cluster of (tget %dst <Ty> %tuple <idx>)
                // instructions with ascending <idx> starting at 0 emitted immediately after the let pattern.
                // We validate that:
                //  1. The source tuple variable has a recorded arity; if not -> E1455 (non-tuple target).
                //  2. The number of contiguous indices equals the recorded arity; if not -> E1454 (arity mismatch).
                // Limitations (acceptable for Phase 1):
                //  * Only clusters where each tget uses the same %tuple symbol and indices are dense from 0..k-1.
                //  * Stops cluster when encountering a gap, different tuple var, or non-tget instruction.
                auto emit_tuple_pattern_generic_error = [&](node_ptr headNode, const std::string &code, const std::string &msg)
                {
                    // Reuse generic-error metadata channel to surface diagnostics in type checker (similar to generics errors).
                    if (!headNode || !std::holds_alternative<symbol>(headNode->data))
                        return;
                    auto metaNode = rl_make_sym(code + ":" + msg);
                    headNode->metadata["generic-error-" + code + "-" + std::to_string(headNode->metadata.size())] = metaNode;
                };
                if (!modList.elems.empty())
                {
                    node_ptr moduleHead = modList.elems[0];
                    // Walk function bodies to find clusters.
                    for (auto &top : modList.elems)
                    {
                        if (!top || !std::holds_alternative<list>(top->data))
                            continue;
                        auto &fnL = std::get<list>(top->data).elems;
                        if (fnL.empty() || !std::holds_alternative<symbol>(fnL[0]->data))
                            continue;
                        std::string head = std::get<symbol>(fnL[0]->data).name;
                        if (head != "fn" && head != "rfn")
                            continue;
                        // locate :body vector
                        node_ptr bodyVec = nullptr;
                        for (size_t i = 1; i + 1 < fnL.size(); i += 2)
                        {
                            if (!std::holds_alternative<keyword>(fnL[i]->data))
                                break;
                            if (std::get<keyword>(fnL[i]->data).name == "body")
                            {
                                bodyVec = fnL[i + 1];
                                break;
                            }
                        }
                        if (!bodyVec || !std::holds_alternative<vector_t>(bodyVec->data))
                            continue;
                        auto &instrs = std::get<vector_t>(bodyVec->data).elems;
                        // --- Struct pattern diagnostics (Phase 2b) ---
                        // Parser emits:
                        //   (member %bind Type %src field)
                        // for each bound field, plus
                        //   (struct-pattern-meta %src Type <count> f1 f2 ...)
                        // Optional duplicate fields meta already emitted by parser:
                        //   (struct-pattern-duplicate-fields %src [dup1 dup2 ...])
                        // We validate unknown fields by checking that each referenced field exists in a known struct definition.
                        // Current simplified model: we treat absence of a struct declaration as unknown-type => skip unknown field check.
                        // If a struct declaration exists (look for (struct Type [...] ) in module), collect its field names for validation.
                        // Emit generic-error metadata codes:
                        //   E1456: unknown struct field in pattern
                        auto emit_struct_pattern_error = [&](const std::string &code, const std::string &msg)
                        {
                            if (!moduleHead || !std::holds_alternative<symbol>(moduleHead->data))
                                return;
                            auto metaNode = rl_make_sym(code + ":" + msg);
                            moduleHead->metadata["generic-error-" + code + "-" + std::to_string(moduleHead->metadata.size())] = metaNode;
                        };
                        // Build map of struct -> allowed fields (only once per function scan)
                        static thread_local std::unordered_map<std::string, std::unordered_set<std::string>> structFieldsCache;
                        structFieldsCache.clear();
                        for (auto &n2 : modList.elems)
                        {
                            if (!n2 || !std::holds_alternative<list>(n2->data))
                                continue;
                            auto &SL = std::get<list>(n2->data).elems;
                            if (SL.empty() || !std::holds_alternative<symbol>(SL[0]->data))
                                continue;
                            if (std::get<symbol>(SL[0]->data).name != "struct")
                                continue;
                            // Support two shapes:
                            //  (struct Type [ name type name type ... ])  -- legacy simplified form
                            //  (struct :name Type :fields [ (field :name n :type T) ... ]) -- keyword form
                            std::string sname;
                            std::unordered_set<std::string> fnames;
                            bool captured = false;
                            if (SL.size() >= 3 && std::holds_alternative<symbol>(SL[1]->data) && std::holds_alternative<vector_t>(SL[2]->data))
                            {
                                sname = std::get<symbol>(SL[1]->data).name;
                                auto &vec = std::get<vector_t>(SL[2]->data).elems;
                                for (size_t i = 0; i < vec.size(); ++i)
                                { // even indices are names in simplified form
                                    auto &nv = vec[i];
                                    if (!nv)
                                        continue;
                                    if (!std::holds_alternative<symbol>(nv->data))
                                        continue;
                                    std::string fname = std::get<symbol>(nv->data).name;
                                    if (!fname.empty() && fname[0] != '%' && (i % 2 == 0))
                                        fnames.insert(fname);
                                }
                                captured = true;
                            }
                            else
                            {
                                // keyword form; scan for :name and :fields
                                node_ptr fieldsNode = nullptr;
                                for (size_t i = 1; i + 1 < SL.size(); i += 2)
                                {
                                    if (!SL[i] || !std::holds_alternative<keyword>(SL[i]->data))
                                        break; // stop at first non-keyword
                                    std::string kw = std::get<keyword>(SL[i]->data).name;
                                    auto val = SL[i + 1];
                                    if (kw == "name" && val && std::holds_alternative<symbol>(val->data))
                                        sname = std::get<symbol>(val->data).name;
                                    else if (kw == "fields" && val && std::holds_alternative<vector_t>(val->data))
                                        fieldsNode = val;
                                }
                                if (!sname.empty() && fieldsNode)
                                {
                                    for (auto &f : std::get<vector_t>(fieldsNode->data).elems)
                                    {
                                        if (!f || !std::holds_alternative<list>(f->data))
                                            continue;
                                        auto &FL = std::get<list>(f->data).elems;
                                        if (FL.empty())
                                            continue;
                                        if (std::holds_alternative<symbol>(FL[0]->data) && std::get<symbol>(FL[0]->data).name == "field")
                                        {
                                            // locate :name keyword
                                            for (size_t j = 1; j + 1 < FL.size(); j += 2)
                                            {
                                                if (!FL[j] || !std::holds_alternative<keyword>(FL[j]->data))
                                                    break;
                                                if (std::get<keyword>(FL[j]->data).name == "name" && FL[j + 1] && std::holds_alternative<symbol>(FL[j + 1]->data))
                                                {
                                                    std::string fname = std::get<symbol>(FL[j + 1]->data).name;
                                                    if (!fname.empty() && fname[0] != '%')
                                                        fnames.insert(fname);
                                                }
                                            }
                                        }
                                    }
                                    if (!fnames.empty())
                                        captured = true;
                                }
                            }
                            if (captured && !sname.empty())
                                structFieldsCache[sname] = std::move(fnames);
                        }
                        // Scan for struct-pattern-meta instructions and validate member field names against struct declaration if present.
                        for (size_t si = 0; si < instrs.size(); ++si)
                        {
                            auto &instS = instrs[si];
                            if (!instS || !std::holds_alternative<list>(instS->data))
                                continue;
                            auto &LS = std::get<list>(instS->data).elems;
                            if (LS.size() >= 5 && std::holds_alternative<symbol>(LS[0]->data) && std::get<symbol>(LS[0]->data).name == "struct-pattern-meta")
                            {
                                if (!std::holds_alternative<symbol>(LS[1]->data) || !std::holds_alternative<symbol>(LS[2]->data) || !std::holds_alternative<int64_t>(LS[3]->data))
                                    continue;
                                std::string srcVar = std::get<symbol>(LS[1]->data).name;
                                std::string typeName = std::get<symbol>(LS[2]->data).name;
                                size_t count = (size_t)std::get<int64_t>(LS[3]->data);
                                if (count + 4 != LS.size())
                                    continue; // ensure expected arity
                                auto itSF = structFieldsCache.find(typeName);
                                if (itSF == structFieldsCache.end())
                                    continue; // unknown struct: skip
                                for (size_t fi = 0; fi < count; ++fi)
                                {
                                    auto &fldNode = LS[4 + fi];
                                    if (!fldNode || !std::holds_alternative<symbol>(fldNode->data))
                                        continue;
                                    std::string fld = std::get<symbol>(fldNode->data).name;
                                    if (!itSF->second.count(fld))
                                    {
                                        emit_struct_pattern_error("E1456", "unknown field " + fld + " in pattern for " + typeName);
                                    }
                                }
                            }
                        }
                        // Scan for duplicate field meta emitted by parser and convert to diagnostic E1457.
                        for (size_t si = 0; si < instrs.size(); ++si)
                        {
                            auto &instS = instrs[si];
                            if (!instS || !std::holds_alternative<list>(instS->data))
                                continue;
                            auto &LS = std::get<list>(instS->data).elems;
                            if (LS.size() >= 3 && std::holds_alternative<symbol>(LS[0]->data) && std::get<symbol>(LS[0]->data).name == "struct-pattern-duplicate-fields")
                            {
                                // Shape: (struct-pattern-duplicate-fields %src [d1 d2 ...])
                                if (!std::holds_alternative<symbol>(LS[1]->data) || !std::holds_alternative<vector_t>(LS[2]->data))
                                    continue;
                                auto &vec = std::get<vector_t>(LS[2]->data).elems;
                                std::vector<std::string> dups;
                                for (auto &dv : vec)
                                {
                                    if (!dv || !std::holds_alternative<symbol>(dv->data))
                                        continue;
                                    dups.push_back(std::get<symbol>(dv->data).name);
                                }
                                if (!dups.empty())
                                {
                                    std::string msg = "duplicate field" + (dups.size() > 1 ? std::string("s ") : std::string(" "));
                                    bool first = true;
                                    for (auto &d : dups)
                                    {
                                        if (!first)
                                            msg += " ";
                                        first = false;
                                        msg += d;
                                    }
                                    msg += " in struct pattern";
                                    emit_struct_pattern_error("E1457", msg);
                                }
                            }
                        }
                        for (size_t i = 0; i < instrs.size(); ++i)
                        {
                            auto &inst = instrs[i];
                            if (!inst || !std::holds_alternative<list>(inst->data))
                                continue;
                            // Fast-path: explicit meta emitted by parser: (tuple-pattern-meta %var expectedCount)
                            {
                                auto &Lmeta = std::get<list>(inst->data).elems;
                                if (Lmeta.size() == 3 && std::holds_alternative<symbol>(Lmeta[0]->data) && std::get<symbol>(Lmeta[0]->data).name == "tuple-pattern-meta")
                                {
                                    if (std::holds_alternative<symbol>(Lmeta[1]->data) && std::holds_alternative<int64_t>(Lmeta[2]->data))
                                    {
                                        std::string tup = std::get<symbol>(Lmeta[1]->data).name;
                                        size_t expected = (size_t)std::get<int64_t>(Lmeta[2]->data);
                                        size_t recorded = 0;
                                        bool have = false;
                                        if (!tup.empty() && tup[0] == '%')
                                        {
                                            auto itA = macroCtx->tupleVarArity.find(tup.substr(1));
                                            if (itA != macroCtx->tupleVarArity.end())
                                            {
                                                recorded = itA->second;
                                                have = true;
                                            }
                                        }
                                        if (!have)
                                        {
                                            // Phase 2 relaxation: Suppress E1455 for tuple-pattern-meta when the macro layer
                                            // has not recorded an arity for the source symbol. This situation now arises for
                                            // match arm surface patterns where we emit (tget ...) clusters / meta prior to
                                            // tuple struct materialization. Previously this produced a noisy false positive
                                            // (E1455: tuple pattern target not tuple). We defer validation until a tuple
                                            // struct synthetic declaration exists (recorded arity) or later phases add richer
                                            // typing. Intentional no-op here.
                                        }
                                        else if (recorded != expected)
                                        {
                                            emit_tuple_pattern_generic_error(moduleHead, "E1454", "tuple pattern arity mismatch expected" + std::to_string(recorded) + " got" + std::to_string(expected));
                                        }
                                        else
                                        {
                                            // Over-arity detection (Phase 2a meta path): scan preceding contiguous pattern extraction
                                            // instructions (either lowered (member %dst __TupleN %tup _idx) or unreduced
                                            // (tget %dst Ty %tup idx)) to find the highest referenced index.
                                            // We walk backwards until a non-matching instruction is found.
                                            size_t highestIndexPlusOne = 0;
                                            bool any = false;
                                            size_t bi = i; // meta at instrs[i]
                                            while (bi > 0)
                                            {
                                                size_t prev = bi - 1;
                                                auto &pinst = instrs[prev];
                                                if (!pinst || !std::holds_alternative<list>(pinst->data))
                                                    break;
                                                auto &PL = std::get<list>(pinst->data).elems;
                                                if (PL.size() != 5)
                                                    break;
                                                if (!std::holds_alternative<symbol>(PL[0]->data))
                                                    break;
                                                std::string op = std::get<symbol>(PL[0]->data).name;
                                                if (op == "tget")
                                                {
                                                    if (!std::holds_alternative<symbol>(PL[3]->data) || std::get<symbol>(PL[3]->data).name != tup)
                                                        break;
                                                    if (!std::holds_alternative<int64_t>(PL[4]->data))
                                                        break;
                                                    int64_t idx = std::get<int64_t>(PL[4]->data);
                                                    if (idx < 0)
                                                        break;
                                                    any = true;
                                                    if ((size_t)idx + 1 > highestIndexPlusOne)
                                                        highestIndexPlusOne = (size_t)idx + 1;
                                                    bi = prev;
                                                    continue;
                                                }
                                                else if (op == "member")
                                                {
                                                    // (member %dst __TupleN %tup _idx)
                                                    if (!std::holds_alternative<symbol>(PL[2]->data))
                                                        break; // struct name
                                                    if (!std::holds_alternative<symbol>(PL[3]->data) || std::get<symbol>(PL[3]->data).name != tup)
                                                        break; // tuple var
                                                    if (!std::holds_alternative<symbol>(PL[4]->data))
                                                        break; // field symbol _k
                                                    std::string field = std::get<symbol>(PL[4]->data).name;
                                                    if (field.size() < 2 || field[0] != '_')
                                                        break;
                                                    try
                                                    {
                                                        size_t idx = (size_t)std::stoul(field.substr(1));
                                                        any = true;
                                                        if (idx + 1 > highestIndexPlusOne)
                                                            highestIndexPlusOne = idx + 1;
                                                        bi = prev;
                                                        continue;
                                                    }
                                                    catch (...)
                                                    {
                                                        break;
                                                    }
                                                }
                                                else
                                                {
                                                    break; // other op => stop
                                                }
                                            }
                                            if (any && highestIndexPlusOne > recorded)
                                            {
                                                emit_tuple_pattern_generic_error(moduleHead, "E1454", "tuple pattern arity mismatch expected" + std::to_string(recorded) + " got" + std::to_string(highestIndexPlusOne));
                                            }
                                        }
                                        continue; // do not treat meta as cluster start
                                    }
                                }
                            }
                            auto &L = std::get<list>(inst->data).elems;
                            if (L.size() != 5)
                                continue;
                            if (!std::holds_alternative<symbol>(L[0]->data))
                                continue;
                            if (std::get<symbol>(L[0]->data).name != "tget")
                                continue;
                            // Potential start of cluster: require index literal 0.
                            if (!std::holds_alternative<int64_t>(L[4]->data) || std::get<int64_t>(L[4]->data) != 0)
                                continue;
                            if (!std::holds_alternative<symbol>(L[3]->data))
                                continue;
                            std::string tupleSym = std::get<symbol>(L[3]->data).name;
                            if (tupleSym.empty() || tupleSym[0] != '%')
                                continue;
                            // Gather cluster
                            size_t clusterCount = 0;
                            size_t j = i;
                            bool indicesDense = true;
                            while (j < instrs.size())
                            {
                                auto &inst2 = instrs[j];
                                if (!inst2 || !std::holds_alternative<list>(inst2->data))
                                    break;
                                auto &L2 = std::get<list>(inst2->data).elems;
                                if (L2.size() != 5)
                                    break;
                                if (!std::holds_alternative<symbol>(L2[0]->data) || std::get<symbol>(L2[0]->data).name != "tget")
                                    break;
                                if (!std::holds_alternative<symbol>(L2[3]->data) || std::get<symbol>(L2[3]->data).name != tupleSym)
                                    break;
                                if (!std::holds_alternative<int64_t>(L2[4]->data))
                                    break;
                                int64_t idx = std::get<int64_t>(L2[4]->data);
                                if (idx != (int64_t)clusterCount)
                                {
                                    indicesDense = false;
                                    break;
                                }
                                ++clusterCount;
                                ++j;
                            }
                            if (clusterCount == 0 || !indicesDense)
                                continue; // not a valid pattern cluster
                            // Lookup recorded arity.
                            size_t recorded = 0;
                            bool haveArity = false;
                            auto itA = macroCtx->tupleVarArity.find(tupleSym.substr(1));
                            if (itA != macroCtx->tupleVarArity.end())
                            {
                                recorded = itA->second;
                                haveArity = true;
                            }
                            if (!haveArity)
                            {
                                // Phase 2 relaxation (see above): skip E1455 for raw tget cluster when arity unknown.
                                // This allows match arm tuple patterns on values without prior tuple struct typing.
                                continue;
                            }
                            if (recorded != clusterCount)
                            {
                                emit_tuple_pattern_generic_error(moduleHead, "E1454", "tuple pattern arity mismatch expected" + std::to_string(recorded) + " got" + std::to_string(clusterCount));
                            }
                            // Advance i past cluster to avoid re-processing
                            i += clusterCount - 1;
                        }
                        // Secondary pattern: after macro rewrite, (tget ...) become (member %dst __TupleN %tup _idx).
                        // If a user destructures fewer fields than the tuple arity, we only see the emitted members.
                        // Detect clusters of member ops with struct name __TupleN, starting at field _0 with dense _0.._k-1.
                        for (size_t mi = 0; mi < instrs.size(); ++mi)
                        {
                            auto &mInst = instrs[mi];
                            if (!mInst || !std::holds_alternative<list>(mInst->data))
                                continue;
                            auto &ML = std::get<list>(mInst->data).elems;
                            if (ML.size() != 5)
                                continue;
                            if (!std::holds_alternative<symbol>(ML[0]->data) || std::get<symbol>(ML[0]->data).name != "member")
                                continue;
                            if (!std::holds_alternative<symbol>(ML[2]->data))
                                continue;
                            std::string structName = std::get<symbol>(ML[2]->data).name;
                            if (structName.rfind("__Tuple", 0) != 0)
                                continue;
                            if (!std::holds_alternative<symbol>(ML[4]->data) || std::get<symbol>(ML[4]->data).name != "_0")
                                continue; // start of potential cluster
                            // Parse arity from struct name
                            size_t tupleAr = 0;
                            try
                            {
                                tupleAr = (size_t)std::stoul(structName.substr(7));
                            }
                            catch (...)
                            {
                                continue;
                            }
                            if (tupleAr == 0)
                                continue;
                            if (!std::holds_alternative<symbol>(ML[3]->data))
                                continue;
                            std::string baseTup = std::get<symbol>(ML[3]->data).name; // %t
                            size_t cLen = 0;
                            size_t j = mi;
                            bool dense = true;
                            while (j < instrs.size())
                            {
                                auto &mn = instrs[j];
                                if (!mn || !std::holds_alternative<list>(mn->data))
                                    break;
                                auto &L2 = std::get<list>(mn->data).elems;
                                if (L2.size() != 5)
                                    break;
                                if (!std::holds_alternative<symbol>(L2[0]->data) || std::get<symbol>(L2[0]->data).name != "member")
                                    break;
                                if (!std::holds_alternative<symbol>(L2[2]->data) || std::get<symbol>(L2[2]->data).name != structName)
                                    break;
                                if (!std::holds_alternative<symbol>(L2[3]->data) || std::get<symbol>(L2[3]->data).name != baseTup)
                                    break;
                                if (!std::holds_alternative<symbol>(L2[4]->data))
                                    break;
                                std::string field = std::get<symbol>(L2[4]->data).name;
                                std::string want = std::string("_") + std::to_string(cLen);
                                if (field != want)
                                {
                                    dense = false;
                                    break;
                                }
                                ++cLen;
                                ++j;
                            }
                            if (cLen > 0 && cLen < tupleAr && dense)
                            {
                                // Arity mismatch: fewer bound fields than tuple arity.
                                emit_tuple_pattern_generic_error(moduleHead, "E1454", "tuple pattern arity mismatch expected" + std::to_string(tupleAr) + " got" + std::to_string(cLen));
                                mi += cLen - 1; // skip cluster
                                continue;
                            }
                            if (cLen == tupleAr && dense)
                            {
                                // Potential over-arity: look ahead for stray unreduced (tget ...) referencing same tuple with index >= tupleAr
                                size_t overMax = tupleAr; // one past last valid index seen
                                size_t look = mi + cLen;
                                bool foundExtra = false;
                                while (look < instrs.size())
                                {
                                    auto &n2 = instrs[look];
                                    if (!n2 || !std::holds_alternative<list>(n2->data))
                                        break;
                                    auto &L2 = std::get<list>(n2->data).elems;
                                    if (L2.size() == 5 && std::holds_alternative<symbol>(L2[0]->data) && std::get<symbol>(L2[0]->data).name == "tget")
                                    {
                                        if (std::holds_alternative<symbol>(L2[3]->data) && std::get<symbol>(L2[3]->data).name == baseTup && std::holds_alternative<int64_t>(L2[4]->data))
                                        {
                                            int64_t idx = std::get<int64_t>(L2[4]->data);
                                            if (idx >= (int64_t)tupleAr)
                                            {
                                                foundExtra = true;
                                                if ((size_t)(idx + 1) > overMax)
                                                    overMax = (size_t)(idx + 1);
                                                ++look;
                                                continue;
                                            }
                                        }
                                        break; // stop scan on first non-extra
                                    }
                                    if (foundExtra)
                                    {
                                        emit_tuple_pattern_generic_error(moduleHead, "E1454", "tuple pattern arity mismatch expected" + std::to_string(tupleAr) + " got" + std::to_string(overMax));
                                        mi = look - 1; // advance past extras
                                        continue;
                                    }
                                }
                            }
                        }
                    }
                    // First pass: infer field types per arity
                    std::unordered_map<size_t, std::vector<node_ptr>> inferred;
                    std::unordered_map<size_t, std::vector<bool>> complete;
                    for (size_t ar : macroCtx->tupleArities)
                    {
                        inferred[ar] = std::vector<node_ptr>(ar, nullptr);
                        complete[ar] = std::vector<bool>(ar, false);
                    }
                    auto try_infer_from_symbol = [&](const std::vector<node_ptr> &body, size_t uptoIdx, const std::string &sym) -> node_ptr
                    {
                        // Scan backwards (excluding instruction at uptoIdx) for defining const / as of %sym.
                        if (uptoIdx == 0)
                            return nullptr;
                        for (size_t i = uptoIdx; i-- > 0;)
                        { // i runs: uptoIdx-1 ... 0
                            auto &n = body[i];
                            if (!n || !std::holds_alternative<list>(n->data))
                                continue;
                            auto &L = std::get<list>(n->data).elems;
                            if (L.empty() || !std::holds_alternative<symbol>(L[0]->data))
                                continue;
                            const std::string op = std::get<symbol>(L[0]->data).name;
                            if (op == "const" && L.size() >= 4 && std::holds_alternative<symbol>(L[1]->data) && std::get<symbol>(L[1]->data).name == sym)
                            {
                                // (const %a <Ty> ...)
                                return L[2];
                            }
                            if (op == "as" && L.size() >= 4 && std::holds_alternative<symbol>(L[1]->data) && std::get<symbol>(L[1]->data).name == sym)
                            {
                                // (as %a <Ty> ...)
                                return L[2];
                            }
                        }
                        return nullptr;
                    };
                    // Iterate module nodes to find fn/rfn forms
                    for (auto &top : modList.elems)
                    {
                        if (!top || !std::holds_alternative<list>(top->data))
                            continue;
                        auto &fnL = std::get<list>(top->data).elems;
                        if (fnL.empty() || !std::holds_alternative<symbol>(fnL[0]->data))
                            continue;
                        std::string head = std::get<symbol>(fnL[0]->data).name;
                        if (head != "fn" && head != "rfn")
                            continue;
                        // locate :body vector
                        node_ptr bodyVec = nullptr;
                        for (size_t i = 1; i + 1 < fnL.size(); i += 2)
                        {
                            if (!std::holds_alternative<keyword>(fnL[i]->data))
                                break;
                            if (std::get<keyword>(fnL[i]->data).name == "body")
                            {
                                bodyVec = fnL[i + 1];
                                break;
                            }
                        }
                        if (!bodyVec || !std::holds_alternative<vector_t>(bodyVec->data))
                            continue;
                        auto &instrs = std::get<vector_t>(bodyVec->data).elems;
                        for (size_t idx = 0; idx < instrs.size(); ++idx)
                        {
                            auto &inst = instrs[idx];
                            if (!inst || !std::holds_alternative<list>(inst->data))
                                continue;
                            auto &L = std::get<list>(inst->data).elems;
                            if (L.size() != 4)
                                continue;
                            if (!std::holds_alternative<symbol>(L[0]->data))
                                continue;
                            if (std::get<symbol>(L[0]->data).name != "struct-lit")
                                continue;
                            if (!std::holds_alternative<symbol>(L[2]->data))
                                continue;
                            std::string sname = std::get<symbol>(L[2]->data).name;
                            if (sname.rfind("__Tuple", 0) != 0)
                                continue;
                            size_t arity = 0;
                            try
                            {
                                arity = (size_t)std::stoul(sname.substr(7));
                            }
                            catch (...)
                            {
                                continue;
                            }
                            if (!macroCtx->tupleArities.count(arity))
                                continue;
                            if (!std::holds_alternative<vector_t>(L[3]->data))
                                continue;
                            auto &fields = std::get<vector_t>(L[3]->data).elems;
                            // fields vector pattern: _0 %a _1 %b ... pairs
                            for (size_t fi = 0, fieldIndex = 0; fi + 1 < fields.size(); fi += 2, ++fieldIndex)
                            {
                                if (fieldIndex >= arity)
                                    break;
                                auto &valNode = fields[fi + 1];
                                if (!valNode || !std::holds_alternative<symbol>(valNode->data))
                                    continue;
                                std::string vsym = std::get<symbol>(valNode->data).name;
                                if (!inferred[arity][fieldIndex])
                                {
                                    auto tyNode = try_infer_from_symbol(instrs, idx, vsym);
                                    if (tyNode)
                                    {
                                        inferred[arity][fieldIndex] = tyNode;
                                        complete[arity][fieldIndex] = true;
                                    }
                                }
                            }
                        }
                    }
                    // Collect existing struct names
                    std::unordered_set<std::string> existing;
                    for (auto &n : modList.elems)
                    {
                        if (!n || !std::holds_alternative<list>(n->data))
                            continue;
                        auto &L = std::get<list>(n->data).elems;
                        if (L.empty())
                            continue;
                        if (std::holds_alternative<symbol>(L[0]->data) && std::get<symbol>(L[0]->data).name == "struct")
                        {
                            // scan for :name
                            for (size_t i = 1; i + 1 < L.size(); i += 2)
                            {
                                if (!std::holds_alternative<keyword>(L[i]->data))
                                    break;
                                if (std::get<keyword>(L[i]->data).name == "name" && std::holds_alternative<symbol>(L[i + 1]->data))
                                    existing.insert(std::get<symbol>(L[i + 1]->data).name);
                            }
                        }
                    }
                    for (size_t ar : macroCtx->tupleArities)
                    {
                        std::string sname = "__Tuple" + std::to_string(ar);
                        if (existing.count(sname))
                            continue;
                        // Determine if we have complete inferred types
                        bool allResolved = true;
                        if (inferred.count(ar))
                        {
                            for (size_t i = 0; i < ar; ++i)
                            {
                                if (!complete[ar][i] || !inferred[ar][i])
                                {
                                    allResolved = false;
                                    break;
                                }
                            }
                        }
                        else
                            allResolved = false;
                        // Build fields vector: [ (field :name _i :type <Ty>) ... ]
                        vector_t fieldsV;
                        for (size_t i = 0; i < ar; ++i)
                        {
                            node_ptr tyNode = (allResolved ? inferred[ar][i] : nullptr);
                            if (!tyNode)
                                tyNode = rl_make_sym("i32"); // fallback
                            list fld;
                            fld.elems = {rl_make_sym("field"), rl_make_kw("name"), rl_make_sym("_" + std::to_string(i)), rl_make_kw("type"), tyNode};
                            fieldsV.elems.push_back(std::make_shared<node>(node{fld, {}}));
                        }
                        list structL;
                        structL.elems.push_back(rl_make_sym("struct"));
                        structL.elems.push_back(rl_make_kw("name"));
                        structL.elems.push_back(rl_make_sym(sname));
                        structL.elems.push_back(rl_make_kw("fields"));
                        structL.elems.push_back(std::make_shared<node>(node{fieldsV, {}}));
                        modList.elems.push_back(std::make_shared<node>(node{structL, {}}));
                    }
                }
            }

            // Post-pass: remap uses of initializer-const symbols back to their variable symbols.
            // Rationale: frontends often synthesize a const (e.g., %__rl_c26 = 0) and then (assign %z %__rl_c26).
            // Later expressions should reference %z, not the one-time const symbol, so that slot-backed loads reflect updates.
            using edn::keyword;
            using edn::list;
            using edn::node_ptr;
            using edn::symbol;
            using edn::vector_t;

            std::function<void(vector_t &, std::unordered_map<std::string, std::string> &)> rewrite_seq;
            std::function<void(node_ptr &, std::unordered_map<std::string, std::string> &)> rewrite_node;

            auto replace_sym_if_mapped = [](node_ptr &n, const std::unordered_map<std::string, std::string> &env)
            {
                if (!n)
                    return;
                if (std::holds_alternative<symbol>(n->data))
                {
                    const auto &s = std::get<symbol>(n->data).name;
                    auto it = env.find(s);
                    if (it != env.end())
                    {
                        n->data = symbol{it->second};
                    }
                }
            };

            rewrite_node = [&](node_ptr &n, std::unordered_map<std::string, std::string> &env)
            {
                if (!n)
                    return;
                if (std::holds_alternative<vector_t>(n->data))
                {
                    auto &v = std::get<vector_t>(n->data);
                    // New sequential scope inherits env by value (copy) so sibling sequences don't affect each other
                    auto envCopy = env;
                    rewrite_seq(v, envCopy);
                    return;
                }
                if (!std::holds_alternative<list>(n->data))
                {
                    // Simple atoms: apply symbol replacement if mapped
                    replace_sym_if_mapped(n, env);
                    return;
                }
                auto &l = std::get<list>(n->data);
                if (l.elems.empty() || !std::holds_alternative<symbol>(l.elems[0]->data))
                {
                    // Recurse into children conservatively
                    for (auto &ch : l.elems)
                    {
                        rewrite_node(ch, env);
                    }
                    return;
                }
                std::string op = std::get<symbol>(l.elems[0]->data).name;
                // For structured ops that contain nested vectors (bodies), process those as sequences with a forked env
                auto process_nested_vec = [&](size_t idx) -> void
                {
                    if (idx < l.elems.size() && l.elems[idx] && std::holds_alternative<vector_t>(l.elems[idx]->data))
                    {
                        auto envCopy = env;
                        rewrite_seq(std::get<vector_t>(l.elems[idx]->data), envCopy);
                    }
                    return; // explicit to satisfy -Wreturn-type (defensive)
                };
                (void)process_nested_vec;

                // Update env from declarations/assignments before replacing later uses in the same sequence step
                if (op == "as" && l.elems.size() == 4)
                {
                    // (as %var <ty> %init)
                    if (std::holds_alternative<symbol>(l.elems[1]->data) && std::holds_alternative<symbol>(l.elems[3]->data))
                    {
                        std::string var = std::get<symbol>(l.elems[1]->data).name;
                        std::string init = std::get<symbol>(l.elems[3]->data).name;
                        env[init] = var;
                    }
                    // Do not rewrite operands inside this same node; only future uses should see the alias
                    return;
                }

                // Replace symbol operands (skip head op and keywords)
                for (size_t i = 1; i < l.elems.size(); ++i)
                {
                    if (l.elems[i] && std::holds_alternative<keyword>(l.elems[i]->data))
                    {
                        // Process the value after a keyword; if it is a vector body, handle sequentially
                        if (i + 1 < l.elems.size() && l.elems[i + 1])
                        {
                            // If body vector, process as sequence; otherwise recurse normally
                            if (std::holds_alternative<vector_t>(l.elems[i + 1]->data))
                            {
                                auto envCopy = env;
                                rewrite_seq(std::get<vector_t>(l.elems[i + 1]->data), envCopy);
                            }
                            else
                            {
                                rewrite_node(l.elems[i + 1], env);
                            }
                            ++i; // skip value just processed
                        }
                        continue;
                    }
                    // Recurse into nested lists/vectors or replace plain symbol
                    if (l.elems[i] && (std::holds_alternative<list>(l.elems[i]->data) || std::holds_alternative<vector_t>(l.elems[i]->data)))
                    {
                        rewrite_node(l.elems[i], env);
                    }
                    else
                    {
                        replace_sym_if_mapped(l.elems[i], env);
                    }
                }
            };

            rewrite_seq = [&](vector_t &seq, std::unordered_map<std::string, std::string> &env)
            {
                for (auto &inst : seq.elems)
                {
                    rewrite_node(inst, env);
                }
            };

            // Kick off rewriting at module top-level if it’s a list: visit function bodies and nested vectors via generic recursion above.
            if (expanded && std::holds_alternative<list>(expanded->data))
            {
                auto env = std::unordered_map<std::string, std::string>{};
                rewrite_node(expanded, env);
            }
            return expanded;
        }

    } // end expand_rustlite

} // namespace rustlite
