#pragma once
#include "edn.hpp"
#include <unordered_map>
#include <functional>
#include <optional>
#include <any>

namespace edn {

// Result sink concept: user can subclass / supply callbacks for produced IR nodes.
// This header supplies a generic Transformer handling two phases:
// 1. Macro expansion (symbol -> macro function) rewriting list forms.
// 2. Visiting expanded tree (symbol -> visitor) to build analysis/IR.
//
// Macros: signature std::optional<node_ptr>(const list& form)
//   Return std::nullopt if not applicable (allows arity-based conditional expansion).
//   Returned value is recursively expanded again (so macros can expand to macros).
// Visitors: signature void(const list& form, const symbol& head)
//   Invoked after full macro expansion on each list whose head symbol has a registered visitor.
// Default visitors: optional for unmatched lists and atomic values.

class Transformer {
public:
    using MacroFn = std::function<std::optional<node_ptr>(const list&)>; // may rewrite, returns replacement root (already node graph)
    using ListVisitorFn = std::function<void(node&, list&, const symbol& head)>; // analysis, can mutate metadata
    using FallbackListVisitorFn = std::function<void(node&, list&)>;
    using AtomVisitorFn = std::function<void(node&)>;

    // Register a macro associated to head symbol name.
    Transformer& add_macro(std::string name, MacroFn fn) {
        macros_[std::move(name)] = std::move(fn); return *this;
    }
    // Register a structural visitor.
    Transformer& add_visitor(std::string name, ListVisitorFn fn) {
        visitors_[std::move(name)] = std::move(fn); return *this;
    }
    // Fallbacks
    Transformer& on_unmatched_list(FallbackListVisitorFn fn){ unmatched_list_ = std::move(fn); return *this; }
    Transformer& on_atom(AtomVisitorFn fn){ atom_ = std::move(fn); return *this; }

    // Expand macros (returns a deep-copied expanded value separate from input)
    node_ptr expand(const node_ptr& n){ return expand_impl(n); }

    // Run expand then traverse with visitors (does not modify original)
    node_ptr expand_and_traverse(const node_ptr& n){ auto out = expand_impl(n); traverse(out); return out; }

    // Traverse pre-expanded value (no expansion during traversal)
    void traverse(const node_ptr& n){ traverse_impl(n); }

private:
    std::unordered_map<std::string, MacroFn> macros_;
    std::unordered_map<std::string, ListVisitorFn> visitors_;
    FallbackListVisitorFn unmatched_list_{};
    AtomVisitorFn atom_{};

    node_ptr clone_node(const node_ptr& n){
        auto out = std::make_shared<node>();
        out->metadata = n->metadata; // shallow copy meta
        // deep copy structure
        std::visit([&](auto&& arg){ using T = std::decay_t<decltype(arg)>; if constexpr(std::is_same_v<T,list>){ list l; for(auto& ch: arg.elems) l.elems.push_back(clone_node(ch)); out->data = std::move(l);} else if constexpr(std::is_same_v<T,vector_t>){ vector_t v; for(auto& ch: arg.elems) v.elems.push_back(clone_node(ch)); out->data = std::move(v);} else if constexpr(std::is_same_v<T,set>){ set s; for(auto& ch: arg.elems) s.elems.push_back(clone_node(ch)); out->data = std::move(s);} else if constexpr(std::is_same_v<T,map>){ map m; for(auto& kv: arg.entries) m.entries.emplace_back(clone_node(kv.first), clone_node(kv.second)); out->data = std::move(m);} else if constexpr(std::is_same_v<T,tagged_value>){ out->data = tagged_value{ arg.tag, clone_node(arg.inner) }; } else { out->data = arg; } }, n->data);
        return out;
    }

