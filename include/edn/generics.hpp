// generics.hpp - Macro-like expander for generic functions (reader macro style)
#pragma once
#include "edn/edn.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <functional>
#include <variant>

namespace edn {

// Expand generic function declarations and calls into concrete specializations.
// Syntax (reader-macro layer; not part of the type checker):
// - Generic function def: (gfn :name "f" :generics [ T U ... ] :ret <type> :params [ (param <type> %x) ... ] :body [ ... ])
// - Generic call: (gcall %dst <ret-type> f :types [ <type-args>* ] %args...)
// The expander clones the gfn body per unique type argument vector and appends specialized (fn ...) into the module.

namespace detail_generics {
    inline node_ptr clone_node(const node_ptr& n){
        auto out = std::make_shared<node>();
        out->metadata = n->metadata;
        std::visit([&](auto&& arg){
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::is_same_v<T,list>){ list l; for(auto& ch: arg.elems) l.elems.push_back(clone_node(ch)); out->data = std::move(l); }
            else if constexpr(std::is_same_v<T,vector_t>){ vector_t v; for(auto& ch: arg.elems) v.elems.push_back(clone_node(ch)); out->data = std::move(v); }
            else if constexpr(std::is_same_v<T,set>){ set s; for(auto& ch: arg.elems) s.elems.push_back(clone_node(ch)); out->data = std::move(s); }
            else if constexpr(std::is_same_v<T,map>){ map m; for(auto& kv: arg.entries) m.entries.emplace_back(clone_node(kv.first), clone_node(kv.second)); out->data = std::move(m); }
            else if constexpr(std::is_same_v<T,tagged_value>){ out->data = tagged_value{ arg.tag, clone_node(arg.inner) }; }
            else { out->data = arg; }
        }, n->data);
        return out;
    }

    inline node_ptr make_sym(const std::string& s){ return std::make_shared<node>( node{ symbol{s}, {} } ); }
    inline node_ptr make_kw(const std::string& s){ return std::make_shared<node>( node{ keyword{s}, {} } ); }
    inline node_ptr make_str(const std::string& s){ return std::make_shared<node>( node{ std::string{s}, {} } ); }

    inline std::string sanitize(const std::string& in){
        std::string out; out.reserve(in.size());
        for(char c : in){
            if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.') out.push_back(c);
            else out.push_back('_');
        }
        return out;
    }

    inline std::string mangle_name(const std::string& base, const std::vector<node_ptr>& typeArgs){
        std::string m = base;
        m += "@";
        for(size_t i=0;i<typeArgs.size(); ++i){ if(i) m += "$"; m += sanitize(to_string(*typeArgs[i])); }
        return m;
    }

    // Substitute symbols matching type parameter names with provided type argument nodes.
    inline node_ptr subst_types(const node_ptr& n, const std::unordered_map<std::string,node_ptr>& subst){
        if(std::holds_alternative<symbol>(n->data)){
            const std::string& nm = std::get<symbol>(n->data).name;
            if(!nm.empty() && nm[0] == '%'){
                return n; // never touch SSA/var symbols
            }
            auto it = subst.find(nm);
            if(it != subst.end()) return clone_node(it->second);
            return n;
        }
        if(std::holds_alternative<list>(n->data)){
            auto out = std::make_shared<node>(); out->metadata = n->metadata; list l; l.elems.reserve(std::get<list>(n->data).elems.size());
            for(auto& ch : std::get<list>(n->data).elems) l.elems.push_back(subst_types(ch, subst));
            out->data = std::move(l); return out;
        }
        if(std::holds_alternative<vector_t>(n->data)){
            auto out = std::make_shared<node>(); out->metadata = n->metadata; vector_t v; v.elems.reserve(std::get<vector_t>(n->data).elems.size());
            for(auto& ch : std::get<vector_t>(n->data).elems) v.elems.push_back(subst_types(ch, subst));
            out->data = std::move(v); return out;
        }
        if(std::holds_alternative<set>(n->data)){
            auto out = std::make_shared<node>(); out->metadata = n->metadata; set s; s.elems.reserve(std::get<set>(n->data).elems.size());
            for(auto& ch : std::get<set>(n->data).elems) s.elems.push_back(subst_types(ch, subst));
            out->data = std::move(s); return out;
        }
        if(std::holds_alternative<map>(n->data)){
            auto out = std::make_shared<node>(); out->metadata = n->metadata; map m; m.entries.reserve(std::get<map>(n->data).entries.size());
            for(auto& kv : std::get<map>(n->data).entries) m.entries.emplace_back(subst_types(kv.first, subst), subst_types(kv.second, subst));
            out->data = std::move(m); return out;
        }
        if(std::holds_alternative<tagged_value>(n->data)){
            auto out = std::make_shared<node>(); out->metadata = n->metadata; auto& tv=std::get<tagged_value>(n->data);
            out->data = tagged_value{ tv.tag, subst_types(tv.inner, subst) }; return out;
        }
        return n;
    }
}

