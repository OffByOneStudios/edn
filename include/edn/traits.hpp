// traits.hpp - Minimal reader-macro expander for Traits/VTables
#pragma once
#include "edn/edn.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace edn {

namespace detail_traits {
    inline node_ptr clone_node(const node_ptr& n){
        if(!n) return nullptr;
        auto out = std::make_shared<node>(); out->metadata = n->metadata;
        std::visit([&](auto&& arg){
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::is_same_v<T,list>){ list l; l.elems.reserve(arg.elems.size()); for(auto &ch: arg.elems) l.elems.push_back(clone_node(ch)); out->data = std::move(l); }
            else if constexpr(std::is_same_v<T,vector_t>){ vector_t v; v.elems.reserve(arg.elems.size()); for(auto &ch: arg.elems) v.elems.push_back(clone_node(ch)); out->data = std::move(v); }
            else if constexpr(std::is_same_v<T,set>){ set s; s.elems.reserve(arg.elems.size()); for(auto &ch: arg.elems) s.elems.push_back(clone_node(ch)); out->data = std::move(s); }
            else if constexpr(std::is_same_v<T,map>){ map m; m.entries.reserve(arg.entries.size()); for(auto &kv: arg.entries) m.entries.emplace_back(clone_node(kv.first), clone_node(kv.second)); out->data = std::move(m); }
            else if constexpr(std::is_same_v<T,tagged_value>){ out->data = tagged_value{ arg.tag, clone_node(arg.inner) }; }
            else { out->data = arg; }
        }, n->data);
        return out;
    }
    inline node_ptr make_sym(const std::string& s){ return std::make_shared<node>( node{ symbol{s}, {} } ); }
    inline node_ptr make_kw(const std::string& s){ return std::make_shared<node>( node{ keyword{s}, {} } ); }
    inline node_ptr make_str(const std::string& s){ return std::make_shared<node>( node{ std::string{s}, {} } ); }
}

