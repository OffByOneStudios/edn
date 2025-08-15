// Clean implementation for type checker including blocks, assign, control flow
#pragma once
#include "type_check.hpp"
#include <algorithm>

namespace edn {

inline void TypeChecker::error(TypeCheckResult& r, const node& n, std::string msg){
    ErrorReporter rep{&r.errors,&r.warnings};
    rep.emit_error(rep.make_error("EGEN", std::move(msg), "", line(n), col(n)));
}
inline void TypeChecker::warn(TypeCheckResult& r, const node& n, std::string msg){
    ErrorReporter rep{&r.errors,&r.warnings};
    rep.emit_warning(rep.make_warning("WGEN", std::move(msg), "", line(n), col(n)));
}
inline void TypeChecker::error_code(TypeCheckResult& r, const node& n, std::string code, std::string msg, std::string hint){
    ErrorReporter rep{&r.errors,&r.warnings};
    rep.emit_error(rep.make_error(std::move(code), std::move(msg), std::move(hint), line(n), col(n)));
}
inline void TypeChecker::reset(){ structs_.clear(); unions_.clear(); functions_.clear(); globals_.clear(); var_types_.clear(); }
inline void TypeChecker::collect_typedefs(TypeCheckResult& r, const std::vector<node_ptr>& elems){
    for(size_t i=1;i<elems.size(); ++i){
        auto &n = elems[i];
        if(!n||!std::holds_alternative<list>(n->data)) continue;
        auto &l = std::get<list>(n->data).elems; if(l.empty()||!std::holds_alternative<symbol>(l[0]->data)) continue;
        if(std::get<symbol>(l[0]->data).name!="typedef") continue;
        std::string name; node_ptr typeNode; bool haveName=false, haveType=false;
        for(size_t j=1;j<l.size(); ++j){ if(!l[j]||!std::holds_alternative<keyword>(l[j]->data)) break; std::string kw=std::get<keyword>(l[j]->data).name; if(++j>=l.size()) break; auto val=l[j];
            if(kw=="name"){ if(std::holds_alternative<symbol>(val->data)) { name=std::get<symbol>(val->data).name; haveName=true; } else if(std::holds_alternative<std::string>(val->data)){ name=std::get<std::string>(val->data); haveName=true; } }
            else if(kw=="type"){ typeNode=val; haveType=true; }
        }
        if(!haveName){ error_code(r,*n,"E1330","typedef missing :name","expected (typedef :name Alias :type <type>)"); r.success=false; continue; }
        if(!haveType){ error_code(r,*n,"E1331","typedef missing :type","add :type <type-form>"); r.success=false; continue; }
        if(typedefs_.count(name) || structs_.count(name)){ error_code(r,*n,"E1332","typedef redefinition","choose unique alias name"); r.success=false; continue; }
        TypeId underlying = 0; try { underlying = ctx_.parse_type(typeNode); } catch(const parse_error&){ error_code(r,*n,"E1333","typedef type form invalid","fix underlying type form"); r.success=false; continue; }
        if(!underlying){ error_code(r,*n,"E1333","typedef type form invalid","fix underlying type form"); r.success=false; continue; }
        typedefs_[name]=underlying; // success
    }
}
inline void TypeChecker::collect_enums(TypeCheckResult& r, const std::vector<node_ptr>& elems){
    for(size_t i=1;i<elems.size(); ++i){ auto &n=elems[i]; if(!n||!std::holds_alternative<list>(n->data)) continue; auto &l=std::get<list>(n->data).elems; if(l.empty()||!std::holds_alternative<symbol>(l[0]->data)) continue; if(std::get<symbol>(l[0]->data).name!="enum") continue; std::string name; node_ptr underlyingNode; node_ptr valuesNode; bool haveName=false, haveUnderlying=false, haveValues=false; for(size_t j=1;j<l.size(); ++j){ if(!l[j]||!std::holds_alternative<keyword>(l[j]->data)) break; std::string kw=std::get<keyword>(l[j]->data).name; if(++j>=l.size()) break; auto val=l[j]; if(kw=="name"){ if(std::holds_alternative<symbol>(val->data)) { name=std::get<symbol>(val->data).name; haveName=true; } else if(std::holds_alternative<std::string>(val->data)){ name=std::get<std::string>(val->data); haveName=true; } } else if(kw=="underlying"){ underlyingNode=val; haveUnderlying=true; } else if(kw=="values"){ valuesNode=val; haveValues=true; } }
        if(!haveName){ error_code(r,*n,"E1340","enum missing :name","provide (enum :name E ...)" ); r.success=false; continue; }
        if(enums_.count(name) || structs_.count(name) || typedefs_.count(name)){ error_code(r,*n,"E1347","enum redefinition","choose unique enum name"); r.success=false; continue; }
        if(!haveUnderlying){ error_code(r,*n,"E1341","enum missing :underlying","add :underlying <int-type>"); r.success=false; continue; }
        TypeId underlying=0; try { underlying = ctx_.parse_type(underlyingNode); } catch(const parse_error&){ error_code(r,*n,"E1342","enum underlying not integer","use integer base type"); r.success=false; }
        if(underlying){ const Type& UT=ctx_.at(underlying); if(!(UT.kind==Type::Kind::Base && is_integer_base(UT.base))){ error_code(r,*n,"E1342","enum underlying not integer","choose integer base type"); r.success=false; underlying=0; } }
        if(!haveValues || !valuesNode || !std::holds_alternative<vector_t>(valuesNode->data)){ error_code(r,*n,"E1343","enum missing :values","add :values [ (eval ...) ]"); r.success=false; continue; }
        EnumInfo info; info.name=name; info.underlying=underlying?underlying:ctx_.get_base(BaseType::I32);
        std::unordered_set<std::string> localNames;
        for(auto &ev : std::get<vector_t>(valuesNode->data).elems){ if(!ev||!std::holds_alternative<list>(ev->data)){ error_code(r,*n,"E1344","enum value entry malformed","use (eval :name X :value <int>)"); r.success=false; continue; } auto &vl=std::get<list>(ev->data).elems; if(vl.empty()||!std::holds_alternative<symbol>(vl[0]->data) || std::get<symbol>(vl[0]->data).name!="eval"){ error_code(r,*ev,"E1344","enum value entry malformed","starts with eval"); r.success=false; continue; } std::string cname; bool haveC=false, haveVal=false; int64_t cval=0; for(size_t k=1;k<vl.size(); ++k){ if(!vl[k]||!std::holds_alternative<keyword>(vl[k]->data)) break; std::string kw=std::get<keyword>(vl[k]->data).name; if(++k>=vl.size()) break; auto v=vl[k]; if(kw=="name"){ if(std::holds_alternative<symbol>(v->data)) { cname=std::get<symbol>(v->data).name; haveC=true; } else if(std::holds_alternative<std::string>(v->data)){ cname=std::get<std::string>(v->data); haveC=true; } } else if(kw=="value"){ if(std::holds_alternative<int64_t>(v->data)){ cval=std::get<int64_t>(v->data); haveVal=true; } else { error_code(r,*v,"E1346","enum constant value not int","use integer literal"); r.success=false; } } }
            if(!haveC||!haveVal){ error_code(r,*ev,"E1344","enum value entry malformed","need :name and :value"); r.success=false; continue; }
            if(localNames.count(cname) || info.constants.count(cname) || enum_constants_.count(cname)){ error_code(r,*ev,"E1345","enum duplicate constant","rename constant"); r.success=false; continue; }
            // global clash check with globals_/functions_ names?
            if(globals_.count(cname) || functions_.count(cname)){ error_code(r,*ev,"E1348","enum constant global clash","rename constant"); r.success=false; continue; }
            localNames.insert(cname); info.constants[cname]=cval; enum_constants_[cname]= { info.underlying, cval }; }
        enums_[name]=info;
    }
}
inline void TypeChecker::collect_unions(TypeCheckResult& r, const std::vector<node_ptr>& elems){
    for(size_t i=1;i<elems.size(); ++i){ auto &n=elems[i]; if(!n||!std::holds_alternative<list>(n->data)) continue; auto &l=std::get<list>(n->data).elems; if(l.empty()||!std::holds_alternative<symbol>(l[0]->data)) continue; if(std::get<symbol>(l[0]->data).name!="union") continue; std::string name; node_ptr fieldsNode; bool haveName=false, haveFields=false; for(size_t j=1;j<l.size(); ++j){ if(!l[j]||!std::holds_alternative<keyword>(l[j]->data)) break; std::string kw=std::get<keyword>(l[j]->data).name; if(++j>=l.size()) break; auto val=l[j]; if(kw=="name"){ if(std::holds_alternative<symbol>(val->data)) { name=std::get<symbol>(val->data).name; haveName=true; } else if(std::holds_alternative<std::string>(val->data)){ name=std::get<std::string>(val->data); haveName=true; } }
            else if(kw=="fields"){ fieldsNode=val; haveFields=true; }
        }
        if(!haveName){ error_code(r,*n,"E1350","union missing :name","provide (union :name U ...)" ); r.success=false; continue; }
        if(unions_.count(name) || structs_.count(name) || enums_.count(name) || typedefs_.count(name)){ error_code(r,*n,"E1356","union redefinition","choose unique union name"); r.success=false; continue; }
        if(!haveFields || !fieldsNode || !std::holds_alternative<vector_t>(fieldsNode->data)){ error_code(r,*n,"E1351","union missing :fields","add :fields [ (ufield :name a :type i32) ... ]"); r.success=false; continue; }
        UnionInfo info; info.name=name; std::unordered_set<std::string> localNames; for(auto &f : std::get<vector_t>(fieldsNode->data).elems){ if(!f||!std::holds_alternative<list>(f->data)){ error_code(r,*n,"E1352","union field malformed","use (ufield :name x :type <type>)"); r.success=false; continue; } auto &fl=std::get<list>(f->data).elems; if(fl.empty()||!std::holds_alternative<symbol>(fl[0]->data) || std::get<symbol>(fl[0]->data).name!="ufield"){ error_code(r,*f,"E1352","union field malformed","starts with ufield symbol"); r.success=false; continue; } std::string fname; TypeId fty=0; bool haveF=false, haveT=false; for(size_t k=1;k<fl.size(); ++k){ if(!fl[k]||!std::holds_alternative<keyword>(fl[k]->data)) break; std::string kw=std::get<keyword>(fl[k]->data).name; if(++k>=fl.size()) break; auto v=fl[k]; if(kw=="name"){ if(std::holds_alternative<symbol>(v->data)) { fname=std::get<symbol>(v->data).name; haveF=true; } else if(std::holds_alternative<std::string>(v->data)){ fname=std::get<std::string>(v->data); haveF=true; } }
                else if(kw=="type"){ try { fty=ctx_.parse_type(v); haveT=true; } catch(const parse_error&){ error_code(r,*v,"E1353","union field type invalid","fix field :type form"); r.success=false; } }
            }
            if(!haveF||!haveT){ error_code(r,*f,"E1352","union field malformed","need :name and :type" ); r.success=false; continue; }
            if(localNames.count(fname)){ error_code(r,*f,"E1355","union duplicate field","rename field"); r.success=false; continue; }
            // disallow aggregate members for now (simplify size computation); allow base, pointer
            const Type& FT = ctx_.at(fty); if(!(FT.kind==Type::Kind::Base || FT.kind==Type::Kind::Pointer)){ error_code(r,*f,"E1354","union field type unsupported","only base or pointer fields supported initially"); r.success=false; continue; }
            localNames.insert(fname); info.fields.push_back(UnionFieldInfo{fname,fty});
        }
        // success registration even if some fields invalid -> skip if none valid
        if(info.fields.empty()){ error_code(r,*n,"E1352","union field malformed","define at least one valid field"); r.success=false; continue; }
        for(auto &uf: info.fields) info.field_map[uf.name]=&uf;
        unions_[name]=info;
    }
}
// --- M6 Suggestion utilities ---
inline int TypeChecker::edit_distance(const std::string& a, const std::string& b){
    size_t n=a.size(), m=b.size();
    if(n>64||m>64){ // cap to avoid large allocs; simple fallback
        int dist=0; for(size_t i=0;i<std::min(n,m);++i) if(a[i]!=b[i]) ++dist; dist += (int)std::max(n,m)- (int)std::min(n,m); return dist; }
    int dp[65][65];
    for(size_t i=0;i<=n;++i) dp[i][0]=(int)i; for(size_t j=0;j<=m;++j) dp[0][j]=(int)j;
    for(size_t i=1;i<=n;++i){ for(size_t j=1;j<=m;++j){ int c = a[i-1]==b[j-1]?0:1; dp[i][j]=std::min({dp[i-1][j]+1, dp[i][j-1]+1, dp[i-1][j-1]+c}); } }
    return dp[n][m];
}
inline std::vector<std::string> TypeChecker::fuzzy_candidates(const std::string& target, const std::vector<std::string>& pool, int maxDist){
    std::vector<std::string> out; for(auto &c: pool){ if(c.empty()) continue; int d=edit_distance(target,c); if(d<=maxDist) out.push_back(c); }
    if(out.size()>5) out.resize(5); return out;
}
inline void TypeChecker::append_suggestions(TypeError& err, const std::vector<std::string>& suggs){
    if(suggs.empty()) return;
    // Gate with EDN_SUGGEST env var: default ON if unset (to encourage discoverability); explicit 0 disables.
    if(const char* env = std::getenv("EDN_SUGGEST")){ if(env[0]=='0') return; }
    std::string msg="did you mean ";
    for(size_t i=0;i<suggs.size();++i){ msg+=suggs[i]; if(i+1<suggs.size()) msg+= i+2==suggs.size()?" or ":", "; }
    err.notes.push_back(TypeNote{msg,err.line,err.col});
}
inline void TypeChecker::collect_globals(TypeCheckResult& r, const std::vector<node_ptr>& elems){
    for(size_t i=1;i<elems.size(); ++i){
        auto &n = elems[i];
        if(!n||!std::holds_alternative<list>(n->data)) continue;
        auto &l=std::get<list>(n->data).elems;
        if(l.empty()||!std::holds_alternative<symbol>(l[0]->data)) continue;
        if(std::get<symbol>(l[0]->data).name!="global") continue;
        std::string name; TypeId ty=0; bool isConst=false; node_ptr init;
        for(size_t j=1;j<l.size(); ++j){
            if(l[j]&&std::holds_alternative<keyword>(l[j]->data)){
                std::string kw=std::get<keyword>(l[j]->data).name; if(++j>=l.size()) break; auto val=l[j];
                if(kw=="name" && std::holds_alternative<symbol>(val->data)) name=std::get<symbol>(val->data).name;
                else if(kw=="type") ty=parse_type_node(val,r);
                else if(kw=="const"){ if(std::holds_alternative<bool>(val->data)) isConst=std::get<bool>(val->data); }
                else if(kw=="init") init=val;
            }
        }
        if(name.empty()){ error(r,*n,"global missing :name"); r.success=false; continue;}
        if(!ty){ error(r,*n,"global missing :type"); r.success=false; continue;}
        if(globals_.count(name)){ error(r,*n,"duplicate global"); r.success=false; continue;}
        // Validation (M5): const/initializer semantic checks + aggregate literal validation.
        // Error code range E1220+ reserved for global const/data issues.
        if(init){
            const Type& T = ctx_.at(ty);
            auto emitInitError = [&](const std::string& code, const std::string& msg, const std::string& hint=""){ error_code(r,*n,code,msg,hint); r.success=false; };
            auto isIntLit=[&](const node_ptr& lit){ return lit && std::holds_alternative<int64_t>(lit->data); };
            auto isFloatLit=[&](const node_ptr& lit){ return lit && std::holds_alternative<double>(lit->data); };
            auto addMismatch=[&](const std::string& code, const std::string& role, TypeId expected, const std::string& msg, const std::string& hint){
                // emit error with expected/found style notes (Phase 3 diagnostics uniformity)
                ErrorReporter rep{&r.errors,&r.warnings};
                auto err = rep.make_error(code,msg,hint,line(*n),col(*n));
                if(expected){ err.notes.push_back(TypeNote{"expected: "+ctx_.to_string(expected), line(*n), col(*n)}); }
                // For globals, we only have literal node; infer found kind string
                std::string found;
                if(init){ if(std::holds_alternative<int64_t>(init->data)) found="integer literal"; else if(std::holds_alternative<double>(init->data)) found="float literal"; else if(std::holds_alternative<vector_t>(init->data)) found="aggregate literal"; else found="unknown literal"; }
                err.notes.push_back(TypeNote{"   found: "+found, line(*n), col(*n)});
                rep.emit_error(err); r.success=false; };
            auto checkScalar=[&](BaseType b){ bool ok=false; if(is_integer_base(b)) ok = isIntLit(init); else if(is_float_base(b)) ok = isFloatLit(init) || isIntLit(init); if(!ok){ addMismatch("E1220","global", ty, "global scalar initializer type mismatch","use literal matching declared global base type"); } };
            if(T.kind==Type::Kind::Base){ checkScalar(T.base); }
            else if(T.kind==Type::Kind::Array){ if(!std::holds_alternative<vector_t>(init->data)){ emitInitError("E1221","global array initializer must be vector","wrap elements in [ ]"); }
                else {
                    auto &vec=std::get<vector_t>(init->data).elems; if(vec.size()!=T.array_size) emitInitError("E1222","array initializer length mismatch","provide exactly "+std::to_string(T.array_size)+" elements");
                    const Type& ET = ctx_.at(T.elem); if(ET.kind!=Type::Kind::Base){ addMismatch("E1223","array",T.elem, "array element type unsupported for const init","only base scalar elements supported"); }
                    else {
                        for(auto &e: vec){ if(!e){ emitInitError("E1223","array element missing","remove null element"); break; }
                            if(is_integer_base(ET.base)){ if(!isIntLit(e)) { addMismatch("E1223","array",T.elem, "array element literal type mismatch","use integer literal for element type"); break; } }
                            else if(is_float_base(ET.base)){ if(!(isFloatLit(e)||isIntLit(e))) { addMismatch("E1223","array",T.elem, "array element literal type mismatch","use float/int literal convertible to element type"); break; } }
                            else { addMismatch("E1223","array",T.elem, "array element type unsupported","use integer or float base type"); break; }
                        }
                    }
                }
            }
            else if(T.kind==Type::Kind::Struct){ if(!std::holds_alternative<vector_t>(init->data)){ emitInitError("E1224","global struct initializer must be vector","wrap field literals in [ ] in declared order"); }
                else {
                    auto sit = structs_.find(T.struct_name); if(sit==structs_.end()){ emitInitError("E1224","struct for initializer not declared","declare struct before global"); }
                    else {
                        auto &vec=std::get<vector_t>(init->data).elems; if(vec.size()!=sit->second.fields.size()) emitInitError("E1224","struct initializer field count mismatch","provide one literal per field");
                        else {
                            for(size_t fi=0; fi<vec.size() && fi<sit->second.fields.size(); ++fi){ auto &fld = sit->second.fields[fi]; const Type& FT = ctx_.at(fld.type); if(FT.kind!=Type::Kind::Base){ addMismatch("E1225","struct",fld.type, "struct field type unsupported for const init","only base scalar fields supported"); break; }
                                auto lit = vec[fi]; if(is_integer_base(FT.base)){ if(!isIntLit(lit)) { addMismatch("E1225","struct",fld.type, "struct field literal type mismatch","use integer literal"); break; } }
                                else if(is_float_base(FT.base)){ if(!(isFloatLit(lit)||isIntLit(lit))) { addMismatch("E1225","struct",fld.type, "struct field literal type mismatch","use float/int literal convertible to field type"); break; } }
                                else { addMismatch("E1225","struct",fld.type, "struct field type unsupported for const init","only integer/float base fields supported"); break; }
                            }
                        }
                    }
                }
            }
            else { emitInitError("E1228","unsupported global initializer kind","only base/array/struct types currently supported"); }
        }
        if(isConst && !init){ error_code(r,*n,"E1227","const global requires :init","add :init literal or aggregate vector"); r.success=false; }
        globals_[name]=GlobalInfoTC{name,ty,isConst,init};
    }
}
inline TypeId TypeChecker::parse_type_node(const node_ptr& n, TypeCheckResult& r){
    // Intercept simple symbol names for typedef alias resolution.
    if(n && std::holds_alternative<symbol>(n->data)){
        std::string nm = std::get<symbol>(n->data).name;
        auto it = typedefs_.find(nm);
        if(it!=typedefs_.end()) return it->second;
    }
    try { return ctx_.parse_type(n); } catch(const parse_error& e){ r.success=false; error(r,*n,e.what()); return ctx_.get_base(BaseType::I32);} }
inline bool TypeChecker::parse_struct(TypeCheckResult& r, const node_ptr& n){ if(!n||!std::holds_alternative<list>(n->data)) return false; auto &l=std::get<list>(n->data).elems; if(l.empty()) return false; if(!std::holds_alternative<symbol>(l[0]->data)||std::get<symbol>(l[0]->data).name!="struct") return false; std::string name; std::vector<FieldInfo> fields; std::unordered_set<std::string> names; for(size_t i=1;i<l.size(); ++i){ if(!l[i]||!std::holds_alternative<keyword>(l[i]->data)) break; std::string kw=std::get<keyword>(l[i]->data).name; if(++i>=l.size()) break; auto val=l[i]; if(kw=="name"){ if(std::holds_alternative<symbol>(val->data)) name=std::get<symbol>(val->data).name; else if(std::holds_alternative<std::string>(val->data)) name=std::get<std::string>(val->data); else { r.success=false; error(r,*val,"struct :name expects symbol or string"); } } else if(kw=="fields"){ if(!val||!std::holds_alternative<vector_t>(val->data)){ r.success=false; error(r,*val,":fields expects vector"); continue; } auto &vec=std::get<vector_t>(val->data).elems; size_t idx=0; for(auto &f: vec){ if(!f||!std::holds_alternative<list>(f->data)){ r.success=false; error(r,*val,"field entry must be list"); continue; } auto &fl=std::get<list>(f->data).elems; std::string fname; TypeId fty=0; for(size_t k=0;k<fl.size(); ++k){ if(fl[k] && std::holds_alternative<keyword>(fl[k]->data)){ std::string fkw=std::get<keyword>(fl[k]->data).name; if(++k>=fl.size()) break; auto v=fl[k]; if(fkw=="name"){ if(std::holds_alternative<symbol>(v->data)) fname=std::get<symbol>(v->data).name; else { r.success=false; error(r,*v,"field :name expects symbol"); } } else if(fkw=="type"){ fty=parse_type_node(v,r); } } } if(fname.empty()){ r.success=false; error(r,*f,"field missing name"); continue;} if(names.count(fname)){ r.success=false; error(r,*f,"duplicate field"); continue;} names.insert(fname); fields.push_back(FieldInfo{fname,fty,idx++}); } } } if(name.empty()){ r.success=false; error(r,*n,"struct missing :name"); return true; } if(structs_.count(name)){ r.success=false; error(r,*n,"duplicate struct name"); } StructInfo si; si.name=name; si.fields=std::move(fields); for(auto &f: si.fields) si.field_map[f.name]=&f; structs_[name]=std::move(si); return true; }
inline bool TypeChecker::parse_function_header(TypeCheckResult& r, const node_ptr& fn, FunctionInfoTC& out_fn){ if(!fn||!std::holds_alternative<list>(fn->data)) return false; auto &fl=std::get<list>(fn->data).elems; if(fl.empty()) return false; if(!std::holds_alternative<symbol>(fl[0]->data)||std::get<symbol>(fl[0]->data).name!="fn") return false; std::string name; TypeId ret=ctx_.get_base(BaseType::Void); std::vector<ParamInfoTC> params; bool variadic=false; bool external=false; for(size_t i=1;i<fl.size(); ++i){ if(fl[i] && std::holds_alternative<keyword>(fl[i]->data)){ std::string kw=std::get<keyword>(fl[i]->data).name; if(++i>=fl.size()) break; auto val=fl[i]; if(kw=="name"){ if(std::holds_alternative<std::string>(val->data)) name=std::get<std::string>(val->data); } else if(kw=="ret"){ ret=parse_type_node(val,r); } else if(kw=="params"){ if(val && std::holds_alternative<vector_t>(val->data)){ for(auto &p: std::get<vector_t>(val->data).elems){ if(!p||!std::holds_alternative<list>(p->data)) continue; auto &pl=std::get<list>(p->data).elems; if(pl.size()==3 && std::holds_alternative<symbol>(pl[0]->data) && std::get<symbol>(pl[0]->data).name=="param"){ TypeId pty=parse_type_node(pl[1],r); std::string v; if(std::holds_alternative<symbol>(pl[2]->data)){ v=std::get<symbol>(pl[2]->data).name; if(!v.empty()&&v[0]=='%') v.erase(0,1);} params.push_back(ParamInfoTC{v,pty}); } } } } else if(kw=="vararg"){ if(val && std::holds_alternative<bool>(val->data)) variadic=std::get<bool>(val->data); else { error(r,*val,":vararg expects bool"); r.success=false; } } else if(kw=="external"){ if(val && std::holds_alternative<bool>(val->data)) external=std::get<bool>(val->data); else { error(r,*val,":external expects bool"); r.success=false; } } } } if(name.empty()){ r.success=false; error(r,*fn,"function missing :name"); } out_fn.name=name; out_fn.ret=ret; out_fn.params=std::move(params); out_fn.variadic=variadic; out_fn.external=external; return true; }
inline void TypeChecker::collect_structs(TypeCheckResult& r, const std::vector<node_ptr>& elems){ for(size_t i=1;i<elems.size(); ++i) parse_struct(r, elems[i]); }
inline void TypeChecker::collect_functions_headers(TypeCheckResult& r, const std::vector<node_ptr>& elems){ for(size_t i=1;i<elems.size(); ++i){ FunctionInfoTC fi; if(parse_function_header(r, elems[i], fi)){ if(functions_.count(fi.name)){ error(r,*elems[i],"duplicate function name"); r.success=false; } else functions_[fi.name]=fi; } } }
inline void TypeChecker::check_functions(TypeCheckResult& r, const std::vector<node_ptr>& elems){ for(size_t i=1;i<elems.size(); ++i){ auto &n=elems[i]; if(!n||!std::holds_alternative<list>(n->data)) continue; auto &fl=std::get<list>(n->data).elems; if(fl.empty()||!std::holds_alternative<symbol>(fl[0]->data) || std::get<symbol>(fl[0]->data).name!="fn") continue; std::string fname; bool isExternal=false; for(size_t j=1;j<fl.size(); ++j){ if(fl[j] && std::holds_alternative<keyword>(fl[j]->data)){ std::string kw=std::get<keyword>(fl[j]->data).name; if(++j>=fl.size()) break; if(kw=="name" && std::holds_alternative<std::string>(fl[j]->data)) fname=std::get<std::string>(fl[j]->data); else if(kw=="external" && std::holds_alternative<bool>(fl[j]->data)) isExternal=std::get<bool>(fl[j]->data); } } if(fname.empty()) continue; auto it=functions_.find(fname); if(it!=functions_.end()){ if(it->second.external || isExternal) continue; check_function_body(r,n,it->second); } } }
inline void TypeChecker::check_function_body(TypeCheckResult& r, const node_ptr& fn, const FunctionInfoTC& info){ var_types_.clear(); for(auto &p: info.params) var_types_[p.name]=p.type; auto &fl=std::get<list>(fn->data).elems; node_ptr body; for(size_t i=1;i<fl.size(); ++i){ if(fl[i] && std::holds_alternative<keyword>(fl[i]->data) && std::get<keyword>(fl[i]->data).name=="body"){ if(++i<fl.size()) body=fl[i]; break; } } if(!body || !std::holds_alternative<vector_t>(body->data)){ error(r,*fn,":body missing or not vector"); r.success=false; return; } check_instruction_list(r, std::get<vector_t>(body->data).elems, info, 0); }
inline void TypeChecker::check_instruction_list(TypeCheckResult& r, const std::vector<node_ptr>& insts, const FunctionInfoTC& fn, int loop_depth){ auto get_var=[&](const std::string& n)->TypeId{ auto it=var_types_.find(n); return it==var_types_.end() ? (TypeId)-1 : it->second; }; auto attach=[&](const node_ptr& n, TypeId t){ n->metadata["type-id"]=detail::make_node((int64_t)t); }; auto is_int=[&](TypeId t){ const Type& T=ctx_.at(t); if(T.kind!=Type::Kind::Base) return false; switch(T.base){ case BaseType::I1: case BaseType::I8: case BaseType::I16: case BaseType::I32: case BaseType::I64: case BaseType::U8: case BaseType::U16: case BaseType::U32: case BaseType::U64: return true; default: return false;} }; for(auto &n: insts){ if(!n||!std::holds_alternative<list>(n->data)){ error(r,*n,"instruction must be list"); r.success=false; continue; } auto &il=std::get<list>(n->data).elems; if(il.empty()||!std::holds_alternative<symbol>(il[0]->data)){ error(r,*n,"instruction missing opcode"); r.success=false; continue; } std::string op=std::get<symbol>(il[0]->data).name; auto sym=[&](size_t i)->std::string{ if(i<il.size() && std::holds_alternative<symbol>(il[i]->data)) return std::get<symbol>(il[i]->data).name; return std::string{}; };
    if(op=="block"){ std::unordered_map<std::string,TypeId> saved=var_types_; for(size_t i=1;i<il.size(); ++i){ if(!il[i]||!std::holds_alternative<keyword>(il[i]->data)) break; std::string kw=std::get<keyword>(il[i]->data).name; if(++i>=il.size()) break; auto val=il[i]; if(kw=="locals"){ if(val && std::holds_alternative<vector_t>(val->data)){ for(auto &d: std::get<vector_t>(val->data).elems){ if(!d||!std::holds_alternative<list>(d->data)) continue; auto &dl=std::get<list>(d->data).elems; if(dl.size()==3 && std::holds_alternative<symbol>(dl[0]->data) && std::get<symbol>(dl[0]->data).name=="local"){ TypeId lty=parse_type_node(dl[1],r); if(std::holds_alternative<symbol>(dl[2]->data)){ std::string vn=std::get<symbol>(dl[2]->data).name; if(!vn.empty()&&vn[0]=='%') vn.erase(0,1); if(var_types_.count(vn)){ error(r,*d,"duplicate local"); r.success=false; } else var_types_[vn]=lty; } } } } } else if(kw=="body"){ if(val && std::holds_alternative<vector_t>(val->data)) check_instruction_list(r, std::get<vector_t>(val->data).elems, fn); } } var_types_=saved; continue; }
    if(op=="if"){ if(il.size()<3){ error_code(r,*n,"E1000","if arity","expected (if %cond [ then ] [ else ])"); r.success=false; continue; } std::string cond=sym(1); if(cond.empty()||cond[0] != '%'){ error_code(r,*n,"E1001","if cond must be %var","prefix condition with %"); r.success=false; continue; } auto ct=get_var(cond.substr(1)); if(ct!=(TypeId)-1){ const Type& T=ctx_.at(ct); if(!(T.kind==Type::Kind::Base && T.base==BaseType::I1)){ error_code(r,*n,"E1002","if cond must be i1","use boolean (i1) value"); r.success=false; } } if(il.size()>=3 && std::holds_alternative<vector_t>(il[2]->data)) check_instruction_list(r, std::get<vector_t>(il[2]->data).elems, fn); if(il.size()>=4 && std::holds_alternative<vector_t>(il[3]->data)) check_instruction_list(r, std::get<vector_t>(il[3]->data).elems, fn); continue; }
    if(op=="while"){ if(il.size()<3){ error_code(r,*n,"E1003","while arity","expected (while %cond [ body ])"); r.success=false; continue; } std::string cond=sym(1); if(cond.empty()||cond[0] != '%'){ error_code(r,*n,"E1004","while cond must be %var","prefix condition with %"); r.success=false; continue; } auto ct=get_var(cond.substr(1)); if(ct!=(TypeId)-1){ const Type& T=ctx_.at(ct); if(!(T.kind==Type::Kind::Base && T.base==BaseType::I1)){ error_code(r,*n,"E1005","while cond must be i1","use boolean (i1) value"); r.success=false; } } if(il.size()>=3 && std::holds_alternative<vector_t>(il[2]->data)) check_instruction_list(r, std::get<vector_t>(il[2]->data).elems, fn, loop_depth+1); continue; }
    // --- Phase 3 For Loop (E137x) ---
    if(op=="for"){ // (for :init [ ... ] :cond %c :step [ ... ] :body [ ... ]) order flexible but all required
        // Parse keyword pairs
        std::vector<node_ptr> initVec, stepVec, bodyVec; std::string condVar; bool haveInit=false, haveCond=false, haveStep=false, haveBody=false;
        for(size_t i=1;i<il.size(); ++i){ if(!il[i]||!std::holds_alternative<keyword>(il[i]->data)) break; std::string kw=std::get<keyword>(il[i]->data).name; if(++i>=il.size()) break; auto val=il[i]; if(kw=="init"){ if(val && std::holds_alternative<vector_t>(val->data)){ initVec=std::get<vector_t>(val->data).elems; haveInit=true;} }
            else if(kw=="cond"){ if(val && std::holds_alternative<symbol>(val->data)){ condVar=std::get<symbol>(val->data).name; haveCond=true; } }
            else if(kw=="step"){ if(val && std::holds_alternative<vector_t>(val->data)){ stepVec=std::get<vector_t>(val->data).elems; haveStep=true; } }
            else if(kw=="body"){ if(val && std::holds_alternative<vector_t>(val->data)){ bodyVec=std::get<vector_t>(val->data).elems; haveBody=true; } }
        }
        if(!haveInit){ error_code(r,*n,"E1371","for missing :init","add :init [ ... ] vector"); r.success=false; }
        if(!haveCond){ error_code(r,*n,"E1372","for missing :cond","add :cond %var (i1)"); r.success=false; }
        if(!haveStep){ error_code(r,*n,"E1375","for missing :step","add :step [ ... ] even if empty"); r.success=false; }
        if(!haveBody){ error_code(r,*n,"E1374","for missing :body","add :body [ ... ]"); r.success=false; }
        // Execute :init first so variables declared there are visible to condition validation
        if(haveInit) check_instruction_list(r, initVec, fn, loop_depth);
        // Now validate condition variable (after :init definitions)
        if(haveCond){ if(condVar.empty()||condVar[0] != '%'){ error_code(r,*n,"E1372","for missing :cond","use %var as condition symbol"); r.success=false; }
            else { auto ct=get_var(condVar.substr(1)); if(ct!=(TypeId)-1){ const Type& T=ctx_.at(ct); if(!(T.kind==Type::Kind::Base && T.base==BaseType::I1)){ error_code(r,*n,"E1373","for cond must be i1","ensure condition variable has type i1"); r.success=false; } } } }
        // body & step inside loop scope (loop_depth+1 so break/continue allowed)
        if(haveBody) check_instruction_list(r, bodyVec, fn, loop_depth+1);
        if(haveStep) check_instruction_list(r, stepVec, fn, loop_depth+1);
        continue;
    }
    if(op=="continue"){ if(loop_depth==0){ error_code(r,*n,"E1380","continue outside loop","use inside while/for body"); r.success=false; } if(il.size()!=1){ error_code(r,*n,"E1381","continue takes no operands","remove extra tokens"); r.success=false; } continue; }
    if(op=="switch"){ // (switch %expr :cases [ (case <int> [ ... ])* ] :default [ ... ])
        if(il.size()<2 || !std::holds_alternative<symbol>(il[1]->data)){
            error_code(r,*n,"E1390","switch missing expr","provide (switch %var :cases [...] :default [...])"); r.success=false; continue; }
        std::string exprSym = sym(1);
        if(exprSym.empty() || exprSym[0] != '%'){ error_code(r,*n,"E1391","switch expr must be %var","prefix expression variable with %"); r.success=false; }
        TypeId exprTy=(TypeId)-1; if(!exprSym.empty() && exprSym[0]=='%') exprTy=get_var(exprSym.substr(1));
        if(exprTy!=(TypeId)-1){ const Type& ET=ctx_.at(exprTy); if(!(ET.kind==Type::Kind::Base && is_integer_base(ET.base))){ error_code(r,*n,"E1392","switch expr must be int","use integer base type for switch expression"); r.success=false; } }
        bool haveCases=false, haveDefault=false; node_ptr casesNode, defaultNode;
        for(size_t i=2;i<il.size(); ++i){ if(!il[i]||!std::holds_alternative<keyword>(il[i]->data)) break; std::string kw=std::get<keyword>(il[i]->data).name; if(++i>=il.size()) break; auto val=il[i]; if(kw=="cases"){ casesNode=val; haveCases=true; } else if(kw=="default"){ defaultNode=val; haveDefault=true; } }
        if(!haveCases){ error_code(r,*n,"E1393","switch missing :cases","add :cases [ (case <int> [ ... ])* ]"); r.success=false; }
        if(haveCases && (!casesNode || !std::holds_alternative<vector_t>(casesNode->data))){ error_code(r,*n,"E1394","switch cases must be vector","wrap cases in [ ]"); r.success=false; haveCases=false; }
        std::unordered_set<int64_t> caseVals; if(haveCases){ for(auto &cv : std::get<vector_t>(casesNode->data).elems){ if(!cv||!std::holds_alternative<list>(cv->data)){ error_code(r,*n,"E1395","switch case malformed","use (case <int> [ ... ])"); r.success=false; continue; } auto &cl=std::get<list>(cv->data).elems; if(cl.size()<3 || !std::holds_alternative<symbol>(cl[0]->data) || std::get<symbol>(cl[0]->data).name!="case" || !std::holds_alternative<int64_t>(cl[1]->data) || !std::holds_alternative<vector_t>(cl[2]->data)){ error_code(r,*cv,"E1395","switch case malformed","expected (case <int> [ ... ])"); r.success=false; continue; } int64_t cval=std::get<int64_t>(cl[1]->data); if(caseVals.count(cval)){ error_code(r,*cv,"E1396","switch duplicate case","remove duplicate constant"); r.success=false; continue; } caseVals.insert(cval); // Descend into case body
                check_instruction_list(r, std::get<vector_t>(cl[2]->data).elems, fn, loop_depth); }
        }
        if(!haveDefault){ error_code(r,*n,"E1397","switch missing :default","add :default [ ... ] block"); r.success=false; }
        if(haveDefault && (!defaultNode || !std::holds_alternative<vector_t>(defaultNode->data))){ error_code(r,*n,"E1397","switch missing :default","provide :default [ ... ] vector"); r.success=false; }
        if(haveDefault && defaultNode && std::holds_alternative<vector_t>(defaultNode->data)){
            check_instruction_list(r, std::get<vector_t>(defaultNode->data).elems, fn, loop_depth);
        }
        continue;
    }
    if(op=="break"){ if(loop_depth==0){ error_code(r,*n,"E1006","break outside loop","use inside (while ...) body"); r.success=false; } if(il.size()!=1){ error_code(r,*n,"E1007","break takes no operands","remove extra tokens"); r.success=false; } continue; }
    if(op=="and"||op=="or"||op=="xor"||op=="shl"||op=="lshr"||op=="ashr"){ if(il.size()!=5){ error_code(r,*n,"E0600","bit/logical op arity","expected ("+op+" %dst <int-type> %a %b)"); r.success=false; continue; } std::string dst=sym(1), a=sym(3), b=sym(4); if(dst.empty()||a.empty()||b.empty()){ error_code(r,*n,"E0601","bit/logical expects symbols","use %dst %lhs %rhs"); r.success=false; continue; } TypeId ty=parse_type_node(il[2],r); if(ty==(TypeId)-1){ r.success=false; continue; } const Type& T=ctx_.at(ty); if(!(T.kind==Type::Kind::Base && (T.base==BaseType::I1||T.base==BaseType::I8||T.base==BaseType::I16||T.base==BaseType::I32||T.base==BaseType::I64))){ error_code(r,*n,"E0602","bit/logical op type must be integer","choose i1/i8/i16/i32/i64"); r.success=false; }
        auto check_operand=[&](const std::string& v){ if(v[0]=='%'){ auto vt=get_var(v.substr(1)); if(vt!=(TypeId)-1 && vt!=ty){ error_code(r,*n,"E0603","operand type mismatch","operands must match annotated type"); r.success=false; } } else { error_code(r,*n,"E0604","operand must be %var","prefix with %"); r.success=false; } };
        check_operand(a); check_operand(b); if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0605","redefinition of variable","rename destination"); r.success=false; } var_types_[dst.substr(1)]=ty; attach(n,ty);} else { error_code(r,*n,"E0606","dest must be %var","prefix destination with %"); r.success=false; } continue; }
    if(op=="fadd"||op=="fsub"||op=="fmul"||op=="fdiv"){ if(il.size()!=5){ error_code(r,*n,"E0700","fbinop arity","expected ("+op+" %dst <float-type> %a %b)"); r.success=false; continue; } std::string dst=sym(1),a=sym(3),b=sym(4); if(dst.empty()||a.empty()||b.empty()){ error_code(r,*n,"E0701","fbinop symbol expected","use % for dst and operands"); r.success=false; continue; } TypeId ty=parse_type_node(il[2],r); const Type& T=ctx_.at(ty); if(!(T.kind==Type::Kind::Base && (T.base==BaseType::F32||T.base==BaseType::F64))){ error_code(r,*n,"E0702","fbinop type must be f32/f64","choose f32 or f64"); r.success=false; }
        if(a[0]=='%'){ auto at=get_var(a.substr(1)); if(at!=(TypeId)-1 && at!=ty){ type_mismatch(r,*n,"E0703","lhs",ty,at); r.success=false; } }
        if(b[0]=='%'){ auto bt=get_var(b.substr(1)); if(bt!=(TypeId)-1 && bt!=ty){ type_mismatch(r,*n,"E0704","rhs",ty,bt); r.success=false; } }
        if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0705","redefinition of variable","rename destination"); r.success=false; } var_types_[dst.substr(1)]=ty; attach(n,ty);} else { error_code(r,*n,"E0706","dest must be %var","prefix destination with %"); r.success=false; } continue; }
    if(op=="add"||op=="sub"||op=="mul"||op=="sdiv"||op=="udiv"||op=="srem"||op=="urem"){ if(il.size()!=5){ error_code(r,*n,"E0001","binop arity","expected ("+op+" %dst <int-type> %a %b)"); r.success=false; continue; } std::string dst=sym(1),a=sym(3),b=sym(4); if(dst.empty()||a.empty()||b.empty()){ error_code(r,*n,"E0002","binop symbol expected","use %prefix on destination and operands e.g. %x"); r.success=false; continue; } TypeId ty=parse_type_node(il[2],r); if(!is_int(ty)){ error_code(r,*n,"E0003","binop type must be integer","choose one of i1/i8/i16/i32/i64/u8/u16/u32/u64"); r.success=false; } if(a[0]=='%'){ auto at=get_var(a.substr(1)); if(at!=(TypeId)-1 && at!=ty){ type_mismatch(r,*n,"E0004","lhs",ty,at); r.success=false; } } if(b[0]=='%'){ auto bt=get_var(b.substr(1)); if(bt!=(TypeId)-1 && bt!=ty){ type_mismatch(r,*n,"E0005","rhs",ty,bt); r.success=false; } } if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0006","redefinition of variable","rename destination to fresh SSA name"); r.success=false; } var_types_[dst.substr(1)]=ty; attach(n,ty);} else { error_code(r,*n,"E0007","dest must be %var","prepend % to create SSA name"); r.success=false; } continue; }
    // --- Phase 3.1 Pointer Arithmetic ---
    if(op=="ptr-add"||op=="ptr-sub"){ // (ptr-add %dst (ptr <T>) %base %offset)
        if(il.size()!=5){ error_code(r,*n,"E1300",op+" arity","expected ("+op+" %dst (ptr <T>) %base %offset)"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0]!='%'){ error_code(r,*n,"E1301",op+" dst must be %var","prefix destination with %"); r.success=false; continue; }
        TypeId annot=parse_type_node(il[2],r); const Type& AT=ctx_.at(annot); if(AT.kind!=Type::Kind::Pointer){ error_code(r,*n,"E1303","ptr op annotation must be pointer","use (ptr <elem-type>)"); r.success=false; }
        std::string base=sym(3); if(base.empty()||base[0] != '%'){ error_code(r,*n,"E1302","ptr base must be %var","prefix base with %"); r.success=false; continue; }
        std::string off=sym(4); if(off.empty()||off[0] != '%'){ error_code(r,*n,"E1304","ptr offset must be %var int","prefix offset with % and define integer variable"); r.success=false; continue; }
        auto bt=get_var(base.substr(1)); if(bt==(TypeId)-1){ error_code(r,*n,"E1303","ptr base type mismatch","base undefined or wrong type"); r.success=false; }
    else { const Type& BT=ctx_.at(bt); if(BT.kind!=Type::Kind::Pointer || BT.pointee!=AT.pointee){ if(BT.kind==Type::Kind::Pointer) type_mismatch(r,*n,"E1303","ptr base",ctx_.get_pointer(AT.pointee),bt); else error_code(r,*n,"E1303","ptr base type mismatch","ensure base has type annotation pointer"); r.success=false; } }
        auto ot=get_var(off.substr(1)); if(ot!=(TypeId)-1){ const Type& OT=ctx_.at(ot); if(!(OT.kind==Type::Kind::Base && is_integer_base(OT.base))){ error_code(r,*n,"E1304","ptr offset must be %var int","offset must be integer variable"); r.success=false; } }
        else { error_code(r,*n,"E1304","ptr offset must be %var int","define integer offset earlier"); r.success=false; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1305","redefinition of variable","rename destination"); r.success=false; } else { var_types_[dst.substr(1)]=annot; attach(n,annot);} continue;
    }
    if(op=="ptr-diff"){ // (ptr-diff %dst <int-type> %a %b)
        if(il.size()!=5){ error_code(r,*n,"E1306","ptr-diff arity","expected (ptr-diff %dst <int-type> %a %b)"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1307","ptr-diff dst must be %var","prefix destination with %"); r.success=false; continue; }
        TypeId rty=parse_type_node(il[2],r); const Type& RT=ctx_.at(rty); if(!(RT.kind==Type::Kind::Base && is_integer_base(RT.base))){ error_code(r,*n,"E1308","ptr-diff result type must be int","choose integer base type"); r.success=false; }
        std::string a=sym(3), b=sym(4); if(a.empty()||b.empty()||a[0] != '%'||b[0] != '%'){ error_code(r,*n,"E1308","ptr-diff operands must be %var","supply %a %b"); r.success=false; continue; }
    auto at=get_var(a.substr(1)); auto bt=get_var(b.substr(1)); bool mismatch=false; bool bothPtr=false; if(at==(TypeId)-1||bt==(TypeId)-1) mismatch=true; else { const Type& ATy=ctx_.at(at); const Type& BTy=ctx_.at(bt); bothPtr = (ATy.kind==Type::Kind::Pointer && BTy.kind==Type::Kind::Pointer); if(!bothPtr || ATy.pointee!=BTy.pointee) mismatch=true; }
    if(mismatch){ if(bothPtr){ const Type& ATy=ctx_.at(at); const Type& BTy=ctx_.at(bt); type_mismatch(r,*n,"E1309","ptr-diff operand",ATy.pointee,BTy.pointee); } else { error_code(r,*n,"E1309","ptr-diff pointer type mismatch","both operands must be same pointer type"); } r.success=false; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1305","redefinition of variable","rename destination"); r.success=false; }
        var_types_[dst.substr(1)]=rty; attach(n,rty); continue;
    }
    // --- Phase 3.2 Address-of & Deref ---
    if(op=="addr"){ // (addr %dst (ptr <T>) %src)
        if(il.size()!=4){ error_code(r,*n,"E1310","addr arity","expected (addr %dst (ptr <T>) %src)"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1311","addr dst must be %var","prefix destination with %"); r.success=false; continue; }
        TypeId annot=parse_type_node(il[2],r); const Type& AT=ctx_.at(annot); if(AT.kind!=Type::Kind::Pointer){ error_code(r,*n,"E1312","addr annotation must be pointer","use (ptr <elem-type>)"); r.success=false; }
        std::string src=sym(3); if(src.empty()||src[0] != '%'){ error_code(r,*n,"E1313","addr source must be %var","prefix source with %"); r.success=false; continue; }
        auto st=get_var(src.substr(1)); if(st==(TypeId)-1){ error_code(r,*n,"E1314","addr source undefined","define source earlier"); r.success=false; }
    else { const Type& srcT=ctx_.at(st); (void)srcT; if(AT.kind==Type::Kind::Pointer && AT.pointee!=st){ type_mismatch(r,*n,"E1315","addr source",AT.pointee,st); r.success=false; } }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1316","redefinition of variable","rename destination"); r.success=false; }
        var_types_[dst.substr(1)]=annot; attach(n,annot); continue; }
    if(op=="deref"){ // (deref %dst <T> %ptr)
        if(il.size()!=4){ error_code(r,*n,"E1317","deref arity","expected (deref %dst <type> %ptr)"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1318","deref dst must be %var","prefix destination with %"); r.success=false; continue; }
        TypeId ty=parse_type_node(il[2],r); std::string ptr=sym(3); if(ptr.empty()||ptr[0] != '%'){ error_code(r,*n,"E1319","deref ptr must be %var","prefix pointer with %"); r.success=false; continue; }
        auto pt=get_var(ptr.substr(1)); if(pt==(TypeId)-1){ error_code(r,*n,"E1319","deref ptr must be %var","pointer var undefined"); r.success=false; }
    else { const Type& PT=ctx_.at(pt); if(PT.kind!=Type::Kind::Pointer || PT.pointee!=ty){ if(PT.kind==Type::Kind::Pointer) type_mismatch(r,*n,"E1319","deref ptr",ty,PT.pointee); else error_code(r,*n,"E1319","deref ptr type mismatch","pointer pointee must match <type>"); r.success=false; } }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1316","redefinition of variable","rename destination"); r.success=false; }
        var_types_[dst.substr(1)]=ty; attach(n,ty); continue; }
    // --- Phase 3.3 Function Pointers & Indirect Call ---
    if(op=="fnptr"){ // (fnptr %dst (ptr (fn-type ...)) FunctionName)
        if(il.size()!=4){ error_code(r,*n,"E1329","fnptr arity","expected (fnptr %dst (ptr (fn-type ...)) Name)"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1321","call-indirect dst must be %var","prefix destination with %"); r.success=false; continue; }
        TypeId pty=parse_type_node(il[2],r); const Type& PTY=ctx_.at(pty); if(PTY.kind!=Type::Kind::Pointer || ctx_.at(PTY.pointee).kind!=Type::Kind::Function){ error_code(r,*n,"E1323","fnptr annotation must be ptr to fn","use (ptr (fn-type ...))"); r.success=false; }
        std::string fname=sym(3); if(fname.empty()){ error_code(r,*n,"E1329","fnptr arity","expected (fnptr %dst (ptr (fn-type ...)) Name)"); r.success=false; continue; }
        FunctionInfoTC* finfo=nullptr; if(!lookup_function(fname, finfo)){ error_code(r,*n,"E0403","unknown callee","define function first"); r.success=false; }
    if(finfo){ const Type& FT=ctx_.at(PTY.pointee); if(FT.kind==Type::Kind::Function){ if(FT.ret!=finfo->ret){ type_mismatch(r,*n,"E1324","fnptr return",finfo->ret,FT.ret); r.success=false; } if(FT.params.size()!=finfo->params.size()){ error_code(r,*n,"E1324","fnptr param count mismatch","expected "+std::to_string(finfo->params.size())+" got "+std::to_string(FT.params.size())); r.success=false; } else { for(size_t i=0;i<FT.params.size(); ++i){ if(FT.params[i]!=finfo->params[i].type){ type_mismatch(r,*n,"E1324","fnptr param"+std::to_string(i),finfo->params[i].type,FT.params[i]); r.success=false; break; } } } } }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1328","redefinition of variable","rename destination"); r.success=false; }
        var_types_[dst.substr(1)]=pty; attach(n,pty); continue; }
    if(op=="call-indirect"){ // (call-indirect %dst <ret-type> %fptr %arg...)
        if(il.size()<4){ error_code(r,*n,"E1320","call-indirect arity","expected (call-indirect %dst <ret-type> %fptr %args...)"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1321","call-indirect dst must be %var","prefix destination with %"); r.success=false; continue; }
        TypeId ret=parse_type_node(il[2],r); std::string fptr=sym(3); if(fptr.empty()||fptr[0]!='%'){ error_code(r,*n,"E1322","call-indirect fptr must be %var","prefix function pointer with %"); r.success=false; continue; }
        auto fp=get_var(fptr.substr(1)); if(fp==(TypeId)-1){ error_code(r,*n,"E1323","call-indirect fptr undefined","define fn pointer earlier"); r.success=false; continue; }
        const Type& FPT=ctx_.at(fp); if(FPT.kind!=Type::Kind::Pointer){ error_code(r,*n,"E1323","call-indirect fptr not pointer","pointer to (fn-type ...) required"); r.success=false; }
        Type fnTy; if(FPT.kind==Type::Kind::Pointer) fnTy=ctx_.at(FPT.pointee); if(FPT.kind!=Type::Kind::Pointer || fnTy.kind!=Type::Kind::Function){ error_code(r,*n,"E1323","call-indirect fptr not function pointer","use (ptr (fn-type ...))"); r.success=false; }
    if(fnTy.kind==Type::Kind::Function){ if(ret!=fnTy.ret){ type_mismatch(r,*n,"E1324","call-indirect return",fnTy.ret,ret); r.success=false; } size_t expected=fnTy.params.size(); size_t provided = (il.size()>4)? il.size()-4:0; if(provided!=expected){ error_code(r,*n,"E1325","call-indirect arg count mismatch","expected "+std::to_string(expected)+" got "+std::to_string(provided)); r.success=false; } else { for(size_t ai=0; ai<provided; ++ai){ std::string av=sym(4+ai); if(av.empty()||av[0] != '%'){ error_code(r,*n,"E1326","call-indirect arg must be %var","prefix each arg with %"); r.success=false; continue; } auto at=get_var(av.substr(1)); if(at==(TypeId)-1){ error_code(r,*n,"E0407","unknown arg var","define arg value earlier"); r.success=false; } else if(at!=fnTy.params[ai]){ type_mismatch(r,*n,"E1327","call-indirect arg"+std::to_string(ai),fnTy.params[ai],at); r.success=false; } } } }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1328","redefinition of variable","rename destination"); r.success=false; }
        var_types_[dst.substr(1)]=ret; attach(n,ret); continue; }
    if(op=="eq"||op=="ne"||op=="lt"||op=="gt"||op=="le"||op=="ge"){ if(const char* warnEnv = std::getenv("EDN_WARN_DEPRECATED")){ if(warnEnv[0]=='1') warn(r,*n,"legacy comparison op '"+op+"' deprecated; use (icmp %dst <type> :pred <pred>)"); } if(il.size()!=5){ error_code(r,*n,"E0100","cmp arity","expected ("+op+" %dst <int-type> %a %b)"); r.success=false; continue; } std::string dst=sym(1),a=sym(3),b=sym(4); if(dst.empty()||a.empty()||b.empty()){ error_code(r,*n,"E0101","cmp symbol expected","supply %dst %lhs %rhs"); r.success=false; continue; } TypeId opty=parse_type_node(il[2],r); if(!is_int(opty)){ error_code(r,*n,"E0102","cmp operand type must be integer","use integer base type"); r.success=false; } auto check_operand=[&](const std::string& v){ if(v[0]=='%'){ auto vt=get_var(v.substr(1)); if(vt!=(TypeId)-1 && vt!=opty){ error_code(r,*n,"E0103","cmp operand type mismatch","make both operands the same type as annotated comparison type"); r.success=false; } } else { error_code(r,*n,"E0104","operand must be %var","precede variable with %"); r.success=false; } }; check_operand(a); check_operand(b); if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0105","redefinition of variable","rename destination"); r.success=false; } TypeId rty=ctx_.get_base(BaseType::I1); var_types_[dst.substr(1)]=rty; attach(n,rty);} else { error_code(r,*n,"E0106","dest must be %var","precede destination with %"); r.success=false; } continue; }
    if(op=="icmp"){ if(il.size()!=7){ error_code(r,*n,"E0110","icmp arity","expected (icmp %dst <int-type> :pred <pred> %a %b)"); r.success=false; continue; } std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E0111","icmp dst must be %var","prefix destination with %"); r.success=false; continue; } TypeId opty=parse_type_node(il[2],r); if(!is_int(opty)){ error_code(r,*n,"E0112","icmp operand type must be integer","choose integer type"); r.success=false; } if(!il[3]||!std::holds_alternative<keyword>(il[3]->data) || std::get<keyword>(il[3]->data).name!="pred"){ error_code(r,*n,"E0113","icmp expects :pred keyword","insert :pred before predicate symbol"); r.success=false; continue; } std::string pred=sym(4); static const std::unordered_set<std::string> preds{"eq","ne","slt","sgt","sle","sge","ult","ugt","ule","uge"}; if(!preds.count(pred)){ error_code(r,*n,"E0114","unknown icmp predicate","use one of eq/ne/slt/sgt/sle/sge/ult/ugt/ule/uge"); r.success=false; } std::string a=sym(5), b=sym(6); if(a.empty()||b.empty()){ error_code(r,*n,"E0115","icmp operands required","supply %lhs %rhs"); r.success=false; continue; } auto check_operand=[&](const std::string& v){ if(v[0]=='%'){ auto vt=get_var(v.substr(1)); if(vt!=(TypeId)-1 && vt!=opty){ type_mismatch(r,*n,"E0116","icmp operand",opty,vt); r.success=false; } } else { error_code(r,*n,"E0117","icmp operand must be %var","prefix operand with %"); r.success=false; } }; check_operand(a); check_operand(b); if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0118","redefinition of variable","rename destination"); r.success=false; } TypeId rty=ctx_.get_base(BaseType::I1); var_types_[dst.substr(1)]=rty; attach(n,rty); continue; }
    // Cast instructions (Phase 2) with coded diagnostics
    if(op=="as"){ // (as %dst <to-type> %src) sugar selecting specific cast opcode
        if(il.size()!=4){ error_code(r,*n,"E13A0","as arity","expected (as %dst <to-type> %src)"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E13A1","as dst must be %var","prefix destination with %"); r.success=false; continue; }
        TypeId toTy=parse_type_node(il[2],r); std::string src=sym(3); if(src.empty()||src[0] != '%'){ error_code(r,*n,"E13A2","as src must be %var","prefix source with %"); r.success=false; continue; }
        auto st=get_var(src.substr(1)); if(st==(TypeId)-1){ error_code(r,*n,"E13A3","as unknown src var","define source earlier"); r.success=false; continue; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E13A5","redefinition of variable","rename destination"); r.success=false; }
        const Type& FROM=ctx_.at(st); const Type& TO=ctx_.at(toTy);
        auto isInt=[&](const Type&t){ return t.kind==Type::Kind::Base && is_integer_base(t.base); };
        auto intWidth=[&](const Type&t){ switch(t.base){ case BaseType::I1: return 1; case BaseType::I8: case BaseType::U8: return 8; case BaseType::I16: case BaseType::U16: return 16; case BaseType::I32: case BaseType::U32: return 32; case BaseType::I64: case BaseType::U64: return 64; default: return 0; } };
        std::string chosen; bool ok=true;
        if(isInt(FROM) && isInt(TO)){
            int fw=intWidth(FROM), tw=intWidth(TO);
            if(fw==tw) chosen="bitcast"; else if(fw<tw) chosen = is_signed_base(FROM.base)?"sext":"zext"; else chosen="trunc";
        } else if(isInt(FROM) && TO.kind==Type::Kind::Base && (TO.base==BaseType::F32||TO.base==BaseType::F64)){
            chosen = is_signed_base(FROM.base)?"sitofp":"uitofp";
        } else if(FROM.kind==Type::Kind::Base && (FROM.base==BaseType::F32||FROM.base==BaseType::F64) && isInt(TO)){
            chosen = is_signed_base(TO.base)?"fptosi":"fptoui";
        } else if(FROM.kind==Type::Kind::Pointer && isInt(TO)){
            chosen = "ptrtoint";
        } else if(isInt(FROM) && TO.kind==Type::Kind::Pointer){
            chosen = "inttoptr";
        } else if(FROM.kind==Type::Kind::Pointer && TO.kind==Type::Kind::Pointer){
            chosen = "bitcast";
        } else if(FROM.kind==Type::Kind::Base && (FROM.base==BaseType::F32||FROM.base==BaseType::F64) && TO.kind==Type::Kind::Base && (TO.base==BaseType::F32||TO.base==BaseType::F64) && FROM.base==TO.base){
            chosen="bitcast"; // identical float type
        } else {
            ok=false;
        }
        if(!ok||chosen.empty()){ error_code(r,*n,"E13A4","as unsupported conversion","no implicit op available between types"); r.success=false; continue; }
        // Desugar by rewriting this list node into the explicit cast instruction form
        // Replace symbol at position 0 with chosen opcode and keep (%dst <to-type> %src) shape
        if(n && std::holds_alternative<list>(n->data)){
            auto &lst = std::get<list>(n->data).elems;
            if(!lst.empty() && lst[0] && std::holds_alternative<symbol>(lst[0]->data)){
                std::get<symbol>(lst[0]->data).name = chosen; // mutate op symbol
            }
        }
        if(!var_types_.count(dst.substr(1))) var_types_[dst.substr(1)]=toTy; attach(n,toTy); continue; }
    if(op=="zext"||op=="sext"||op=="trunc"||op=="bitcast"||op=="sitofp"||op=="uitofp"||op=="fptosi"||op=="fptoui"||op=="ptrtoint"||op=="inttoptr"){
        if(il.size()!=4){ error_code(r,*n,"E0500", op+" arity","expected ("+op+" %dst <to-type> %src)"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E0504","cast dst must be %var","prefix destination with %"); r.success=false; continue; }
        TypeId toTy=parse_type_node(il[2],r); std::string src=sym(3); if(src.empty()||src[0] != '%'){ error_code(r,*n,"E0501", op+" src must be %var","prefix source with %"); r.success=false; continue; }
        auto st=get_var(src.substr(1)); if(st==(TypeId)-1){ error_code(r,*n,"E0502","unknown cast source var","define source earlier"); r.success=false; continue; }
        const Type& TO=ctx_.at(toTy); const Type& FROM=ctx_.at(st);
        auto bad=[&](const std::string& msg, const std::string& hint){ error_code(r,*n,"E0508",msg,hint); r.success=false; };
        auto width=[&](TypeId t)->unsigned{ const Type& TT=ctx_.at(t); if(TT.kind!=Type::Kind::Base) return 0; return base_type_bit_width(TT.base); };
        if(op=="zext"||op=="sext"||op=="trunc"){
            if(FROM.kind!=Type::Kind::Base || TO.kind!=Type::Kind::Base || !is_integer_base(FROM.base) || !is_integer_base(TO.base)) bad("int cast requires integer types","both from/to must be integer base types");
            else {
                unsigned fw=width(st), tw=width(toTy); if(fw==0||tw==0||fw==tw) bad("width change required","choose different bit width");
                else if(op=="zext"||op=="sext"){ if(!(tw>fw)) bad("extension must increase width","target width > source width"); if(op=="sext" && !is_signed_base(FROM.base)) bad("sext requires signed source","use signed integer source"); }
                else if(op=="trunc" && !(fw>tw)) bad("trunc must reduce width","source width > target width");
            }
        } else if(op=="bitcast"){
            bool ok=false; if(FROM.kind==Type::Kind::Pointer && TO.kind==Type::Kind::Pointer) ok=true; else if(FROM.kind==Type::Kind::Base && TO.kind==Type::Kind::Base){ unsigned fw=width(st), tw=width(toTy); ok = (fw==tw && fw!=0); }
            if(!ok) bad("invalid bitcast types","same bit width scalars or pointer->pointer only");
        } else if(op=="sitofp"||op=="uitofp"){
            if(FROM.kind!=Type::Kind::Base || TO.kind!=Type::Kind::Base || !is_integer_base(FROM.base) || !is_float_base(TO.base)) bad("int->float cast types invalid","from integer to float");
            else if(op=="sitofp" && !is_signed_base(FROM.base)) bad("sitofp requires signed int source","use signed integer");
        } else if(op=="fptosi"||op=="fptoui"){
            if(FROM.kind!=Type::Kind::Base || TO.kind!=Type::Kind::Base || !is_float_base(FROM.base) || !is_integer_base(TO.base)) bad("float->int cast types invalid","from float to integer");
            else if(op=="fptosi" && !is_signed_base(TO.base)) bad("fptosi requires signed int destination","target must be signed integer");
        } else if(op=="ptrtoint"){
            if(FROM.kind!=Type::Kind::Pointer || TO.kind!=Type::Kind::Base || !is_integer_base(TO.base)) bad("ptrtoint requires pointer -> integer","source pointer, target integer");
            else { unsigned tw=width(toTy); if(!(tw==64 || tw==32)) bad("ptrtoint integer width must match pointer size (assume 64/32)","use i64 or i32"); }
        } else if(op=="inttoptr"){
            if(FROM.kind!=Type::Kind::Base || TO.kind!=Type::Kind::Pointer || !is_integer_base(FROM.base)) bad("inttoptr requires integer -> pointer","source integer, target pointer");
            else { unsigned fw=width(st); if(!(fw==64 || fw==32)) bad("inttoptr integer width must match pointer size (assume 64/32)","use i64 or i32"); }
        }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0505","redefinition of variable","rename destination"); r.success=false; }
        var_types_[dst.substr(1)]=toTy; attach(n,toTy); continue;
    }
    if(op=="fcmp"){ if(il.size()!=7){ error_code(r,*n,"E0120","fcmp arity","expected (fcmp %dst <float-type> :pred <pred> %a %b)"); r.success=false; continue; } std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E0121","fcmp dst must be %var","prefix destination with %"); r.success=false; continue; } TypeId opty=parse_type_node(il[2],r); const Type& T=ctx_.at(opty); if(!(T.kind==Type::Kind::Base && (T.base==BaseType::F32||T.base==BaseType::F64))){ error_code(r,*n,"E0122","fcmp operand type must be float","use f32 or f64"); r.success=false; }
        if(!il[3]||!std::holds_alternative<keyword>(il[3]->data) || std::get<keyword>(il[3]->data).name!="pred"){ error_code(r,*n,"E0123","fcmp expects :pred keyword","insert :pred before predicate"); r.success=false; continue; }
        std::string pred=sym(4); static const std::unordered_set<std::string> fpreds{"oeq","one","olt","ogt","ole","oge","ord","uno","ueq","une","ult","ugt","ule","uge"}; if(!fpreds.count(pred)){ error_code(r,*n,"E0124","unknown fcmp predicate","use one of oeq/one/olt/ogt/ole/oge/ord/uno/ueq/une/ult/ugt/ule/uge"); r.success=false; }
        std::string a=sym(5), b=sym(6); if(a.empty()||b.empty()){ error_code(r,*n,"E0125","fcmp operands required","supply %lhs %rhs"); r.success=false; continue; }
    auto check_operand=[&](const std::string& v){ if(v[0]=='%'){ auto vt=get_var(v.substr(1)); if(vt!=(TypeId)-1 && vt!=opty){ type_mismatch(r,*n,"E0126","fcmp operand",opty,vt); r.success=false; } } else { error_code(r,*n,"E0127","fcmp operand must be %var","prefix operand with %"); r.success=false; } }; check_operand(a); check_operand(b);
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0128","redefinition of variable","rename destination"); r.success=false; }
        TypeId rty=ctx_.get_base(BaseType::I1); var_types_[dst.substr(1)]=rty; attach(n,rty); continue; }
    if(op=="const"){ // (const %dst <type> <literal|EnumConstantSymbol>)
        if(il.size()!=4){ error_code(r,*n,"E1100","const arity","expected (const %dst <type> <literal|EnumConst>)"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1101","const dst must be %var","prefix destination with %"); r.success=false; continue; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1102","redefinition of variable","rename destination"); r.success=false; }
        TypeId ty=parse_type_node(il[2],r);
        // Validate literal / enum constant compatibility
        bool okVal=false; if(std::holds_alternative<int64_t>(il[3]->data)){ okVal=true; }
        else if(std::holds_alternative<double>(il[3]->data)){ // allow float literal only if target is float
            const Type& T=ctx_.at(ty); if(T.kind==Type::Kind::Base && is_float_base(T.base)) okVal=true; else {
                error_code(r,*n,"E1220","const literal type mismatch","float literal requires float target type"); r.success=false; }
        } else if(std::holds_alternative<symbol>(il[3]->data)){
            std::string cname=std::get<symbol>(il[3]->data).name; auto cit=enum_constants_.find(cname); if(cit==enum_constants_.end()){
                size_t before=r.errors.size(); error_code(r,*n,"E1349","unknown enum constant","check enum constant spelling");
                if(before<r.errors.size()){
                    // suggestions from existing enum constants
                    std::vector<std::string> pool; pool.reserve(enum_constants_.size()); for(auto &kv: enum_constants_) pool.push_back(kv.first);
                    append_suggestions(r.errors.back(), fuzzy_candidates(cname,pool));
                }
                r.success=false; okVal=false; 
            } else {
                // Type match: ensure requested type matches constant underlying type (or is implicitly compatible integer base)
                TypeId cty = cit->second.first; const Type& CT = ctx_.at(cty); const Type& TT = ctx_.at(ty);
                if(!(TT.kind==Type::Kind::Base && CT.kind==Type::Kind::Base && is_integer_base(TT.base) && is_integer_base(CT.base))){
                    error_code(r,*n,"E1220","const literal type mismatch","enum constant usable only with integer target type"); r.success=false; }
                else okVal=true;
                // Replace symbol node with actual integer literal node for downstream emitters (desugaring)
                if(okVal){ int64_t v = cit->second.second; il[3] = detail::make_node(v); }
            }
        } else {
            error_code(r,*n,"E1100","const arity","expected (const %dst <type> <literal|EnumConst>)"); r.success=false; }
        if(!okVal){ // fallback attach underlying type anyway to keep metadata consistent
            attach(n,ty); var_types_[dst.substr(1)]=ty; continue; }
        var_types_[dst.substr(1)]=ty; attach(n,ty); continue; }
    if(op=="assign"){ if(il.size()!=3){ error_code(r,*n,"E1103","assign arity","expected (assign %dst %src)"); r.success=false; continue; } std::string v=sym(1), src=sym(2); if(v.empty()||v[0] != '%'){ error_code(r,*n,"E1104","assign target must be %var","prefix target with %"); r.success=false; continue; } if(src.empty()||src[0] != '%'){ error_code(r,*n,"E1105","assign src must be %var","prefix source with %"); r.success=false; continue; } auto vt=get_var(v.substr(1)); if(vt==(TypeId)-1){ error_code(r,*n,"E1106","unknown target var","define destination earlier"); r.success=false; continue; } auto st=get_var(src.substr(1)); if(st!=(TypeId)-1 && st!=vt){ type_mismatch(r,*n,"E1107","assign",vt,st); r.success=false; } continue; }
    if(op=="ret"){ if(il.size()!=3){ error_code(r,*n,"E1010","ret arity","expected (ret <type> %val)"); r.success=false; continue; } TypeId ty=parse_type_node(il[1],r); if(ty!=fn.ret){ type_mismatch(r,*n,"E1011","return",fn.ret,ty); r.success=false; } std::string val=sym(2); if(!val.empty()&&val[0]=='%'){ auto vt=get_var(val.substr(1)); if(vt!=(TypeId)-1 && vt!=ty){ type_mismatch(r,*n,"E1012","return value",ty,vt); r.success=false; } } continue; }
    if(op=="member"){ if(il.size()!=5){ error_code(r,*n,"E0800","member arity","expected (member %dst Struct %base %field)"); r.success=false; continue; } std::string dst=sym(1), stName=sym(2), base=sym(3), field=sym(4); if(dst.empty()||stName.empty()||base.empty()||field.empty()){ error_code(r,*n,"E0801","member expects symbols","supply %dst StructName %base %field"); r.success=false; continue; } auto sit=structs_.find(stName); if(sit==structs_.end()){ error_code(r,*n,"E0802","unknown struct in member","declare struct before use"); r.success=false; continue; } auto fit=sit->second.field_map.find(field); if(fit==sit->second.field_map.end()){ size_t before=r.errors.size(); error_code(r,*n,"E0803","unknown field","check struct field spelling"); if(before<r.errors.size()){ std::vector<std::string> fields; for(auto &f: sit->second.fields) fields.push_back(f.name); auto sugg=fuzzy_candidates(field,fields); append_suggestions(r.errors.back(),sugg); } r.success=false; continue; } if(base[0]=='%'){ auto bt=get_var(base.substr(1)); if(bt==(TypeId)-1){ size_t before=r.errors.size(); error_code(r,*n,"E0804","base undefined","define base variable first"); if(before<r.errors.size()){ // base suggestions: all current vars of compatible struct type names
                std::vector<std::string> cands; for(auto &kv: var_types_){ const Type& VT=ctx_.at(kv.second); bool ok=false; if(VT.kind==Type::Kind::Struct && VT.struct_name==stName) ok=true; else if(VT.kind==Type::Kind::Pointer){ const Type& PT=ctx_.at(VT.pointee); if(PT.kind==Type::Kind::Struct && PT.struct_name==stName) ok=true; } if(ok) cands.push_back("%"+kv.first); }
                append_suggestions(r.errors.back(), fuzzy_candidates(base, cands)); }
                r.success=false; } else { const Type& BT=ctx_.at(bt); bool ok=false; if(BT.kind==Type::Kind::Struct && BT.struct_name==stName) ok=true; else if(BT.kind==Type::Kind::Pointer){ const Type& PT=ctx_.at(BT.pointee); if(PT.kind==Type::Kind::Struct && PT.struct_name==stName) ok=true; } if(!ok){ TypeId expectedStruct = ctx_.get_struct(stName); type_mismatch(r,*n,"E0805","member base", expectedStruct, bt); r.success=false; } } } else { error_code(r,*n,"E0806","base must be %var","prefix with %"); r.success=false; }
        if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0808","redefinition of variable","rename destination"); r.success=false; } var_types_[dst.substr(1)]=fit->second->type; attach(n, fit->second->type);} else { error_code(r,*n,"E0807","member dst must be %var","prefix destination with %"); r.success=false; } continue; }
    if(op=="union-member"){ // (union-member %dst Union %ptr field)
        if(il.size()!=5){ error_code(r,*n,"E1357","union-member arity","expected (union-member %dst Union %ptr field)"); r.success=false; continue; }
        std::string dst=sym(1), uName=sym(2), base=sym(3), field=sym(4);
        if(dst.empty()||uName.empty()||base.empty()||field.empty()){ error_code(r,*n,"E1357","union-member arity","supply %dst UnionName %base %field"); r.success=false; continue; }
        auto uit=unions_.find(uName); if(uit==unions_.end()){ error_code(r,*n,"E1358","unknown union","declare union before use"); r.success=false; continue; }
        auto fit=uit->second.field_map.find(field); if(fit==uit->second.field_map.end()){ size_t before=r.errors.size(); error_code(r,*n,"E1359","unknown union field","check union field name"); if(before<r.errors.size()){ std::vector<std::string> fields; for(auto &f: uit->second.fields) fields.push_back(f.name); append_suggestions(r.errors.back(), fuzzy_candidates(field,fields)); } r.success=false; continue; }
        if(base[0]=='%'){ auto bt=get_var(base.substr(1)); if(bt==(TypeId)-1){ error_code(r,*n,"E1358","unknown union","base variable undefined"); r.success=false; }
            else { const Type& BT=ctx_.at(bt); bool ok=false; if(BT.kind==Type::Kind::Pointer){ const Type& PT=ctx_.at(BT.pointee); if(PT.kind==Type::Kind::Struct && PT.struct_name==uName) ok=true; else if(PT.kind==Type::Kind::Struct && PT.struct_name==uName) ok=true; }
                if(!ok){ error_code(r,*n,"E1358","unknown union","base must be pointer to union struct %"+uName); r.success=false; } }
        } else { error_code(r,*n,"E1357","union-member arity","%base required"); r.success=false; }
        if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1357","union-member arity","destination already defined"); r.success=false; } var_types_[dst.substr(1)]=fit->second->type; attach(n, fit->second->type);} else { error_code(r,*n,"E1357","union-member arity","destination must be %var"); r.success=false; }
        continue; }
    if(op=="member-addr"){ if(il.size()!=5){ error_code(r,*n,"E0810","member-addr arity","expected (member-addr %dst Struct %base %field)"); r.success=false; continue; } std::string dst=sym(1), stName=sym(2), base=sym(3), field=sym(4); if(dst.empty()||stName.empty()||base.empty()||field.empty()){ error_code(r,*n,"E0811","member-addr expects symbols","supply %dst StructName %base %field"); r.success=false; continue; } auto sit=structs_.find(stName); if(sit==structs_.end()){ error_code(r,*n,"E0812","unknown struct in member-addr","declare struct before use"); r.success=false; continue; } auto fit=sit->second.field_map.find(field); if(fit==sit->second.field_map.end()){ error_code(r,*n,"E0813","unknown field","check field name"); r.success=false; continue; } if(base[0]=='%'){ auto bt=get_var(base.substr(1)); if(bt==(TypeId)-1){ error_code(r,*n,"E0814","base undefined","define base variable first"); r.success=false; } else { const Type& BT=ctx_.at(bt); bool ok=false; if(BT.kind==Type::Kind::Pointer){ const Type& PT=ctx_.at(BT.pointee); if(PT.kind==Type::Kind::Struct && PT.struct_name==stName) ok=true; } if(!ok){ error_code(r,*n,"E0815","member-addr base must be pointer to struct","pass pointer (e.g. param (ptr (struct-ref S)))"); r.success=false; } } } else { error_code(r,*n,"E0816","base must be %var","prefix with %"); r.success=false; }
        if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0818","redefinition of variable","rename destination"); r.success=false; } TypeId pty=ctx_.get_pointer(fit->second->type); var_types_[dst.substr(1)]=pty; attach(n, pty);} else { error_code(r,*n,"E0817","member-addr dst must be %var","prefix destination with %"); r.success=false; } continue; }
    if(op=="load"){ if(il.size()!=4){ error_code(r,*n,"E0200","load arity","expected (load %dst <type> %ptr)"); r.success=false; continue; } std::string dst=sym(1), ptr=sym(3); if(dst.empty()||ptr.empty()){ error_code(r,*n,"E0201","load symbols","supply %dst and %ptr"); r.success=false; continue; } TypeId ty=parse_type_node(il[2],r); if(ptr[0]=='%'){ auto pt=get_var(ptr.substr(1)); if(pt==(TypeId)-1){ error_code(r,*n,"E0202","ptr undefined","define pointer variable before load"); r.success=false; } else { const Type& PT=ctx_.at(pt); if(PT.kind!=Type::Kind::Pointer){ error_code(r,*n,"E0203","ptr not pointer to type","pointer must be (ptr <type>)"); r.success=false; } else if(PT.pointee!=ty){ type_mismatch(r,*n,"E0203","load ptr",ctx_.get_pointer(ty),pt); r.success=false; } } } if(dst[0]=='%'){ var_types_[dst.substr(1)]=ty; attach(n,ty);} else { error_code(r,*n,"E0204","load dst must be %var","prefix destination with %"); r.success=false; } continue; }
    if(op=="store"){ if(il.size()!=4){ error_code(r,*n,"E0210","store arity","expected (store <type> %ptr %val)"); r.success=false; continue; } TypeId ty=parse_type_node(il[1],r); std::string ptr=sym(2), val=sym(3); if(ptr.empty()||val.empty()){ error_code(r,*n,"E0211","store symbols","provide %ptr and %val"); r.success=false; continue; } if(ptr[0]=='%'){ auto pt=get_var(ptr.substr(1)); if(pt==(TypeId)-1){ error_code(r,*n,"E0212","ptr undefined","define pointer before store"); r.success=false; } else { const Type& PT=ctx_.at(pt); if(PT.kind!=Type::Kind::Pointer || PT.pointee!=ty){ if(PT.kind==Type::Kind::Pointer) type_mismatch(r,*n,"E0213","store ptr",ty,PT.pointee); else error_code(r,*n,"E0213","store ptr type mismatch","pointer pointee must match <type>"); r.success=false; } } } if(val[0]=='%'){ auto vt=get_var(val.substr(1)); if(vt!=(TypeId)-1 && vt!=ty){ type_mismatch(r,*n,"E0214","store value",ty,vt); r.success=false; } } continue; }
    if(op=="gload"){ if(il.size()!=4){ error_code(r,*n,"E0900","gload arity","expected (gload %dst <type> GlobalName)"); r.success=false; continue;} std::string dst=sym(1), gname=sym(3); if(dst.empty()||gname.empty()){ error_code(r,*n,"E0901","gload symbols","supply %dst and global name"); r.success=false; continue;} TypeId ty=parse_type_node(il[2],r); GlobalInfoTC* gi=nullptr; if(!lookup_global(gname,gi)){ size_t before=r.errors.size(); error_code(r,*n,"E0902","unknown global","declare (global :name ...)"); if(before<r.errors.size()){ std::vector<std::string> names; names.reserve(globals_.size()); for(auto &kv: globals_) names.push_back(kv.first); auto sugg=fuzzy_candidates(gname,names); append_suggestions(r.errors.back(),sugg); } r.success=false; } else if(gi->type!=ty){ type_mismatch(r,*n,"E0903","gload", gi->type, ty); r.success=false; } if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0905","redefinition of variable","rename destination"); r.success=false;} var_types_[dst.substr(1)]=ty; attach(n,ty);} else { error_code(r,*n,"E0904","gload dst must be %var","prefix destination with %"); r.success=false;} continue; }
    if(op=="gstore"){ if(il.size()!=4){ error_code(r,*n,"E0910","gstore arity","expected (gstore <type> GlobalName %val)"); r.success=false; continue;} TypeId ty=parse_type_node(il[1],r); std::string gname=sym(2), val=sym(3); if(gname.empty()||val.empty()){ error_code(r,*n,"E0911","gstore symbols","provide global name and %val"); r.success=false; continue;} GlobalInfoTC* gi=nullptr; if(!lookup_global(gname,gi)){ error_code(r,*n,"E0912","unknown global","declare (global :name ...)"); r.success=false; } else { if(gi->type!=ty){ type_mismatch(r,*n,"E0913","gstore", gi->type, ty); r.success=false; } if(gi->is_const){ error_code(r,*n,"E1226","cannot store to const global","remove :const or avoid mutation"); r.success=false; } } if(val[0]=='%'){ auto vt=get_var(val.substr(1)); if(vt!=(TypeId)-1 && vt!=ty){ type_mismatch(r,*n,"E0914","gstore value", ty, vt); r.success=false; } } else { error_code(r,*n,"E0915","gstore value must be %var","prefix value with %"); r.success=false; } continue; }
    if(op=="index"){ if(il.size()!=5){ error_code(r,*n,"E0820","index arity","expected (index %dst <elem-type> %basePtr %idx)"); r.success=false; continue; } std::string dst=sym(1), base=sym(3), idx=sym(4); if(dst.empty()||base.empty()||idx.empty()){ error_code(r,*n,"E0821","index symbols","supply %dst %base %idx"); r.success=false; continue; } TypeId elem=parse_type_node(il[2],r); if(base[0]=='%'){ auto bt=get_var(base.substr(1)); if(bt==(TypeId)-1){ error_code(r,*n,"E0822","base undefined","define base pointer earlier"); r.success=false; } else { const Type& BT=ctx_.at(bt); if(BT.kind!=Type::Kind::Pointer){ error_code(r,*n,"E0823","index base not pointer","use pointer to array"); r.success=false; } else { const Type& AT=ctx_.at(BT.pointee); if(AT.kind!=Type::Kind::Array){ error_code(r,*n,"E0824","base not pointer to array elem type","ensure pointer to (array ...)"); r.success=false; } else if(AT.elem!=elem){ TypeId expectedPtr = ctx_.get_pointer(ctx_.get_array(elem, AT.array_size)); type_mismatch(r,*n,"E0824","index base", expectedPtr, bt); r.success=false; } } } } if(idx[0]=='%'){ auto it=get_var(idx.substr(1)); if(it!=(TypeId)-1 && !is_int(it)){ error_code(r,*n,"E0825","index must be int","index variable must be integer type"); r.success=false; } } if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0827","redefinition of variable","rename destination"); r.success=false; } var_types_[dst.substr(1)]=ctx_.get_pointer(elem); attach(n, ctx_.get_pointer(elem)); } else { error_code(r,*n,"E0826","index dst must be %var","prefix destination with %"); r.success=false; } continue; }
    if(op=="alloca"){ if(il.size()!=3){ error_code(r,*n,"E1108","alloca arity","expected (alloca %dst <type>)"); r.success=false; continue; } std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1109","alloca dst must be %var","prefix destination with %"); r.success=false; continue; } TypeId ty=parse_type_node(il[2],r); if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1110","redefinition of variable","rename destination"); r.success=false; } TypeId pty=ctx_.get_pointer(ty); var_types_[dst.substr(1)]=pty; attach(n,pty); continue; }
    if(op=="struct-lit"){ if(il.size()!=4){ error_code(r,*n,"E1200","struct-lit arity","expected (struct-lit %dst StructName [ field1 %v1 ... ])"); r.success=false; continue; } std::string dst=sym(1), stName=sym(2); if(dst.empty()||stName.empty()){ error_code(r,*n,"E1201","struct-lit symbols","supply %dst StructName [ ... ]"); r.success=false; continue; } auto sit=structs_.find(stName); if(sit==structs_.end()){ error_code(r,*n,"E1202","unknown struct","declare struct before literal"); r.success=false; continue; } if(!il[3]||!std::holds_alternative<vector_t>(il[3]->data)){ error_code(r,*n,"E1203","struct-lit fields must be vector","wrap field list in [ ]"); r.success=false; continue; } auto &vec=std::get<vector_t>(il[3]->data).elems; if(vec.size()!=sit->second.fields.size()*2){ error_code(r,*n,"E1204","struct-lit field count mismatch","provide name/value pairs for all fields"); r.success=false; } bool orderOk=true; size_t fi=0; for(size_t i=0;i<vec.size(); i+=2,++fi){ if(fi>=sit->second.fields.size()) break; if(!vec[i]||!std::holds_alternative<symbol>(vec[i]->data)){ error_code(r,*n,"E1205","field name must be symbol","use declared field names"); r.success=false; orderOk=false; break; } std::string fname=std::get<symbol>(vec[i]->data).name; if(fname!=sit->second.fields[fi].name){ error_code(r,*n,"E1206","field order/name mismatch","use declared order"); r.success=false; orderOk=false; } if(i+1>=vec.size()||!vec[i+1]||!std::holds_alternative<symbol>(vec[i+1]->data)){ error_code(r,*n,"E1207","field value must be %var symbol","prefix with %"); r.success=false; orderOk=false; continue; } std::string val=std::get<symbol>(vec[i+1]->data).name; if(val.empty()||val[0] != '%'){ error_code(r,*n,"E1207","field value must be %var symbol","prefix with %"); r.success=false; orderOk=false; continue; } auto vt=get_var(val.substr(1)); if(vt!=(TypeId)-1 && vt!=sit->second.fields[fi].type){ type_mismatch(r,*n,"E1208","struct field", sit->second.fields[fi].type, vt); r.success=false; } } if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1209","redefinition of variable","rename destination"); r.success=false; } TypeId sty=ctx_.get_struct(stName); TypeId pty=ctx_.get_pointer(sty); var_types_[dst.substr(1)]=pty; attach(n,pty);} else { error_code(r,*n,"E1201","struct-lit symbols","destination must be %var"); r.success=false; } continue; }
    if(op=="array-lit"){ if(il.size()!=5){ error_code(r,*n,"E1210","array-lit arity","expected (array-lit %dst <elem-type> <size> [ %e0 %e1 ... ])"); r.success=false; continue; } std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1211","array-lit dst must be %var","prefix destination with %"); r.success=false; continue; } TypeId elem=parse_type_node(il[2],r); if(!il[3]||!std::holds_alternative<int64_t>(il[3]->data)){ error_code(r,*n,"E1212","array-lit size int","size must be integer literal"); r.success=false; continue; } uint64_t asz=(uint64_t)std::get<int64_t>(il[3]->data); if(asz==0){ error_code(r,*n,"E1213","array size > 0","use positive size"); r.success=false; } if(!il[4]||!std::holds_alternative<vector_t>(il[4]->data)){ error_code(r,*n,"E1214","array-lit elems must be vector","wrap elements in [ ]"); r.success=false; continue; } auto &elemsVec=std::get<vector_t>(il[4]->data).elems; if(elemsVec.size()!=asz){ error_code(r,*n,"E1215","array-lit count mismatch","element count must equal declared size"); r.success=false; } for(auto &e: elemsVec){ if(!e||!std::holds_alternative<symbol>(e->data)){ error_code(r,*n,"E1216","array elem must be %var","prefix each element with %"); r.success=false; continue; } std::string v=std::get<symbol>(e->data).name; if(v.empty()||v[0] != '%'){ error_code(r,*n,"E1216","array elem must be %var","prefix each element with %"); r.success=false; continue; } auto vt=get_var(v.substr(1)); if(vt!=(TypeId)-1 && vt!=elem){ type_mismatch(r,*n,"E1217","array elem", elem, vt); r.success=false; } } if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1218","redefinition of variable","rename destination"); r.success=false; } TypeId aty=ctx_.get_array(elem, asz); TypeId pty=ctx_.get_pointer(aty); var_types_[dst.substr(1)]=pty; attach(n,pty); } else { error_code(r,*n,"E1211","array-lit dst must be %var","prefix destination with %"); r.success=false; } continue; }
    if(op=="phi"){ // (phi %dst <type> [ (%val %label) ... ])
        if(il.size()!=4){ error_code(r,*n,"E0300","phi arity","expected (phi %dst <type> [ (%v %label) ... ])"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E0301","phi dst must be %var","use %name for SSA value"); r.success=false; continue; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0302","redefinition of variable","rename phi destination"); r.success=false; continue; }
        TypeId ty=parse_type_node(il[2],r);
        if(!il[3] || !std::holds_alternative<vector_t>(il[3]->data)){ error_code(r,*n,"E0303","phi incoming list must be vector","wrap incoming pairs in [ ... ]"); r.success=false; continue; }
        auto &incVec = std::get<vector_t>(il[3]->data).elems; if(incVec.size()<2){ error_code(r,*n,"E0304","phi requires at least two incoming values","provide at least two (%val %label) pairs"); r.success=false; }
    for(auto &inc : incVec){ if(!inc || !std::holds_alternative<list>(inc->data)){ error_code(r,*n,"E0305","phi incoming entry must be list","form is (%val %label)"); r.success=false; continue; } auto &pl = std::get<list>(inc->data).elems; if(pl.size()!=2){ error_code(r,*inc,"E0306","phi incoming entry arity","need exactly (%val %label)"); r.success=false; continue; } if(!std::holds_alternative<symbol>(pl[0]->data) || !std::holds_alternative<symbol>(pl[1]->data)){ error_code(r,*inc,"E0307","phi incoming expects (%val %label)",""); r.success=false; continue; } std::string v = std::get<symbol>(pl[0]->data).name; if(v.empty()||v[0] != '%'){ error_code(r,*inc,"E0308","phi incoming value must be %var","prefix with %"); r.success=false; continue; } auto vt = get_var(v.substr(1)); if(vt!=(TypeId)-1 && vt!=ty){
        // structured mismatch notes
        ErrorReporter rep{&r.errors,&r.warnings};
        auto err = rep.make_error("E0309","phi incoming value type mismatch","match all incoming value types to phi type", line(*inc), col(*inc));
        err.notes.push_back(TypeNote{"expected: "+ctx_.to_string(ty), line(*inc), col(*inc)});
        err.notes.push_back(TypeNote{"   found: "+ctx_.to_string(vt), line(*inc), col(*inc)});
        rep.emit_error(err); r.success=false; } }
        var_types_[dst.substr(1)] = ty; attach(n,ty); continue; }
    if(op=="call"){ if(il.size()<4){ error_code(r,*n,"E0400","call arity","expected (call %dst <ret-type> name %args...) "); r.success=false; continue; } std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E0401","call dst must be %var","prefix destination with %"); r.success=false; continue; } TypeId ret=parse_type_node(il[2],r); std::string callee=sym(3); if(callee.empty()){ error_code(r,*n,"E0402","call callee symbol required","supply function name after return type"); r.success=false; continue; } FunctionInfoTC* finfo=nullptr; if(!lookup_function(callee, finfo)){ error_code(r,*n,"E0403","unknown callee","define function first"); r.success=false; } if(finfo && ret!=finfo->ret){ error_code(r,*n,"E0404","call return type mismatch","callee returns different type"); r.success=false; } if(finfo){ size_t declared=finfo->params.size(); size_t provided= (il.size()>4)? il.size()-4:0; if(!finfo->variadic){ if(provided!=declared){ error_code(r,*n,"E0405","call arg count mismatch","expected "+std::to_string(declared)+" got "+std::to_string(provided)); r.success=false; } }
            else { if(provided < declared){ error_code(r,*n,"E1360","variadic missing required args","requires "+std::to_string(declared)+" fixed args, got "+std::to_string(provided)); r.success=false; } }
            size_t checkN = std::min(declared, provided); for(size_t ai=0; ai<checkN; ++ai){ std::string av=sym(4+ai); if(av.empty()||av[0] != '%'){ error_code(r,*n,"E0406","call arg must be %var","prefix each arg with %"); r.success=false; continue; } auto at=get_var(av.substr(1)); if(at==(TypeId)-1){ size_t before=r.errors.size(); error_code(r,*n,"E0407","unknown arg var","define arg value earlier"); if(before<r.errors.size()){ std::vector<std::string> vars; for(auto &kv: var_types_) if(kv.second==finfo->params[ai].type) vars.push_back("%"+kv.first); append_suggestions(r.errors.back(), fuzzy_candidates(av, vars)); } r.success=false; continue; } if(at!=finfo->params[ai].type){ type_mismatch(r,*n,"E0408","call arg",finfo->params[ai].type,at); r.success=false; } }
            // Extra variadic args (if any) left untyped beyond ensuring they are %vars; could add future type classification.
            if(finfo->variadic && provided>declared){ for(size_t ai=declared; ai<provided; ++ai){ std::string av=sym(4+ai); if(av.empty()||av[0] != '%'){ error_code(r,*n,"E1361","variadic arg must be %var","prefix each variadic arg with %"); r.success=false; } else { auto at=get_var(av.substr(1)); if(at==(TypeId)-1){ size_t before=r.errors.size(); error_code(r,*n,"E1362","variadic arg undefined","define argument earlier"); if(before<r.errors.size()){ // suggestions: all currently known vars not already used
                            std::vector<std::string> cands; for(auto &kv: var_types_) cands.push_back("%"+kv.first); append_suggestions(r.errors.back(), fuzzy_candidates(av, cands)); }
                        r.success=false; } } } }
        }
    if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0405","redefinition of variable","rename destination"); r.success=false; }
        var_types_[dst.substr(1)]=ret; attach(n,ret); continue; }
    // --- Vararg intrinsics (Phase 3 extension) ---
    if(op=="va-start"){ // (va-start %ap)
        if(il.size()!=2){ error_code(r,*n,"E1363","va-start arity","expected (va-start %ap)"); r.success=false; continue; }
        if(!fn.variadic){ error_code(r,*n,"E1364","va-start only in variadic fn","declare function with :vararg true"); r.success=false; continue; }
        std::string ap=sym(1); if(ap.empty()||ap[0] != '%'){ error_code(r,*n,"E1363","va-start arity","destination must be %var"); r.success=false; continue; }
        if(var_types_.count(ap.substr(1))){ error_code(r,*n,"E1363","va-start arity","%ap already defined"); r.success=false; continue; }
        // Represent va_list as i8* (pointer to i8)
        TypeId apTy = ctx_.get_pointer(ctx_.get_base(BaseType::I8));
        var_types_[ap.substr(1)] = apTy; attach(n, apTy); continue; }
    if(op=="va-arg"){ // (va-arg %dst <type> %ap)
        if(il.size()!=4){ error_code(r,*n,"E1365","va-arg arity","expected (va-arg %dst <type> %ap)"); r.success=false; continue; }
        if(!fn.variadic){ error_code(r,*n,"E1366","va-arg only in variadic fn","declare function with :vararg true"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1365","va-arg arity","%dst required"); r.success=false; continue; }
        TypeId ty=parse_type_node(il[2],r); std::string ap=sym(3); if(ap.empty()||ap[0] != '%'){ error_code(r,*n,"E1367","va-arg ap must be %var","prefix va-list variable with %"); r.success=false; continue; }
        auto apt=get_var(ap.substr(1)); if(apt==(TypeId)-1){ error_code(r,*n,"E1367","va-arg ap must be %var","va-list variable undefined"); r.success=false; }
        else { const Type& AT=ctx_.at(apt); const Type& I8=ctx_.at(ctx_.get_base(BaseType::I8)); bool ok=false; if(AT.kind==Type::Kind::Pointer){ const Type& PT=ctx_.at(AT.pointee); if(PT.kind==Type::Kind::Base && PT.base==I8.base) ok=true; }
            if(!ok){ error_code(r,*n,"E1367","va-arg ap must be %var","va-list must have type i8*"); r.success=false; }
        }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1365","va-arg arity","destination already defined"); r.success=false; }
        var_types_[dst.substr(1)] = ty; attach(n,ty); continue; }
    if(op=="va-end"){ // (va-end %ap)
        if(il.size()!=2){ error_code(r,*n,"E1368","va-end arity","expected (va-end %ap)"); r.success=false; continue; }
        if(!fn.variadic){ error_code(r,*n,"E1369","va-end only in variadic fn","declare function with :vararg true"); r.success=false; continue; }
        std::string ap=sym(1); if(ap.empty()||ap[0] != '%'){ error_code(r,*n,"E1368","va-end arity","%ap required"); r.success=false; continue; }
        auto apt=get_var(ap.substr(1)); if(apt==(TypeId)-1){ error_code(r,*n,"E1368","va-end arity","va-list undefined"); r.success=false; }
        continue; }
    error(r,*n,"unknown instruction"); r.success=false; }
}
// (removed stray extra brace that previously closed namespace early)
inline TypeCheckResult TypeChecker::check_module(const node_ptr& m){ TypeCheckResult res{true,{},{}}; if(!m||!std::holds_alternative<list>(m->data)){ res.success=false; res.errors.push_back(TypeError{"EMOD1","module not list","",-1,-1,{}}); return res; } auto &l=std::get<list>(m->data).elems; if(l.empty()||!std::holds_alternative<symbol>(l[0]->data) || std::get<symbol>(l[0]->data).name!="module"){ res.success=false; res.errors.push_back(TypeError{"EMOD2","expected (module ...)","start file with (module ...)",-1,-1,{}}); return res; } reset(); collect_typedefs(res,l); collect_enums(res,l); collect_structs(res,l); collect_unions(res,l); collect_globals(res,l); collect_functions_headers(res,l); if(res.success) check_functions(res,l); return res; }

} // namespace edn