inline node_ptr expand_generics(const node_ptr& module_ast){
    using namespace detail_generics;
    if(!module_ast || !std::holds_alternative<list>(module_ast->data)) return module_ast;
    auto& top = std::get<list>(module_ast->data).elems;
    if(top.empty() || !std::holds_alternative<symbol>(top[0]->data) || std::get<symbol>(top[0]->data).name != "module") return module_ast;

    // Fast path: detect presence of gfn/gcall; if none, return original AST unchanged
    bool hasGen=false;
    std::function<void(const node_ptr&)> scan = [&](const node_ptr& n){ if(!n||hasGen) return; if(std::holds_alternative<list>(n->data)){ auto &l=std::get<list>(n->data).elems; if(!l.empty() && std::holds_alternative<symbol>(l[0]->data)){ auto h=std::get<symbol>(l[0]->data).name; if(h=="gfn"||h=="gcall"){ hasGen=true; return; } } for(auto &c:l) scan(c);} else if(std::holds_alternative<vector_t>(n->data)){ for(auto &c: std::get<vector_t>(n->data).elems) scan(c);} else if(std::holds_alternative<set>(n->data)){ for(auto &c: std::get<set>(n->data).elems) scan(c);} else if(std::holds_alternative<map>(n->data)){ for(auto &kv: std::get<map>(n->data).entries){ scan(kv.first); scan(kv.second);} } else if(std::holds_alternative<tagged_value>(n->data)){ scan(std::get<tagged_value>(n->data).inner);} };
    scan(module_ast);
    if(!hasGen) return module_ast;

    struct GenericFn { std::string name; std::vector<std::string> tparams; node_ptr node; };
    std::unordered_map<std::string, GenericFn> templates; // name -> template

    // Collect templates
    for(size_t i=1;i<top.size(); ++i){
        auto& n = top[i]; if(!n || !std::holds_alternative<list>(n->data)) continue;
        auto& l = std::get<list>(n->data).elems; if(l.empty() || !std::holds_alternative<symbol>(l[0]->data)) continue;
        if(std::get<symbol>(l[0]->data).name != "gfn") continue;
        std::string fname; std::vector<std::string> tparams;
        for(size_t j=1;j<l.size(); ++j){ if(!l[j] || !std::holds_alternative<keyword>(l[j]->data)) break; std::string kw=std::get<keyword>(l[j]->data).name; if(++j>=l.size()) break; auto val=l[j];
            if(kw=="name"){ if(std::holds_alternative<std::string>(val->data)) fname=std::get<std::string>(val->data); else if(std::holds_alternative<symbol>(val->data)) fname=std::get<symbol>(val->data).name; }
            else if(kw=="generics" && val && std::holds_alternative<vector_t>(val->data)){
                for(auto& tp : std::get<vector_t>(val->data).elems){ if(tp && std::holds_alternative<symbol>(tp->data)) tparams.push_back(std::get<symbol>(tp->data).name); else if(tp && std::holds_alternative<std::string>(tp->data)) tparams.push_back(std::get<std::string>(tp->data)); }
            }
        }
        if(!fname.empty()) templates[fname] = GenericFn{ fname, std::move(tparams), n };
    }

    // Build new module, preserving leading header keyword pairs exactly
    list newModule; newModule.elems.push_back(make_sym("module"));
    size_t iHead = 1; // copy leading :kw value pairs
    while(iHead+1 < top.size() && top[iHead] && std::holds_alternative<keyword>(top[iHead]->data)){
        newModule.elems.push_back(top[iHead]);
        newModule.elems.push_back(top[iHead+1]);
        iHead += 2;
    }

    // Helper to instantiate templates once per request, preserving exact type arg nodes
    struct InstKey { std::string name; std::vector<std::string> typeKeys; bool operator==(const InstKey& o) const { return name==o.name && typeKeys==o.typeKeys; } };
    struct InstKeyHash { size_t operator()(const InstKey& k) const noexcept { size_t h=std::hash<std::string>{}(k.name); for(auto& s: k.typeKeys) h ^= (std::hash<std::string>{}(s) + 0x9e3779b97f4a7c15ull + (h<<6) + (h>>2)); return h; } };
    std::unordered_set<InstKey,InstKeyHash> emitted;
    // Collected instances grouped by template name, so we can emit next to original gfn
    std::unordered_map<std::string, std::vector<node_ptr>> generated;

    // Collector: scan AST, find gcall sites, and generate needed instances (but don't rewrite tree)
    std::function<void(const node_ptr&)> collect = [&](const node_ptr& n){
        if(!n) return;
        if(std::holds_alternative<list>(n->data)){
            auto &l = std::get<list>(n->data).elems;
            if(!l.empty() && std::holds_alternative<symbol>(l[0]->data)){
                std::string head = std::get<symbol>(l[0]->data).name;
                if(head == "gcall"){
                    if(l.size()<5) return;
                    std::string callee = (l.size()>=4 && std::holds_alternative<symbol>(l[3]->data)) ? std::get<symbol>(l[3]->data).name : std::string();
                    std::vector<node_ptr> typeArgs; for(size_t i=4;i<l.size(); ++i){ if(!l[i] || !std::holds_alternative<keyword>(l[i]->data)) break; std::string kw=std::get<keyword>(l[i]->data).name; if(++i>=l.size()) break; auto val=l[i]; if(kw=="types" && val && std::holds_alternative<vector_t>(val->data)){ for(auto& ta : std::get<vector_t>(val->data).elems) typeArgs.push_back(ta); } else break; }
                    if(callee.empty() || !templates.count(callee) || typeArgs.empty()) return;
                    std::vector<std::string> typeKeys; typeKeys.reserve(typeArgs.size()); for(auto& ta : typeArgs) typeKeys.push_back(sanitize(to_string(*ta)));
                    InstKey key{ callee, typeKeys }; if(emitted.count(key)) return; emitted.insert(key);

                    // Instantiate template -> (fn ...)
                    std::unordered_map<std::string,node_ptr> subst; const auto& tps = templates[callee].tparams; for(size_t i=0;i<std::min(tps.size(), typeArgs.size()); ++i) subst[tps[i]] = typeArgs[i];
                    auto fnNode = clone_node(templates[callee].node);
                    std::string mangled = mangle_name(callee, typeArgs);
                    if(std::holds_alternative<list>(fnNode->data)){
                        auto &fl = std::get<list>(fnNode->data).elems; list repl; repl.elems.push_back(make_sym("fn"));
                        for(size_t j=1;j<fl.size(); ++j){ if(!fl[j] || !std::holds_alternative<keyword>(fl[j]->data)) break; std::string kw=std::get<keyword>(fl[j]->data).name; if(++j>=fl.size()) break; auto val=fl[j]; if(kw=="generics") continue; if(kw=="name"){ repl.elems.push_back(make_kw("name")); repl.elems.push_back(make_str(mangled)); }
                            else { repl.elems.push_back(make_kw(kw)); repl.elems.push_back(val); }
                        }
                        fnNode = std::make_shared<node>( node{ repl, fnNode->metadata } );
                    }
                    auto specialized = subst_types(fnNode, subst);
                    generated[callee].push_back(specialized);
                }
            }
            for(auto &ch : l) collect(ch);
            return;
        }
        if(std::holds_alternative<vector_t>(n->data)){
            for(auto &ch : std::get<vector_t>(n->data).elems) collect(ch);
            return;
        }
        if(std::holds_alternative<set>(n->data)){
            for(auto &ch : std::get<set>(n->data).elems) collect(ch);
            return;
        }
        if(std::holds_alternative<map>(n->data)){
            for(auto &kv : std::get<map>(n->data).entries){ collect(kv.first); collect(kv.second); }
            return;
        }
        if(std::holds_alternative<tagged_value>(n->data)){
            collect(std::get<tagged_value>(n->data).inner);
            return;
        }
    };
    // First pass to collect what instances we need
    collect(module_ast);

    // Rewriter: deep-copy node, rewriting (gcall ...) -> (call ... mangled) and recording required instances
    std::function<node_ptr(const node_ptr&)> rewrite = [&](const node_ptr& n)->node_ptr{
        if(!n) return n;
        if(std::holds_alternative<list>(n->data)){
            auto &l = std::get<list>(n->data).elems;
            if(!l.empty() && std::holds_alternative<symbol>(l[0]->data)){
                std::string head = std::get<symbol>(l[0]->data).name;
                if(head == "gcall"){
                    // shape: (gcall %dst <ret-type> callee :types [ <type-args>* ] %args...)
                    if(l.size()<5) return n; // keep as-is if malformed
                    std::string callee = (l.size()>=4 && std::holds_alternative<symbol>(l[3]->data)) ? std::get<symbol>(l[3]->data).name : std::string();
                    std::vector<node_ptr> typeArgs; size_t afterTypesIdx = 4;
                    for(size_t i=4;i<l.size(); ++i){ if(!l[i] || !std::holds_alternative<keyword>(l[i]->data)) { afterTypesIdx=i; break; } std::string kw=std::get<keyword>(l[i]->data).name; if(++i>=l.size()) { afterTypesIdx=i; break; } auto val=l[i]; if(kw=="types" && val && std::holds_alternative<vector_t>(val->data)){ for(auto& ta : std::get<vector_t>(val->data).elems) typeArgs.push_back(ta); afterTypesIdx = i+1; } else { afterTypesIdx=i+1; break; } }
                    if(callee.empty()) return n;
                    // Build mangled name
                    std::string mangled = mangle_name(callee, typeArgs);
                    // No instantiation here; was already collected
                    // Now rewrite the call form
                    list repl; repl.elems.reserve(l.size());
                    repl.elems.push_back(make_sym("call"));
                    // copy %dst, <ret-type>
                    if(l.size()>1) repl.elems.push_back(l[1]);
                    if(l.size()>2) repl.elems.push_back(l[2]);
                    // callee symbol -> mangled
                    repl.elems.push_back(make_sym(mangled));
                    // copy args after :types section
                    for(size_t ai=afterTypesIdx; ai<l.size(); ++ai){ repl.elems.push_back(l[ai]); }
                    return std::make_shared<node>( node{ repl, n->metadata } );
                }
            }
            // generic list: rewrite children
            list out; out.elems.reserve(l.size());
            for(auto &ch : l) out.elems.push_back(rewrite(ch));
            return std::make_shared<node>( node{ out, n->metadata } );
        }
        if(std::holds_alternative<vector_t>(n->data)){
            vector_t v; v.elems.reserve(std::get<vector_t>(n->data).elems.size()); for(auto &ch : std::get<vector_t>(n->data).elems) v.elems.push_back(rewrite(ch));
            return std::make_shared<node>( node{ v, n->metadata } );
        }
        if(std::holds_alternative<set>(n->data)){
            set s; s.elems.reserve(std::get<set>(n->data).elems.size()); for(auto &ch : std::get<set>(n->data).elems) s.elems.push_back(rewrite(ch));
            return std::make_shared<node>( node{ s, n->metadata } );
        }
        if(std::holds_alternative<map>(n->data)){
            map m; m.entries.reserve(std::get<map>(n->data).entries.size()); for(auto &kv : std::get<map>(n->data).entries) m.entries.emplace_back(rewrite(kv.first), rewrite(kv.second));
            return std::make_shared<node>( node{ m, n->metadata } );
        }
        if(std::holds_alternative<tagged_value>(n->data)){
            auto &tv = std::get<tagged_value>(n->data);
            return std::make_shared<node>( node{ tagged_value{ tv.tag, rewrite(tv.inner) }, n->metadata } );
        }
        return n;
    };

    // Build new module body: emit specializations at gfn position, and other forms rewritten
    for(size_t i=iHead; i<top.size(); ++i){
        auto &n = top[i];
        bool isGfn=false; std::string gname;
        if(n && std::holds_alternative<list>(n->data)){
            auto &l = std::get<list>(n->data).elems;
            if(!l.empty() && std::holds_alternative<symbol>(l[0]->data) && std::get<symbol>(l[0]->data).name=="gfn"){
                isGfn = true;
                // extract name
                for(size_t j=1;j<l.size(); ++j){ if(!l[j] || !std::holds_alternative<keyword>(l[j]->data)) break; std::string kw=std::get<keyword>(l[j]->data).name; if(++j>=l.size()) break; auto val=l[j]; if(kw=="name"){ if(std::holds_alternative<std::string>(val->data)) gname=std::get<std::string>(val->data); else if(std::holds_alternative<symbol>(val->data)) gname=std::get<symbol>(val->data).name; }
                }
            }
        }
        if(isGfn){
            // Emit all instances for this gfn (if any)
            auto it = generated.find(gname);
            if(it != generated.end()){
                for(auto &inst : it->second) newModule.elems.push_back(inst);
            }
            continue; // skip original gfn
        }
        newModule.elems.push_back(rewrite(n));
    }

    return std::make_shared<node>( node{ newModule, module_ast->metadata } );
}

} // namespace edn