// Expand top-level (trait ...) declarations into concrete (struct ...) vtable types.
// Syntax:
// (trait :name Show :methods [ (method :name print :type (ptr (fn-type :params [ (ptr i32) ] :ret i32))) ... ])
// -> (struct :name ShowVT :fields [ (field :name print :type (ptr (fn-type ...))) ... ])
inline node_ptr expand_traits(const node_ptr& module_ast){
    using namespace detail_traits;
    if(!module_ast || !std::holds_alternative<list>(module_ast->data)) return module_ast;
    auto &top = std::get<list>(module_ast->data).elems; if(top.empty()) return module_ast;
    if(!std::holds_alternative<symbol>(top[0]->data) || std::get<symbol>(top[0]->data).name != "module") return module_ast;

    // Collect trait method type info first (name -> methodName -> typeNode)
    std::unordered_map<std::string, std::unordered_map<std::string, node_ptr>> traitMethods;
    size_t iHeader = 1; while(iHeader+1<top.size() && top[iHeader] && std::holds_alternative<keyword>(top[iHeader]->data)) iHeader += 2;
    for(size_t j=iHeader; j<top.size(); ++j){ auto &n = top[j]; if(!n || !std::holds_alternative<list>(n->data)) continue; auto &l = std::get<list>(n->data).elems; if(l.empty()||!std::holds_alternative<symbol>(l[0]->data) || std::get<symbol>(l[0]->data).name!="trait") continue; std::string tname; node_ptr methodsNode;
        for(size_t k=1;k<l.size(); ++k){ if(!l[k]||!std::holds_alternative<keyword>(l[k]->data)) break; std::string kw=std::get<keyword>(l[k]->data).name; if(++k>=l.size()) break; auto v=l[k]; if(kw=="name"){ if(std::holds_alternative<std::string>(v->data)) tname=std::get<std::string>(v->data); else if(std::holds_alternative<symbol>(v->data)) tname=std::get<symbol>(v->data).name; } else if(kw=="methods") methodsNode=v; }
        if(tname.empty() || !methodsNode || !std::holds_alternative<vector_t>(methodsNode->data)) continue; auto &vec = std::get<vector_t>(methodsNode->data).elems; for(auto &mn : vec){ if(!mn||!std::holds_alternative<list>(mn->data)) continue; auto &ml=std::get<list>(mn->data).elems; if(ml.empty()||!std::holds_alternative<symbol>(ml[0]->data) || std::get<symbol>(ml[0]->data).name!="method") continue; std::string mname; node_ptr mtype; for(size_t q=1;q<ml.size(); ++q){ if(!ml[q]||!std::holds_alternative<keyword>(ml[q]->data)) break; std::string kw=std::get<keyword>(ml[q]->data).name; if(++q>=ml.size()) break; auto v=ml[q]; if(kw=="name"){ if(std::holds_alternative<std::string>(v->data)) mname=std::get<std::string>(v->data); else if(std::holds_alternative<symbol>(v->data)) mname=std::get<symbol>(v->data).name; } else if(kw=="type") mtype=v; }
            if(!mname.empty() && mtype) traitMethods[tname][mname]=mtype; }
    }

    // Helpers to create type nodes
    auto make_ptr_to = [&](const std::string& typeSym){ list l; l.elems.push_back(make_sym("ptr")); l.elems.push_back(make_sym(typeSym)); return std::make_shared<node>( node{ l, {} } ); };

    // Build new module preserving header
    list newMod; newMod.elems.push_back(make_sym("module"));
    size_t i = 1; while(i+1<top.size() && top[i] && std::holds_alternative<keyword>(top[i]->data)){
        newMod.elems.push_back(top[i]); newMod.elems.push_back(top[i+1]); i += 2;
    }

    // Rewriter for fn bodies to expand trait-related forms
    size_t gensymCounter = 0;
    auto gensym = [&](const std::string& base){ return "%" + base + "$t" + std::to_string(++gensymCounter); };
    auto expand_body_vec = [&](const vector_t& inVec, const std::string& currentTraitCtx)->vector_t{
        vector_t out; out.elems.reserve(inVec.elems.size()); (void)currentTraitCtx;
        for(auto &elem : inVec.elems){ if(!elem || !std::holds_alternative<list>(elem->data)){ out.elems.push_back(elem); continue; }
            auto &l = std::get<list>(elem->data).elems; if(l.empty()||!std::holds_alternative<symbol>(l[0]->data)){ out.elems.push_back(elem); continue; }
            std::string op = std::get<symbol>(l[0]->data).name;
            if(op=="make-trait-obj" && l.size()>=5){ // (make-trait-obj %dst Trait %data %vt)
                std::string trait = std::holds_alternative<symbol>(l[2]->data)? std::get<symbol>(l[2]->data).name : (std::holds_alternative<std::string>(l[2]->data)? std::get<std::string>(l[2]->data):"");
                if(!trait.empty()){
                    // Ensure data is i8* by inserting a bitcast to (ptr i8)
                    auto dataI8 = gensym("data.i8");
                    list bc; bc.elems = { make_sym("bitcast"), make_sym(dataI8), make_ptr_to("i8"), l[3] };
                    out.elems.push_back(std::make_shared<node>( node{ bc, {} } ));

                    list sl; sl.elems.push_back(make_sym("struct-lit")); sl.elems.push_back(l[1]); // %dst
                    sl.elems.push_back(make_sym(trait+"Obj"));
                    vector_t fields; fields.elems.push_back(make_sym("data")); fields.elems.push_back(make_sym(dataI8)); fields.elems.push_back(make_sym("vtable")); fields.elems.push_back(l[4]);
                    sl.elems.push_back(std::make_shared<node>( node{ fields, {} } ));
                    out.elems.push_back(std::make_shared<node>( node{ sl, elem->metadata } ));
                    continue;
                }
            }
            if(op=="trait-call" && l.size()>=6){ // (trait-call %dst <ret> Trait %obj method %args...)
                std::string trait = std::holds_alternative<symbol>(l[3]->data)? std::get<symbol>(l[3]->data).name : (std::holds_alternative<std::string>(l[3]->data)? std::get<std::string>(l[3]->data):"");
                std::string method = std::holds_alternative<symbol>(l[5]->data)? std::get<symbol>(l[5]->data).name : (std::holds_alternative<std::string>(l[5]->data)? std::get<std::string>(l[5]->data):"");
                if(!trait.empty() && !method.empty()){
                    // names
                    auto vtabAddr = gensym("vt.addr"); auto vtabPtr = gensym("vt.ptr"); auto fnAddr = gensym("fn.addr"); auto fnPtr = gensym("fn.ptr"); auto dataAddr = gensym("data.addr"); auto ctx = gensym("ctx");
                    // 1) (member-addr %vtabAddr TraitObj %obj vtable)
                    list l1; l1.elems = { make_sym("member-addr"), make_sym(vtabAddr), make_sym(trait+"Obj"), l[4], make_sym("vtable") };
                    // 2) (load %vtabPtr (ptr TraitVT) %vtabAddr)
                    list l2; l2.elems = { make_sym("load"), make_sym(vtabPtr), make_ptr_to(trait+"VT"), make_sym(vtabAddr) };
                    // 3) (member-addr %fnAddr TraitVT %vtabPtr method)
                    list l3; l3.elems = { make_sym("member-addr"), make_sym(fnAddr), make_sym(trait+"VT"), make_sym(vtabPtr), make_sym(method) };
                    // 4) (load %fnPtr <method-type> %fnAddr)
                    node_ptr mty=nullptr; if(auto tit = traitMethods.find(trait); tit!=traitMethods.end()){ auto &mm=tit->second; if(auto mit=mm.find(method); mit!=mm.end()) mty = clone_node(mit->second); }
                    if(!mty) { // if unknown, leave as-is to let TC complain
                        out.elems.push_back(elem); continue; }
                    list l4; l4.elems = { make_sym("load"), make_sym(fnPtr), mty, make_sym(fnAddr) };
                    // 5) (member-addr %dataAddr TraitObj %obj data)
                    list l5; l5.elems = { make_sym("member-addr"), make_sym(dataAddr), make_sym(trait+"Obj"), l[4], make_sym("data") };
                    // 6) (load %ctx (ptr i8) %dataAddr)
                    list l6; l6.elems = { make_sym("load"), make_sym(ctx), make_ptr_to("i8"), make_sym(dataAddr) };
                    // 7) (call-indirect %dst <ret> %fnPtr %ctx <args...>)
                    list l7; l7.elems = { make_sym("call-indirect"), l[1], l[2], make_sym(fnPtr), make_sym(ctx) };
                    for(size_t ai=6; ai<l.size(); ++ai) l7.elems.push_back(l[ai]);
                    out.elems.push_back(std::make_shared<node>( node{ l1, {} } ));
                    out.elems.push_back(std::make_shared<node>( node{ l2, {} } ));
                    out.elems.push_back(std::make_shared<node>( node{ l3, {} } ));
                    out.elems.push_back(std::make_shared<node>( node{ l4, {} } ));
                    out.elems.push_back(std::make_shared<node>( node{ l5, {} } ));
                    out.elems.push_back(std::make_shared<node>( node{ l6, {} } ));
                    out.elems.push_back(std::make_shared<node>( node{ l7, elem->metadata } ));
                    continue;
                }
            }
            out.elems.push_back(elem);
        }
        return out;
    };

    // Rewrite remaining top-level forms: trait -> struct(s); transform fn bodies for macros
    for(size_t j=i; j<top.size(); ++j){ auto &n = top[j];
        bool isTrait=false; std::string tname; node_ptr methodsNode;
        if(n && std::holds_alternative<list>(n->data)){
            auto &l = std::get<list>(n->data).elems;
            if(!l.empty() && std::holds_alternative<symbol>(l[0]->data) && std::get<symbol>(l[0]->data).name=="trait"){
                isTrait = true;
                for(size_t k=1;k<l.size(); ++k){ if(!l[k]||!std::holds_alternative<keyword>(l[k]->data)) break; std::string kw=std::get<keyword>(l[k]->data).name; if(++k>=l.size()) break; auto v=l[k]; if(kw=="name"){ if(std::holds_alternative<std::string>(v->data)) tname=std::get<std::string>(v->data); else if(std::holds_alternative<symbol>(v->data)) tname=std::get<symbol>(v->data).name; }
                    else if(kw=="methods") methodsNode=v; }
            }
        }
        if(isTrait){
            if(tname.empty() || !methodsNode || !std::holds_alternative<vector_t>(methodsNode->data)){
                newMod.elems.push_back(n); continue; }
            // Build VT struct
            list structList; structList.elems.push_back(make_sym("struct"));
            structList.elems.push_back(make_kw("name")); structList.elems.push_back(make_str(tname+"VT"));
            structList.elems.push_back(make_kw("fields")); vector_t fieldsV;
            for(auto &mn : std::get<vector_t>(methodsNode->data).elems){ if(!mn||!std::holds_alternative<list>(mn->data)) continue; auto &ml=std::get<list>(mn->data).elems; if(ml.empty()||!std::holds_alternative<symbol>(ml[0]->data) || std::get<symbol>(ml[0]->data).name!="method") continue; std::string mname; node_ptr mtype;
                for(size_t q=1;q<ml.size(); ++q){ if(!ml[q]||!std::holds_alternative<keyword>(ml[q]->data)) break; std::string kw=std::get<keyword>(ml[q]->data).name; if(++q>=ml.size()) break; auto v=ml[q]; if(kw=="name"){ if(std::holds_alternative<std::string>(v->data)) mname=std::get<std::string>(v->data); else if(std::holds_alternative<symbol>(v->data)) mname=std::get<symbol>(v->data).name; } else if(kw=="type") mtype=v; }
                if(mname.empty() || !mtype) continue; list field; field.elems.push_back(make_sym("field")); field.elems.push_back(make_kw("name")); field.elems.push_back(make_sym(mname)); field.elems.push_back(make_kw("type")); field.elems.push_back(clone_node(mtype)); fieldsV.elems.push_back(std::make_shared<node>( node{ field, mn->metadata } )); }
            structList.elems.push_back(std::make_shared<node>( node{ fieldsV, methodsNode->metadata } ));
            newMod.elems.push_back(std::make_shared<node>( node{ structList, n->metadata } ));
            // Build Obj struct: { data: (ptr i8), vtable: (ptr TraitVT) }
            list objList; objList.elems.push_back(make_sym("struct")); objList.elems.push_back(make_kw("name")); objList.elems.push_back(make_str(tname+"Obj")); objList.elems.push_back(make_kw("fields")); vector_t objFields;
            { list f1; f1.elems = { make_sym("field"), make_kw("name"), make_sym("data"), make_kw("type"), make_ptr_to("i8") }; objFields.elems.push_back(std::make_shared<node>( node{ f1, {} } )); }
            { list f2; f2.elems = { make_sym("field"), make_kw("name"), make_sym("vtable"), make_kw("type"), make_ptr_to(tname+"VT") }; objFields.elems.push_back(std::make_shared<node>( node{ f2, {} } )); }
            objList.elems.push_back(std::make_shared<node>( node{ objFields, {} } ));
            newMod.elems.push_back(std::make_shared<node>( node{ objList, n->metadata } ));
            continue;
        }
        // Transform fn bodies
        if(n && std::holds_alternative<list>(n->data)){
            auto l = std::get<list>(n->data); if(!l.elems.empty() && std::holds_alternative<symbol>(l.elems[0]->data) && std::get<symbol>(l.elems[0]->data).name=="fn"){
                // Find :body and rewrite its vector
                for(size_t k=1; k<l.elems.size(); ++k){ if(!l.elems[k] || !std::holds_alternative<keyword>(l.elems[k]->data)) break; std::string kw=std::get<keyword>(l.elems[k]->data).name; if(++k>=l.elems.size()) break; auto v=l.elems[k]; if(kw=="body" && v && std::holds_alternative<vector_t>(v->data)){ auto nv = expand_body_vec(std::get<vector_t>(v->data), ""); l.elems[k] = std::make_shared<node>( node{ nv, v->metadata } ); } }
                newMod.elems.push_back(std::make_shared<node>( node{ l, n->metadata } ));
                continue;
            }
        }
        newMod.elems.push_back(n);
    }
    return std::make_shared<node>( node{ newMod, module_ast->metadata } );
}

} // namespace edn