    node_ptr expand_impl(const node_ptr& n){
        if(!std::holds_alternative<list>(n->data)){
            // recurse into non-list collections
            return clone_and_rewrite_children(n);
        }
        // copy list for manipulation
        auto current = clone_node(n); // deep copy
        bool changed = true;
        while(changed){
            changed = false;
            auto& l = std::get<list>(current->data);
            if(!l.elems.empty() && std::holds_alternative<symbol>(l.elems[0]->data)){
                auto name = std::get<symbol>(l.elems[0]->data).name;
                auto it = macros_.find(name);
                if(it!=macros_.end()){
                    auto maybe = it->second(std::get<list>(current->data));
                    if(maybe){
                        auto replaced = expand_impl(*maybe); // expand inside result
                        if(std::holds_alternative<list>(replaced->data)){
                            current = replaced; changed = true; continue;
                        } else return replaced;
                    }
                }
            }
        }
        // expand children
        auto& l = std::get<list>(current->data);
        for(auto& ch : l.elems) ch = expand_impl(ch);
        return current;
    }

    node_ptr clone_and_rewrite_children(const node_ptr& n){
        // Only rewrite children collections
        if(std::holds_alternative<vector_t>(n->data)){
            auto copy = std::make_shared<node>(); copy->metadata = n->metadata; vector_t v; for(auto& c: std::get<vector_t>(n->data).elems) v.elems.push_back(expand_impl(c)); copy->data=std::move(v); return copy; }
        if(std::holds_alternative<set>(n->data)){
            auto copy = std::make_shared<node>(); copy->metadata=n->metadata; set s; for(auto& c: std::get<set>(n->data).elems) s.elems.push_back(expand_impl(c)); copy->data=std::move(s); return copy; }
        if(std::holds_alternative<map>(n->data)){
            auto copy = std::make_shared<node>(); copy->metadata=n->metadata; map m; for(auto& kv: std::get<map>(n->data).entries) m.entries.emplace_back(expand_impl(kv.first), expand_impl(kv.second)); copy->data=std::move(m); return copy; }
        if(std::holds_alternative<tagged_value>(n->data)){
            auto copy = std::make_shared<node>(); copy->metadata=n->metadata; auto& tv=std::get<tagged_value>(n->data); copy->data = tagged_value{ tv.tag, expand_impl(tv.inner) }; return copy; }
        return n; // atom
    }

    void traverse_impl(const node_ptr& n){
        if(std::holds_alternative<list>(n->data)){
            auto& l = std::get<list>(n->data);
            if(!l.elems.empty() && std::holds_alternative<symbol>(l.elems[0]->data)){
                auto head = std::get<symbol>(l.elems[0]->data);
                auto it = visitors_.find(head.name);
                if(it != visitors_.end()) it->second(*n, l, head); else if(unmatched_list_) unmatched_list_(*n, l);
            } else if(unmatched_list_) unmatched_list_(*n, l);
            for(auto& ch : l.elems) traverse_impl(ch);
            return;
        }
        if(std::holds_alternative<vector_t>(n->data)){
            for(auto& ch : std::get<vector_t>(n->data).elems) traverse_impl(ch); return; }
        if(std::holds_alternative<set>(n->data)){
            for(auto& ch : std::get<set>(n->data).elems) traverse_impl(ch); return; }
        if(std::holds_alternative<map>(n->data)){
            for(auto& kv : std::get<map>(n->data).entries){ traverse_impl(kv.first); traverse_impl(kv.second);} return; }
        if(std::holds_alternative<tagged_value>(n->data)){
            traverse_impl(std::get<tagged_value>(n->data).inner); return; }
        if(atom_) atom_(*n);
    }
};

// Helper to construct list form: (head :k v ...)
inline node_ptr build_keyword_call(const std::string& head_symbol, std::initializer_list<std::pair<std::string,node_ptr>> kvs){
    list l; l.elems.push_back(std::make_shared<node>( node{ symbol{ head_symbol }, {} }));
    for(auto& pr : kvs){ l.elems.push_back(std::make_shared<node>( node{ keyword{ pr.first }, {} })); l.elems.push_back(pr.second); }
    return std::make_shared<node>( node{ std::move(l), {} } );
}

} // namespace edn
