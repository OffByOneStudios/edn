// Clean implementation for type checker including blocks, assign, control flow
#pragma once
#include "type_check.hpp"
#include <algorithm>
#include <functional>

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
inline void TypeChecker::reset(){ structs_.clear(); unions_.clear(); sums_.clear(); functions_.clear(); globals_.clear(); var_types_.clear(); used_globals_.clear(); }
inline void TypeChecker::collect_typedefs(TypeCheckResult& r, const std::vector<node_ptr>& elems){
    for(size_t i=1;i<elems.size(); ++i){
        auto &n = elems[i]; if(!n||!std::holds_alternative<list>(n->data)) continue;
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
        if(typedefs_.count(name) || structs_.count(name) || sums_.count(name) || unions_.count(name) || enums_.count(name)){
            error_code(r,*n,"E1332","typedef redefinition","choose unique alias name"); r.success=false; continue; }
        // Simple cycle / self-reference detection: (typedef :name X :type X)
        if(typeNode){
            bool selfRef=false;
            if(std::holds_alternative<symbol>(typeNode->data)){
                if(std::get<symbol>(typeNode->data).name==name) selfRef=true;
            } else if(std::holds_alternative<std::string>(typeNode->data)){
                if(std::get<std::string>(typeNode->data)==name) selfRef=true;
            }
            if(selfRef){ error_code(r,*n,"E1333","typedef type form invalid","self-referential cycle"); r.success=false; continue; }
        }
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
inline void TypeChecker::collect_sums(TypeCheckResult& r, const std::vector<node_ptr>& elems){
    // (sum :name T :variants [ (variant :name A :fields [ <types>* ]) ... ])
    for(size_t i=1;i<elems.size(); ++i){
        auto &n=elems[i]; if(!n||!std::holds_alternative<list>(n->data)) continue; auto &l=std::get<list>(n->data).elems; if(l.empty()||!std::holds_alternative<symbol>(l[0]->data)) continue; if(std::get<symbol>(l[0]->data).name!="sum") continue;
        std::string name; node_ptr variantsNode; bool haveName=false, haveVariants=false;
        for(size_t j=1;j<l.size(); ++j){ if(!l[j]||!std::holds_alternative<keyword>(l[j]->data)) break; std::string kw=std::get<keyword>(l[j]->data).name; if(++j>=l.size()) break; auto val=l[j];
            if(kw=="name"){ if(std::holds_alternative<symbol>(val->data)) { name=std::get<symbol>(val->data).name; haveName=true; } else if(std::holds_alternative<std::string>(val->data)) { name=std::get<std::string>(val->data); haveName=true; } }
            else if(kw=="variants"){ variantsNode=val; haveVariants=true; }
        }
        if(!haveName){ error_code(r,*n,"E1400","sum missing :name","provide (sum :name T ...) "); r.success=false; continue; }
        if(sums_.count(name) || structs_.count(name) || unions_.count(name) || enums_.count(name) || typedefs_.count(name)){
            error_code(r,*n,"E1401","sum redefinition","choose unique sum type name"); r.success=false; continue; }
        if(!haveVariants || !variantsNode || !std::holds_alternative<vector_t>(variantsNode->data)){
            error_code(r,*n,"E1402","sum missing :variants","add :variants [ (variant :name A :fields [ ... ])* ]"); r.success=false; continue; }
        SumInfo info; info.name=name; std::unordered_set<std::string> vnames;
        for(auto &vn : std::get<vector_t>(variantsNode->data).elems){
            if(!vn||!std::holds_alternative<list>(vn->data)) { error_code(r,*n,"E1403","variant malformed","use (variant :name A :fields [ <types>* ])"); r.success=false; continue; }
            auto &vl = std::get<list>(vn->data).elems; if(vl.empty()||!std::holds_alternative<symbol>(vl[0]->data) || std::get<symbol>(vl[0]->data).name!="variant"){
                error_code(r,*vn,"E1403","variant malformed","starts with variant symbol"); r.success=false; continue; }
            std::string vname; node_ptr fieldsNode; bool haveVName=false, haveFields=false;
            for(size_t k=1;k<vl.size(); ++k){ if(!vl[k]||!std::holds_alternative<keyword>(vl[k]->data)) break; std::string kw=std::get<keyword>(vl[k]->data).name; if(++k>=vl.size()) break; auto val=vl[k];
                if(kw=="name"){ if(std::holds_alternative<symbol>(val->data)) { vname=std::get<symbol>(val->data).name; haveVName=true; } else if(std::holds_alternative<std::string>(val->data)){ vname=std::get<std::string>(val->data); haveVName=true; } }
                else if(kw=="fields"){ fieldsNode=val; haveFields=true; }
            }
            if(!haveVName){ error_code(r,*vn,"E1404","variant missing :name","provide (variant :name A ...) "); r.success=false; continue; }
            if(vnames.count(vname)){ error_code(r,*vn,"E1405","duplicate variant","rename variant"); r.success=false; continue; }
            std::vector<TypeId> ftys; if(haveFields){ if(!fieldsNode || !std::holds_alternative<vector_t>(fieldsNode->data)){
                    error_code(r,*vn,"E1406","variant :fields must be vector","use [ <types>* ]"); r.success=false; }
                else { for(auto &tf : std::get<vector_t>(fieldsNode->data).elems){ try{ ftys.push_back(ctx_.parse_type(tf)); } catch(const parse_error&){ error_code(r,*tf,"E1407","variant field type invalid","fix type form"); r.success=false; } } }
            }
            vnames.insert(vname); info.variants.push_back(SumVariantInfo{vname, std::move(ftys)});
        }
        for(auto &v : info.variants) info.variant_map[v.name]=&v;
        if(info.variants.empty()){ error_code(r,*n,"E1408","sum has no variants","add at least one variant"); r.success=false; continue; }
        sums_[name]=std::move(info);
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
        // Early generic error metadata harvesting from module head
        if(i==0 && n && std::holds_alternative<symbol>(n->data)){
            for(auto &kv : n->metadata){
                if(kv.first.rfind("generic-error-",0)==0 && kv.second && std::holds_alternative<symbol>(kv.second->data)){
                    std::string payload = std::get<symbol>(kv.second->data).name; // CODE:msg
                    auto pos = payload.find(':');
                    std::string code = pos==std::string::npos?payload:payload.substr(0,pos);
                    std::string msg = pos==std::string::npos?"generic diagnostic":payload.substr(pos+1);
                    if(code.rfind("E17",0)==0){ error_code(r,*n,code,msg,"generic instantiation error"); r.success=false; }
                }
            }
        }
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
            auto addMismatch=[&](const std::string& code, const std::string& role [[maybe_unused]], TypeId expected, const std::string& msg, const std::string& hint){
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
inline bool TypeChecker::parse_struct(TypeCheckResult& r, const node_ptr& n){
    // Recognize (struct :name S :fields [ (field :name x :type i32) ... ])
    if(!n||!std::holds_alternative<list>(n->data)) return false; auto &l=std::get<list>(n->data).elems; if(l.empty()) return false; if(!std::holds_alternative<symbol>(l[0]->data)||std::get<symbol>(l[0]->data).name!="struct") return false;
    std::string name; bool haveFields=false; std::vector<FieldInfo> fields; std::unordered_set<std::string> names;
    for(size_t i=1;i<l.size(); ++i){
        if(!l[i] || !std::holds_alternative<keyword>(l[i]->data)) break; std::string kw=std::get<keyword>(l[i]->data).name; if(++i>=l.size()) break; auto val=l[i];
        if(kw=="name"){
            if(std::holds_alternative<symbol>(val->data)) name=std::get<symbol>(val->data).name;
            else if(std::holds_alternative<std::string>(val->data)) name=std::get<std::string>(val->data);
            else { error_code(r,*val,"E1400","struct missing :name","provide (struct :name S :fields [...])"); r.success=false; }
        } else if(kw=="fields"){
            haveFields=true; if(!val||!std::holds_alternative<vector_t>(val->data)){ error_code(r,*val,"E1401","struct missing :fields","add :fields [ (field :name a :type i32) ... ]"); r.success=false; continue; }
            auto &vec=std::get<vector_t>(val->data).elems; size_t idx=0; if(vec.empty()){ error_code(r,*val,"E1407","struct empty :fields","add at least one field"); r.success=false; }
            for(auto &f: vec){
                if(!f||!std::holds_alternative<list>(f->data)){ error_code(r,*val,"E1402","struct field malformed","use (field :name a :type i32)"); r.success=false; continue; }
                auto &fl=std::get<list>(f->data).elems; std::string fname; TypeId fty=0; for(size_t k=0;k<fl.size(); ++k){
                    if(fl[k] && std::holds_alternative<keyword>(fl[k]->data)){
                        std::string fkw=std::get<keyword>(fl[k]->data).name; if(++k>=fl.size()) break; auto v=fl[k];
                        if(fkw=="name"){
                            if(std::holds_alternative<symbol>(v->data)) fname=std::get<symbol>(v->data).name; else { error_code(r,*v,"E1403","struct field missing name","field :name expects symbol"); r.success=false; }
                        } else if(fkw=="type"){
                            fty=parse_type_node(v,r); if(!fty){ error_code(r,*v,"E1404","struct field type invalid","fix :type form"); r.success=false; }
                        }
                    }
                }
                if(fname.empty()){ error_code(r,*f,"E1403","struct field missing name","add (field :name a :type i32)"); r.success=false; continue; }
                if(names.count(fname)){ error_code(r,*f,"E1405","struct duplicate field","rename field"); r.success=false; continue; }
                names.insert(fname);
                if(!fty){ error_code(r,*f,"E1404","struct field type invalid","add :type <type>"); r.success=false; }
                fields.push_back(FieldInfo{fname,fty,idx++});
            }
        }
    }
    if(name.empty()){ error_code(r,*n,"E1400","struct missing :name","provide (struct :name S :fields [...])"); r.success=false; return true; }
    if(!haveFields){ error_code(r,*n,"E1401","struct missing :fields","add :fields [ (field :name a :type i32) ... ]"); r.success=false; }
    if(structs_.count(name)){ error_code(r,*n,"E1406","struct redefinition","choose unique struct name"); r.success=false; }
    StructInfo si; si.name=name; si.fields=std::move(fields); for(auto &f: si.fields) si.field_map[f.name]=&f; structs_[name]=std::move(si); return true; }
inline bool TypeChecker::parse_function_header(TypeCheckResult& r, const node_ptr& fn, FunctionInfoTC& out_fn){ if(!fn||!std::holds_alternative<list>(fn->data)) return false; auto &fl=std::get<list>(fn->data).elems; if(fl.empty()) return false; if(!std::holds_alternative<symbol>(fl[0]->data)||std::get<symbol>(fl[0]->data).name!="fn") return false; std::string name; TypeId ret=ctx_.get_base(BaseType::Void); std::vector<ParamInfoTC> params; bool variadic=false; bool external=false; for(size_t i=1;i<fl.size(); ++i){ if(fl[i] && std::holds_alternative<keyword>(fl[i]->data)){ std::string kw=std::get<keyword>(fl[i]->data).name; if(++i>=fl.size()) break; auto val=fl[i]; if(kw=="name"){ if(std::holds_alternative<std::string>(val->data)) name=std::get<std::string>(val->data); } else if(kw=="ret"){ ret=parse_type_node(val,r); } else if(kw=="params"){ if(val && std::holds_alternative<vector_t>(val->data)){ for(auto &p: std::get<vector_t>(val->data).elems){ if(!p||!std::holds_alternative<list>(p->data)) continue; auto &pl=std::get<list>(p->data).elems; if(pl.size()==3 && std::holds_alternative<symbol>(pl[0]->data) && std::get<symbol>(pl[0]->data).name=="param"){ TypeId pty=parse_type_node(pl[1],r); std::string v; if(std::holds_alternative<symbol>(pl[2]->data)){ v=std::get<symbol>(pl[2]->data).name; if(!v.empty()&&v[0]=='%') v.erase(0,1);} params.push_back(ParamInfoTC{v,pty}); } } } } else if(kw=="vararg"){ if(val && std::holds_alternative<bool>(val->data)) variadic=std::get<bool>(val->data); else { error(r,*val,":vararg expects bool"); r.success=false; } } else if(kw=="external"){ if(val && std::holds_alternative<bool>(val->data)) external=std::get<bool>(val->data); else { error(r,*val,":external expects bool"); r.success=false; } } } } if(name.empty()){ r.success=false; error(r,*fn,"function missing :name"); } out_fn.name=name; out_fn.ret=ret; out_fn.params=std::move(params); out_fn.variadic=variadic; out_fn.external=external; return true; }
inline void TypeChecker::collect_structs(TypeCheckResult& r, const std::vector<node_ptr>& elems){ for(size_t i=1;i<elems.size(); ++i) parse_struct(r, elems[i]); }
inline void TypeChecker::collect_functions_headers(TypeCheckResult& r, const std::vector<node_ptr>& elems){ for(size_t i=1;i<elems.size(); ++i){ FunctionInfoTC fi; if(parse_function_header(r, elems[i], fi)){ if(functions_.count(fi.name)){ error(r,*elems[i],"duplicate function name"); r.success=false; } else functions_[fi.name]=fi; } } }
inline void TypeChecker::check_functions(TypeCheckResult& r, const std::vector<node_ptr>& elems){ for(size_t i=1;i<elems.size(); ++i){ auto &n=elems[i]; if(!n||!std::holds_alternative<list>(n->data)) continue; auto &fl=std::get<list>(n->data).elems; if(fl.empty()||!std::holds_alternative<symbol>(fl[0]->data) || std::get<symbol>(fl[0]->data).name!="fn") continue; std::string fname; bool isExternal=false; for(size_t j=1;j<fl.size(); ++j){ if(fl[j] && std::holds_alternative<keyword>(fl[j]->data)){ std::string kw=std::get<keyword>(fl[j]->data).name; if(++j>=fl.size()) break; if(kw=="name" && std::holds_alternative<std::string>(fl[j]->data)) fname=std::get<std::string>(fl[j]->data); else if(kw=="external" && std::holds_alternative<bool>(fl[j]->data)) isExternal=std::get<bool>(fl[j]->data); } } if(fname.empty()) continue; auto it=functions_.find(fname); if(it!=functions_.end()){ if(it->second.external || isExternal) continue; check_function_body(r,n,it->second); } } }
inline void TypeChecker::check_function_body(TypeCheckResult& r, const node_ptr& fn, const FunctionInfoTC& info){ var_types_.clear(); for(auto &p: info.params) var_types_[p.name]=p.type; auto &fl=std::get<list>(fn->data).elems; node_ptr body; for(size_t i=1;i<fl.size(); ++i){ if(fl[i] && std::holds_alternative<keyword>(fl[i]->data) && std::get<keyword>(fl[i]->data).name=="body"){ if(++i<fl.size()) body=fl[i]; break; } } if(!body || !std::holds_alternative<vector_t>(body->data)){ error(r,*fn,":body missing or not vector"); r.success=false; return; } auto &insts = std::get<vector_t>(body->data).elems; check_instruction_list(r, insts, info, 0); analyze_fn_lints(r, insts, info); }

inline void TypeChecker::analyze_fn_lints(TypeCheckResult& r, const std::vector<node_ptr>& insts, const FunctionInfoTC& fn){
    // Gate lints behind EDN_LINT=1 (default on if unset to encourage early hygiene)
    if(const char* lintEnv = std::getenv("EDN_LINT")){ if(lintEnv[0]=='0') return; }
    ErrorReporter rep{&r.errors,&r.warnings};
    // W1400: unreachable instructions after a top-level ret in function body
    bool sawRet=false; for(auto &n : insts){ if(!n || !std::holds_alternative<list>(n->data)) continue; auto &il = std::get<list>(n->data).elems; if(il.empty()||!std::holds_alternative<symbol>(il[0]->data)) continue; std::string op = std::get<symbol>(il[0]->data).name; if(op=="ret"){ sawRet=true; continue; } if(sawRet){
            rep.emit_warning(rep.make_warning("W1400","unreachable instruction after return","reorder or remove dead code", line(*n), col(*n)));
        }
    }
    // Helper: recursively scan nested vectors for reachability and usage
    std::function<void(const std::vector<node_ptr>&, bool&)> scan;
    // Track uses of %vars and declared locals/SSA defs to catch unused
    std::unordered_set<std::string> defined;
    std::unordered_set<std::string> used;
    // Seed with parameters
    for(const auto& p : fn.params) if(!p.name.empty()) defined.insert(p.name);
    // Seed with any already defined in var_types_ at this point (top-level defs from body processed earlier)
    for(const auto& kv : var_types_) defined.insert(kv.first);
    scan = [&](const std::vector<node_ptr>& body, bool& reachable){
        for(size_t i=0;i<body.size();++i){ auto &n = body[i]; if(!n || !std::holds_alternative<list>(n->data)) continue; auto &il = std::get<list>(n->data).elems; if(il.empty()||!std::holds_alternative<symbol>(il[0]->data)) continue; std::string op = std::get<symbol>(il[0]->data).name;
            auto symAt=[&](size_t idx)->std::string{ if(idx<il.size() && std::holds_alternative<symbol>(il[idx]->data)) return std::get<symbol>(il[idx]->data).name; return std::string{}; };
            auto noteUse=[&](const std::string& s){ if(!s.empty()&&s[0]=='%') used.insert(s.substr(1)); };
            auto noteDef=[&](const std::string& s){ if(!s.empty()&&s[0]=='%') defined.insert(s.substr(1)); };
            if(!reachable){ rep.emit_warning(rep.make_warning("W1402","unreachable code","code cannot execute after terminator", line(*n), col(*n))); }
            // Collect defs/uses for a few ops
            if(op=="const"||op=="alloca"||op=="add"||op=="sub"||op=="mul"||op=="sdiv"||op=="udiv"||op=="srem"||op=="urem"||op=="and"||op=="or"||op=="xor"||op=="shl"||op=="lshr"||op=="ashr"||op=="icmp"||op=="fcmp"||op=="fadd"||op=="fsub"||op=="fmul"||op=="fdiv"||op=="load"||op=="phi"||op=="member"||op=="member-addr"||op=="sum-new"||op=="sum-is"||op=="sum-get"||op=="array-lit"||op=="struct-lit"||op=="call"||op=="call-indirect"||op=="addr"||op=="deref"||op=="index"||op=="gload"||op=="va-start"||op=="va-arg"||op=="panic"||op=="cstr"||op=="bytes"){
                if(il.size()>=2) noteDef(symAt(1));
                for(size_t k=2;k<il.size();++k){ if(std::holds_alternative<symbol>(il[k]->data)) noteUse(std::get<symbol>(il[k]->data).name); }
            } else if(op=="assign"){ if(il.size()>=3){ noteUse(symAt(2)); noteUse(symAt(1)); } }
            // Control-flow and reachability
            if(op=="ret"){ reachable=false; continue; }
            if(op=="panic"){ // terminator: aborts
                reachable=false; continue;
            }
            if(op=="break"||op=="continue"){ reachable=false; continue; }
            if(op=="block"){ // scan :body
                for(size_t j=1;j<il.size();++j){ if(!il[j]||!std::holds_alternative<keyword>(il[j]->data)) break; std::string kw=std::get<keyword>(il[j]->data).name; if(++j>=il.size()) break; auto val=il[j]; if(kw=="locals"){ if(val && std::holds_alternative<vector_t>(val->data)){ for(auto &d: std::get<vector_t>(val->data).elems){ if(!d||!std::holds_alternative<list>(d->data)) continue; auto &dl=std::get<list>(d->data).elems; if(dl.size()==3 && std::holds_alternative<symbol>(dl[0]->data) && std::get<symbol>(dl[0]->data).name=="local"){ if(std::holds_alternative<symbol>(dl[2]->data)){ std::string vn=std::get<symbol>(dl[2]->data).name; if(!vn.empty()&&vn[0]=='%') vn.erase(0,1); defined.insert(vn); } } } } } else if(kw=="body"){ if(val && std::holds_alternative<vector_t>(val->data)){ bool subReach=true; scan(std::get<vector_t>(val->data).elems, subReach); if(!subReach) reachable=false; } } }
                continue;
            }
            if(op=="if"){ bool thenReach=true, elseReach=true; if(il.size()>=2) noteUse(symAt(1)); if(il.size()>=3 && il[2] && std::holds_alternative<vector_t>(il[2]->data)) scan(std::get<vector_t>(il[2]->data).elems, thenReach); if(il.size()>=4 && il[3] && std::holds_alternative<vector_t>(il[3]->data)) scan(std::get<vector_t>(il[3]->data).elems, elseReach); reachable = thenReach || elseReach; continue; }
            if(op=="try"){ // (try :body [ ... ] :catch [ ... ])
                // Descend into both body and catch to track uses and reachability.
                node_ptr bodyNode=nullptr, catchNode=nullptr; bool haveBody=false, haveCatch=false;
                for(size_t j=1;j<il.size(); ++j){
                    if(!il[j] || !std::holds_alternative<keyword>(il[j]->data)) break;
                    std::string kw = std::get<keyword>(il[j]->data).name; if(++j>=il.size()) break; auto val = il[j];
                    if(kw=="body" && val && std::holds_alternative<vector_t>(val->data)){ bodyNode=val; haveBody=true; }
                    else if(kw=="catch" && val && std::holds_alternative<vector_t>(val->data)){ catchNode=val; haveCatch=true; }
                }
                bool bodyReach=true, catchReach=true;
                if(haveBody) scan(std::get<vector_t>(bodyNode->data).elems, bodyReach);
                if(haveCatch) scan(std::get<vector_t>(catchNode->data).elems, catchReach);
                reachable = bodyReach || catchReach; // control continues if any arm can continue
                continue;
            }
            if(op=="while"){ if(il.size()>=2) noteUse(symAt(1)); // while may loop 0 times -> reachable continues
                if(il.size()>=3 && il[2] && std::holds_alternative<vector_t>(il[2]->data)){ bool bodyReach=true; scan(std::get<vector_t>(il[2]->data).elems, bodyReach); }
                continue; }
            if(op=="for"){ std::vector<node_ptr> initVec, stepVec, bodyVec; std::string condVar; for(size_t j=1;j<il.size();++j){ if(!il[j]||!std::holds_alternative<keyword>(il[j]->data)) break; std::string kw=std::get<keyword>(il[j]->data).name; if(++j>=il.size()) break; auto val=il[j]; if(kw=="init"){ if(val && std::holds_alternative<vector_t>(val->data)) initVec=std::get<vector_t>(val->data).elems; } else if(kw=="cond"){ condVar=symAt(j); } else if(kw=="step"){ if(val && std::holds_alternative<vector_t>(val->data)) stepVec=std::get<vector_t>(val->data).elems; } else if(kw=="body"){ if(val && std::holds_alternative<vector_t>(val->data)) bodyVec=std::get<vector_t>(val->data).elems; } }
                if(!condVar.empty()) noteUse(condVar);
                if(!initVec.empty()){ bool rch=true; scan(initVec, rch); }
                if(!bodyVec.empty()){ bool rch=true; scan(bodyVec, rch); }
                if(!stepVec.empty()){ bool rch=true; scan(stepVec, rch); }
                continue; }
            if(op=="switch"){ if(il.size()>=2) noteUse(symAt(1)); // cases and default
                bool anyReach=false; node_ptr casesNode=nullptr, defaultNode=nullptr; for(size_t j=2;j<il.size();++j){ if(!il[j]||!std::holds_alternative<keyword>(il[j]->data)) break; std::string kw=std::get<keyword>(il[j]->data).name; if(++j>=il.size()) break; auto val=il[j]; if(kw=="cases") casesNode=val; else if(kw=="default") defaultNode=val; }
                if(casesNode && std::holds_alternative<vector_t>(casesNode->data)){ for(auto &cv : std::get<vector_t>(casesNode->data).elems){ if(!cv||!std::holds_alternative<list>(cv->data)) continue; auto &cl=std::get<list>(cv->data).elems; if(cl.size()>=3 && std::holds_alternative<vector_t>(cl[2]->data)){ bool rch=true; scan(std::get<vector_t>(cl[2]->data).elems, rch); anyReach |= rch; } } }
                if(defaultNode && std::holds_alternative<vector_t>(defaultNode->data)){ bool rch=true; scan(std::get<vector_t>(defaultNode->data).elems, rch); anyReach |= rch; }
                reachable = anyReach; continue; }
            if(op=="rtry"){ // (rtry %bind SumType %expr) surface/macro sugar misuse detection when not expanded
                // We only validate misuse; correct forms expand to (match ...) earlier.
                // Expect arity 4: rtry %bind SumType %expr
                if(il.size()==4){
                    auto bindSym = symAt(1); auto sumSym = symAt(2); if(bindSym.empty()||bindSym[0] != '%'){ error_code(r,*n,"E1601","rtry bind must be %var","use %prefix for destination binding"); r.success=false; }
                    if(!sumSym.empty()){
                        bool isOpt = sumSym.rfind("Option",0)==0; bool isRes = sumSym.rfind("Result",0)==0;
                        if(!isOpt && !isRes){
                            error_code(r,*n,"E1603","rtry sum type unsupported","use Option*/Result* or ensure macro expansion produced match"); r.success=false;
                        }
                    }
                } else {
                    error_code(r,*n,"E1601","rtry arity","expected (rtry %bind SumType %expr)"); r.success=false;
                }
                continue; // skip further processing; not a real core op
            }
            if(op=="match"){ // treat like switch for reachability; also descend into case/default bodies to track uses
                bool anyReach=false; size_t argBase=1; if(il.size()>=2 && std::holds_alternative<symbol>(il[1]->data)){ std::string maybeDst=std::get<symbol>(il[1]->data).name; if(!maybeDst.empty() && maybeDst[0]=='%') argBase=3; }
                if(il.size()>argBase+1){ noteUse(symAt(argBase+1)); }
                node_ptr casesNode=nullptr, defaultNode=nullptr; for(size_t j=argBase+2;j<il.size();++j){ if(!il[j]||!std::holds_alternative<keyword>(il[j]->data)) break; std::string kw=std::get<keyword>(il[j]->data).name; if(++j>=il.size()) break; auto val=il[j]; if(kw=="cases") casesNode=val; else if(kw=="default") defaultNode=val; }
                if(casesNode && std::holds_alternative<vector_t>(casesNode->data)){
                    for(auto &cv : std::get<vector_t>(casesNode->data).elems){ if(!cv||!std::holds_alternative<list>(cv->data)) continue; auto &cl=std::get<list>(cv->data).elems; if(cl.size()<3) continue; // simple [ body ] or keyworded
                        if(std::holds_alternative<vector_t>(cl[2]->data)){ bool rch=true; scan(std::get<vector_t>(cl[2]->data).elems, rch); anyReach |= rch; }
                        else if(std::holds_alternative<keyword>(cl[2]->data)){ node_ptr bodyNode=nullptr; for(size_t ci=2; ci<cl.size(); ++ci){ if(!cl[ci]||!std::holds_alternative<keyword>(cl[ci]->data)) break; std::string kw=std::get<keyword>(cl[ci]->data).name; if(++ci>=cl.size()) break; auto valn=cl[ci]; if(kw=="body" && valn && std::holds_alternative<vector_t>(valn->data)) bodyNode=valn; }
                            if(bodyNode){ bool rch=true; scan(std::get<vector_t>(bodyNode->data).elems, rch); anyReach |= rch; }
                        }
                    }
                }
                if(defaultNode){ if(std::holds_alternative<vector_t>(defaultNode->data)){ bool rch=true; scan(std::get<vector_t>(defaultNode->data).elems, rch); anyReach |= rch; }
                    else if(std::holds_alternative<list>(defaultNode->data)){ auto &dl = std::get<list>(defaultNode->data).elems; for(size_t di=0; di<dl.size(); ++di){ if(dl[di]&&std::holds_alternative<keyword>(dl[di]->data) && std::get<keyword>(dl[di]->data).name=="body"){ if(di+1<dl.size() && dl[di+1] && std::holds_alternative<vector_t>(dl[di+1]->data)){ bool rch=true; scan(std::get<vector_t>(dl[di+1]->data).elems, rch); anyReach |= rch; } } }
                    }
                }
                reachable = anyReach; continue; }
            // other ops do not change reachability
        }
    };
    bool reachable=true; scan(insts, reachable);
    // W1401: missing top-level ret for non-void function (approximate)
    if(fn.ret!=ctx_.get_base(BaseType::Void)){
        bool hasTopRet=false; for(auto &n : insts){ if(!n || !std::holds_alternative<list>(n->data)) continue; auto &il = std::get<list>(n->data).elems; if(!il.empty() && std::holds_alternative<symbol>(il[0]->data) && std::get<symbol>(il[0]->data).name=="ret"){ hasTopRet=true; break; } }
        if(!hasTopRet){ rep.emit_warning(rep.make_warning("W1401","function may be missing return on some paths","ensure all paths return a value", -1, -1)); }
    }
    // W1404: unused parameter
    for(const auto& p : fn.params){ if(!p.name.empty() && !used.count(p.name)){ rep.emit_warning(rep.make_warning("W1404","unused parameter '%"+p.name+"'","remove or prefix with _ if intentional", -1, -1)); } }
    // W1403: unused variable (defined but never used)
    for(const auto& d : defined){ if(!used.count(d)){
            // Skip parameters (already handled) and allow names prefixed _ as intentionally unused
            if(!d.empty() && d[0]=='_') continue;
            bool isParam=false; for(const auto& p : fn.params){ if(p.name==d){ isParam=true; break; } }
            if(isParam) continue;
            rep.emit_warning(rep.make_warning("W1403","unused variable '%"+d+"'","remove or prefix with _ if intentional", -1, -1));
        }
    }
}
inline void TypeChecker::check_instruction_list(TypeCheckResult& r, const std::vector<node_ptr>& insts, const FunctionInfoTC& fn, int loop_depth){ auto get_var=[&](const std::string& n)->TypeId{ auto it=var_types_.find(n); return it==var_types_.end() ? (TypeId)-1 : it->second; }; auto attach=[&](const node_ptr& n, TypeId t){ n->metadata["type-id"]=detail::make_node((int64_t)t); }; auto is_int=[&](TypeId t){ const Type& T=ctx_.at(t); if(T.kind!=Type::Kind::Base) return false; switch(T.base){ case BaseType::I1: case BaseType::I8: case BaseType::I16: case BaseType::I32: case BaseType::I64: case BaseType::U8: case BaseType::U16: case BaseType::U32: case BaseType::U64: return true; default: return false;} };
    // (Tuple pattern arity mismatch E1454 is diagnosed during expansion; redundant checker pass removed to avoid duplicate reports.)
    for(auto &n: insts){ if(!n||!std::holds_alternative<list>(n->data)){ error(r,*n,"instruction must be list"); r.success=false; continue; } auto &il=std::get<list>(n->data).elems; if(il.empty()||!std::holds_alternative<symbol>(il[0]->data)){ error(r,*n,"instruction missing opcode"); r.success=false; continue; } std::string op=std::get<symbol>(il[0]->data).name; auto sym=[&](size_t i)->std::string{ if(i<il.size() && std::holds_alternative<symbol>(il[i]->data)) return std::get<symbol>(il[i]->data).name; return std::string{}; };
    // --- M4.4 Closures (minimal non-escaping, single capture env) ---
    if(op=="closure"){ // (closure %dst (ptr (fn-type ...)) Callee [ %env ])
        if(il.size()<5){ error_code(r,*n,"E1430","closure arity","expected (closure %dst (ptr (fn-type ...)) <callee> [ %env ])"); r.success=false; continue; }
        std::string dst = sym(1);
        if(dst.empty() || dst[0] != '%'){ error_code(r,*n,"E1435","closure dst must be %var","prefix destination with %"); r.success=false; continue; }
        TypeId toTy = parse_type_node(il[2], r);
        const Type& TT = ctx_.at(toTy);
        if(!(TT.kind==Type::Kind::Pointer && ctx_.at(TT.pointee).kind==Type::Kind::Function)){
            error_code(r,*n,"E1430","closure type must be (ptr (fn-type ...))","annotate with pointer-to-function type"); r.success=false; continue; }
        const Type& TF = ctx_.at(TT.pointee);
        // Callee name can be symbol or string; accept symbol here (strings aren't supported in checker forms elsewhere)
        std::string callee = sym(3);
        if(callee.empty()){ error_code(r,*n,"E1431","closure missing callee","supply function name symbol"); r.success=false; continue; }
        FunctionInfoTC* finfo=nullptr; if(!lookup_function(callee, finfo)){
            error_code(r,*n,"E1431","unknown function for closure","declare function before use"); r.success=false; continue; }
        // Captures vector
        if(!std::holds_alternative<vector_t>(il[4]->data)){
            error_code(r,*n,"E1433","closure captures must be vector","use [ %env ]"); r.success=false; continue;
        }
        auto caps = std::get<vector_t>(il[4]->data).elems;
        if(caps.size()!=1){ error_code(r,*n,"E1433","closure currently supports exactly one capture","provide [ %env ]"); r.success=false; }
        // Validate function vs thunk signature: function must take env first, then thunk params; return must match
        if(finfo->params.size() < 1u){ error_code(r,*n,"E1432","closure target must take env as first parameter","add an env/context parameter first"); r.success=false; }
        if(finfo->ret != TF.ret){ type_mismatch(r,*n,"E1432","closure return", TF.ret, finfo->ret); r.success=false; }
        if(finfo->variadic != TF.variadic){ error_code(r,*n,"E1432","variadic mismatch between closure type and function","ensure :variadic flags match"); r.success=false; }
        size_t expectedThunkParams = finfo->params.size() - 1; if(TF.params.size() != expectedThunkParams){ error_code(r,*n,"E1432","closure param mismatch","thunk params must match function params excluding env"); r.success=false; }
        for(size_t i=0;i<TF.params.size() && i<expectedThunkParams; ++i){ if(TF.params[i] != finfo->params[i+1].type){ type_mismatch(r,*n,"E1432","closure param", finfo->params[i+1].type, TF.params[i]); r.success=false; } }
        // Validate capture type matches function env param type
        if(!caps.empty()){
            auto cap = caps[0]; if(!cap || !std::holds_alternative<symbol>(cap->data)){ error_code(r,*n,"E1434","closure capture must be %var","use symbol of env pointer"); r.success=false; }
            else {
                std::string cname = std::get<symbol>(cap->data).name; if(cname.empty()||cname[0] != '%'){ error_code(r,*n,"E1434","closure capture must be %var","prefix with %"); r.success=false; }
                else { auto ct = get_var(cname.substr(1)); if(ct==(TypeId)-1){ error_code(r,*n,"E1434","closure capture undefined","define %env before use"); r.success=false; }
                    else if(ct != finfo->params[0].type){ type_mismatch(r,*n,"E1434","closure env", finfo->params[0].type, ct); r.success=false; }
                }
            }
        }
        // Define result type
        var_types_[dst.substr(1)] = toTy; attach(n, toTy); continue;
    }
    // --- M4.4 Closures (record + call path) ---
    if(op=="make-closure"){ // (make-closure %dst Callee [ %env ])
        if(il.size()<4){ error_code(r,*n,"E1436","make-closure arity","expected (make-closure %dst <callee> [ %env ])"); r.success=false; continue; }
        std::string dst = sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1435","closure dst must be %var","prefix destination with %"); r.success=false; continue; }
        std::string callee = sym(2); if(callee.empty()){ error_code(r,*n,"E1431","unknown function for closure","supply function name symbol"); r.success=false; continue; }
        FunctionInfoTC* finfo=nullptr; if(!lookup_function(callee, finfo)){ error_code(r,*n,"E1431","unknown function for closure","declare function before use"); r.success=false; continue; }
        if(il.size()<4 || !std::holds_alternative<vector_t>(il[3]->data)){ error_code(r,*n,"E1433","closure captures must be vector","use [ %env ]"); r.success=false; continue; }
        auto caps = std::get<vector_t>(il[3]->data).elems; if(caps.size()!=1){ error_code(r,*n,"E1433","closure currently supports exactly one capture","provide [ %env ]"); r.success=false; }
        // Validate function takes env first
        if(finfo->params.size()<1u){ error_code(r,*n,"E1432","closure target must take env as first parameter","add an env/context parameter first"); r.success=false; }
        // Validate capture var exists and matches env type
        if(!caps.empty()){
            auto cap=caps[0]; if(!cap || !std::holds_alternative<symbol>(cap->data)){ error_code(r,*n,"E1434","closure capture must be %var","use symbol of env pointer"); r.success=false; }
            else {
                std::string cname=std::get<symbol>(cap->data).name; if(cname.empty()||cname[0] != '%'){ error_code(r,*n,"E1434","closure capture must be %var","prefix with %"); r.success=false; }
                else { auto ct=get_var(cname.substr(1)); if(ct==(TypeId)-1){ error_code(r,*n,"E1434","closure capture undefined","define %env before use"); r.success=false; }
                    else if(ct != finfo->params[0].type){ type_mismatch(r,*n,"E1434","closure env", finfo->params[0].type, ct); r.success=false; }
                }
            }
        }
        // Type of %dst is pointer to struct-ref of internal name
        std::string sname = "__edn.closure."+callee;
        TypeId ty = ctx_.get_pointer(ctx_.get_struct(sname));
        var_types_[dst.substr(1)] = ty; attach(n, ty); continue;
    }
    if(op=="call-closure"){ // (call-closure %dst <ret> %clos %args...)
        if(il.size()<4){ error_code(r,*n,"E1437","call-closure arity","expected (call-closure %dst <ret> %clos %args...)"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1437","call-closure dst must be %var","prefix destination with %"); r.success=false; continue; }
        TypeId retTy = parse_type_node(il[2], r);
        std::string clos=sym(3); if(clos.empty()||clos[0] != '%'){ error_code(r,*n,"E1437","call-closure clos must be %var","pass closure value as %var"); r.success=false; continue; }
        auto cty = get_var(clos.substr(1)); if(cty==(TypeId)-1){ error_code(r,*n,"E1437","call-closure undefined closure var","define closure before use"); r.success=false; continue; }
        const Type& CT = ctx_.at(cty); if(CT.kind!=Type::Kind::Pointer || ctx_.at(CT.pointee).kind!=Type::Kind::Struct){ error_code(r,*n,"E1437","call-closure expects pointer to closure struct","build with make-closure first"); r.success=false; continue; }
        const Type& ST = ctx_.at(CT.pointee);
        std::string sname = ST.struct_name; std::string prefix = "__edn.closure.";
        if(sname.rfind(prefix,0)!=0){ error_code(r,*n,"E1437","call-closure expects closure struct","name must start with __edn.closure."); r.success=false; continue; }
        std::string callee = sname.substr(prefix.size()); FunctionInfoTC* finfo=nullptr; if(!lookup_function(callee, finfo)){ error_code(r,*n,"E1431","unknown function for closure","declare function before use"); r.success=false; continue; }
        // Validate args count/types vs callee (excluding env first param)
        size_t userArgCount = (il.size()>4)? (il.size()-4) : 0; size_t expected = finfo->params.size()>=1? (finfo->params.size()-1) : 0;
        if(!finfo->variadic){ if(userArgCount != expected){ error_code(r,*n,"E1437","call-closure arg count mismatch","provide exactly callee params minus env"); r.success=false; } }
        else { if(userArgCount < expected){ error_code(r,*n,"E1437","call-closure arg count mismatch","provide at least callee params minus env"); r.success=false; } }
        size_t checkN = std::min(userArgCount, expected);
        for(size_t i=0;i<checkN; ++i){ std::string an = sym(4+i); if(an.empty()||an[0] != '%'){ error_code(r,*n,"E1437","call-closure arg must be %var","prefix with %"); r.success=false; continue; } auto at = get_var(an.substr(1)); if(at==(TypeId)-1) { error_code(r,*n,"E1437","call-closure arg undefined","define argument before use"); r.success=false; continue; } if(at != finfo->params[i+1].type){ type_mismatch(r,*n,"E1437","call-closure arg", finfo->params[i+1].type, at); r.success=false; } }
        // Return type must match callee's return
        if(retTy != finfo->ret){ type_mismatch(r,*n,"E1437","call-closure return", finfo->ret, retTy); r.success=false; }
        var_types_[dst.substr(1)] = retTy; attach(n, retTy); continue;
    }
    // --- M4.6 Coroutines (minimal, behind EDN_ENABLE_CORO) ---
    if(op=="coro-begin"){ // (coro-begin %hdl)
        if(il.size()!=2){ error_code(r,*n,"E1460","coro-begin arity","expected (coro-begin %hdl)"); r.success=false; continue; }
        std::string dst=sym(1);
        if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1461","coro-begin dst must be %var","prefix destination with %"); r.success=false; continue; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1464","redefinition of variable","use fresh SSA name for handle"); r.success=false; }
        // Handle type is opaque i8* (coro frame pointer)
        TypeId i8 = ctx_.get_base(BaseType::I8);
        TypeId hty = ctx_.get_pointer(i8);
        var_types_[dst.substr(1)] = hty; attach(n, hty); continue;
    }
    if(op=="coro-suspend"){ // (coro-suspend %st %hdl) -> %st i8
        if(il.size()!=3){ error_code(r,*n,"E1462","coro-suspend arity","expected (coro-suspend %st %hdl)"); r.success=false; continue; }
        std::string dst=sym(1), h=sym(2);
        if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1462","coro-suspend arity","%st required as destination"); r.success=false; continue; }
        if(h.empty()||h[0] != '%'){ error_code(r,*n,"E1463","coro-suspend handle must be %var","pass handle from coro-begin"); r.success=false; continue; }
        auto ht = get_var(h.substr(1)); if(ht==(TypeId)-1){ error_code(r,*n,"E1463","coro-suspend handle undefined","call coro-begin first"); r.success=false; }
        else {
            const Type& HT = ctx_.at(ht);
            bool ok = (HT.kind==Type::Kind::Pointer && ctx_.at(HT.pointee).kind==Type::Kind::Base && ctx_.at(HT.pointee).base==BaseType::I8);
            if(!ok){ TypeId expected = ctx_.get_pointer(ctx_.get_base(BaseType::I8)); type_mismatch(r,*n,"E1463","coro handle", expected, ht); r.success=false; }
        }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1464","redefinition of variable","use fresh SSA name for %st"); r.success=false; }
        // status is i8 from llvm.coro.suspend
        TypeId st = ctx_.get_base(BaseType::I8);
        var_types_[dst.substr(1)] = st; attach(n, st); continue;
    }
    if(op=="coro-final-suspend"){ // (coro-final-suspend %st %hdlOrTok)
        if(il.size()!=3){ error_code(r,*n,"E1462","coro-final-suspend arity","expected (coro-final-suspend %st %hdl)"); r.success=false; continue; }
        std::string dst=sym(1), h=sym(2);
        if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1462","coro-final-suspend arity","%st required as destination"); r.success=false; continue; }
        if(h.empty()||h[0] != '%'){ error_code(r,*n,"E1463","coro-final-suspend handle must be %var","pass handle/token"); r.success=false; continue; }
        auto ht = get_var(h.substr(1)); if(ht==(TypeId)-1){ error_code(r,*n,"E1463","coro-final-suspend operand undefined","call coro-begin or coro-save first"); r.success=false; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1464","redefinition of variable","use fresh SSA name for %st"); r.success=false; }
        TypeId st = ctx_.get_base(BaseType::I8);
        var_types_[dst.substr(1)] = st; attach(n, st); continue;
    }
    if(op=="coro-save"){ // (coro-save %sv %hdl) -> token as i8 placeholder
        if(il.size()!=3){ error_code(r,*n,"E1466","coro-save arity","expected (coro-save %sv %hdl)"); r.success=false; continue; }
        std::string dst=sym(1), h=sym(2);
        if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1466","coro-save arity","%sv required as destination"); r.success=false; continue; }
        if(h.empty()||h[0] != '%'){ error_code(r,*n,"E1466","coro-save arity","handle must be %var from coro-begin"); r.success=false; continue; }
        auto ht=get_var(h.substr(1)); if(ht==(TypeId)-1){ error_code(r,*n,"E1466","coro-save handle undefined","call coro-begin first"); r.success=false; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1464","redefinition of variable","use fresh SSA name for %sv"); r.success=false; }
        // We don't model token in type system yet; use i8 placeholder for SSA bookkeeping
        TypeId placeholder = ctx_.get_base(BaseType::I8);
        var_types_[dst.substr(1)] = placeholder; attach(n, placeholder); continue;
    }
    if(op=="coro-promise"){ // (coro-promise %p %hdl) -> ptr
        if(il.size()!=3){ error_code(r,*n,"E1467","coro-promise arity","expected (coro-promise %p %hdl)"); r.success=false; continue; }
        std::string dst=sym(1), h=sym(2);
        if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1467","coro-promise arity","%p required as destination"); r.success=false; continue; }
        if(h.empty()||h[0] != '%'){ error_code(r,*n,"E1468","coro-promise handle must be %var","pass handle from coro-begin"); r.success=false; continue; }
        auto ht=get_var(h.substr(1)); if(ht==(TypeId)-1){ error_code(r,*n,"E1468","coro-promise handle undefined","call coro-begin first"); r.success=false; }
        else { const Type& HT=ctx_.at(ht); bool ok=(HT.kind==Type::Kind::Pointer && ctx_.at(HT.pointee).kind==Type::Kind::Base && ctx_.at(HT.pointee).base==BaseType::I8);
            if(!ok){ TypeId expected = ctx_.get_pointer(ctx_.get_base(BaseType::I8)); type_mismatch(r,*n,"E1468","coro handle", expected, ht); r.success=false; }
        }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1464","redefinition of variable","use fresh SSA name for %p"); r.success=false; }
        // Promise pointer is opaque ptr (i8*)
        TypeId pty = ctx_.get_pointer(ctx_.get_base(BaseType::I8));
        var_types_[dst.substr(1)] = pty; attach(n, pty); continue;
    }
    if(op=="coro-resume"){ // (coro-resume %hdl)
        if(il.size()!=2){ error_code(r,*n,"E1469","coro-resume arity","expected (coro-resume %hdl)"); r.success=false; continue; }
        std::string h=sym(1); if(h.empty()||h[0] != '%'){ error_code(r,*n,"E1469","coro-resume arity","handle must be %var"); r.success=false; continue; }
        auto ht=get_var(h.substr(1)); if(ht==(TypeId)-1){ error_code(r,*n,"E1469","coro-resume handle undefined","call coro-begin first"); r.success=false; }
        continue;
    }
    if(op=="coro-destroy"){ // (coro-destroy %hdl)
        if(il.size()!=2){ error_code(r,*n,"E1470","coro-destroy arity","expected (coro-destroy %hdl)"); r.success=false; continue; }
        std::string h=sym(1); if(h.empty()||h[0] != '%'){ error_code(r,*n,"E1470","coro-destroy arity","handle must be %var"); r.success=false; continue; }
        auto ht=get_var(h.substr(1)); if(ht==(TypeId)-1){ error_code(r,*n,"E1470","coro-destroy handle undefined","call coro-begin first"); r.success=false; }
        continue;
    }
    if(op=="coro-done"){ // (coro-done %d %hdl) -> i1
        if(il.size()!=3){ error_code(r,*n,"E1471","coro-done arity","expected (coro-done %d %hdl)"); r.success=false; continue; }
        std::string dst=sym(1), h=sym(2);
        if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1471","coro-done arity","%d required as destination"); r.success=false; continue; }
        if(h.empty()||h[0] != '%'){ error_code(r,*n,"E1471","coro-done arity","handle must be %var"); r.success=false; continue; }
        auto ht=get_var(h.substr(1)); if(ht==(TypeId)-1){ error_code(r,*n,"E1471","coro-done handle undefined","call coro-begin first"); r.success=false; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1464","redefinition of variable","use fresh SSA name for %d"); r.success=false; }
        TypeId b = ctx_.get_base(BaseType::I1);
        var_types_[dst.substr(1)] = b; attach(n, b); continue;
    }
    if(op=="coro-id"){ // (coro-id %cid) binds current id token to a name (placeholder type)
        if(il.size()!=2){ error_code(r,*n,"E1472","coro-id arity","expected (coro-id %cid)"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1472","coro-id arity","%cid required as destination"); r.success=false; continue; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1464","redefinition of variable","use fresh SSA name for %cid"); r.success=false; }
        var_types_[dst.substr(1)] = ctx_.get_base(BaseType::I8); attach(n, ctx_.get_base(BaseType::I8)); continue;
    }
    if(op=="coro-size"){ // (coro-size %sz) -> i64
        if(il.size()!=2){ error_code(r,*n,"E1473","coro-size arity","expected (coro-size %sz)"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1473","coro-size arity","%sz required as destination"); r.success=false; continue; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1464","redefinition of variable","use fresh SSA name for %sz"); r.success=false; }
        auto t = ctx_.get_base(BaseType::I64); var_types_[dst.substr(1)] = t; attach(n, t); continue;
    }
    if(op=="coro-alloc"){ // (coro-alloc %need %cid) -> i1
        if(il.size()!=3){ error_code(r,*n,"E1474","coro-alloc arity","expected (coro-alloc %need %cid)"); r.success=false; continue; }
        std::string dst=sym(1), cid=sym(2); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1474","coro-alloc arity","%need required as destination"); r.success=false; continue; }
        if(cid.empty()||cid[0] != '%'){ error_code(r,*n,"E1474","coro-alloc arity","%cid must be a %var"); r.success=false; continue; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1464","redefinition of variable","use fresh SSA name for %need"); r.success=false; }
        var_types_[dst.substr(1)] = ctx_.get_base(BaseType::I1); attach(n, ctx_.get_base(BaseType::I1)); continue;
    }
    if(op=="coro-free"){ // (coro-free %mem %cid %hdl) -> ptr
        if(il.size()!=4){ error_code(r,*n,"E1475","coro-free arity","expected (coro-free %mem %cid %hdl)"); r.success=false; continue; }
        std::string dst=sym(1), cid=sym(2), h=sym(3);
        if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1475","coro-free arity","%mem required as destination"); r.success=false; continue; }
        if(cid.empty()||cid[0] != '%'){ error_code(r,*n,"E1475","coro-free arity","%cid must be %var"); r.success=false; continue; }
        if(h.empty()||h[0] != '%'){ error_code(r,*n,"E1475","coro-free arity","%hdl must be %var"); r.success=false; continue; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1464","redefinition of variable","use fresh SSA name for %mem"); r.success=false; }
        auto p = ctx_.get_pointer(ctx_.get_base(BaseType::I8)); var_types_[dst.substr(1)] = p; attach(n, p); continue;
    }
    if(op=="coro-end"){ // (coro-end %hdl)
        if(il.size()!=2){ error_code(r,*n,"E1465","coro-end arity","expected (coro-end %hdl)"); r.success=false; continue; }
        std::string h=sym(1);
        if(h.empty()||h[0] != '%'){ error_code(r,*n,"E1465","coro-end arity","handle must be %var from coro-begin"); r.success=false; continue; }
        auto ht = get_var(h.substr(1)); if(ht==(TypeId)-1){ error_code(r,*n,"E1465","coro-end handle undefined","call coro-begin first"); r.success=false; }
        continue;
    }
    // --- M4.1 Sum constructors and tag test ---
    if(op=="sum-new"){ // (sum-new %dst SumType Variant [ %v0 %v1 ... ])
        if(il.size()<4){ error_code(r,*n,"E1409","sum-new arity","expected (sum-new %dst SumType Variant [ %vals* ])"); r.success=false; continue; }
        std::string dst=sym(1), sName=sym(2), vName=sym(3);
        if(dst.empty()||sName.empty()||vName.empty()){ error_code(r,*n,"E1409","sum-new arity","supply %dst SumName Variant [ ... ]"); r.success=false; continue; }
        auto sit=sums_.find(sName); if(sit==sums_.end()){ error_code(r,*n,"E1400","unknown sum type","declare sum before use"); r.success=false; continue; }
        auto vit=sit->second.variant_map.find(vName); if(vit==sit->second.variant_map.end()){ error_code(r,*n,"E1405","unknown variant","check variant name"); r.success=false; continue; }
        std::vector<node_ptr> vec;
        if(il.size()>=5){ if(!il[4]||!std::holds_alternative<vector_t>(il[4]->data)){ error_code(r,*n,"E1406","sum-new fields must be vector","wrap payload values in [ ]"); r.success=false; continue; } vec = std::get<vector_t>(il[4]->data).elems; }
        auto &fields = vit->second->fields; if(vec.size()!=fields.size()){ error_code(r,*n,"E1409","sum-new arity","payload count must match variant fields"); r.success=false; }
        for(size_t i=0;i<vec.size() && i<fields.size(); ++i){
            if(!vec[i]||!std::holds_alternative<symbol>(vec[i]->data)){
                error_code(r,*n,"E1409","sum-new arity","payload must be %var symbols"); r.success=false; continue; }
            std::string vs = std::get<symbol>(vec[i]->data).name;
            if(vs.empty()||vs[0] != '%'){
                error_code(r,*n,"E1409","sum-new arity","payload must be %var symbols"); r.success=false; continue; }
            auto vt=get_var(vs.substr(1));
            if(vt==(TypeId)-1){
                error_code(r,*n,"E1409","sum-new payload undefined","define payload variable before use"); r.success=false; continue;
            }
            if(vt!=fields[i]){ type_mismatch(r,*n,"E1409","sum-new field",fields[i],vt); r.success=false; }
        }
        if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1409","sum-new arity","destination already defined"); r.success=false; } TypeId sumTy = ctx_.get_struct(sName); TypeId pty=ctx_.get_pointer(sumTy); var_types_[dst.substr(1)]=pty; attach(n,pty);} else { error_code(r,*n,"E1409","sum-new arity","dst must be %var"); r.success=false; }
        continue;
    }
    if(op=="sum-is"){ // (sum-is %dst SumType %value Variant) -> %dst i1
        if(il.size()!=5){ error_code(r,*n,"E1409","sum-is arity","expected (sum-is %dst SumType %val Variant)"); r.success=false; continue; }
        std::string dst=sym(1), sName=sym(2), val=sym(3), vName=sym(4);
        if(dst.empty()||sName.empty()||val.empty()||vName.empty()){ error_code(r,*n,"E1409","sum-is arity","supply %dst SumName %val Variant"); r.success=false; continue; }
        auto sit=sums_.find(sName); if(sit==sums_.end()){ error_code(r,*n,"E1400","unknown sum type","declare sum before use"); r.success=false; continue; }
        if(sit->second.variant_map.find(vName)==sit->second.variant_map.end()){ error_code(r,*n,"E1405","unknown variant","check variant name"); r.success=false; continue; }
        if(val[0]=='%'){ auto vt=get_var(val.substr(1)); if(vt==(TypeId)-1){ error_code(r,*n,"E1409","sum-is arity","value undefined"); r.success=false; } else { const Type& VT=ctx_.at(vt); bool ok=false; if(VT.kind==Type::Kind::Pointer){ const Type& PT=ctx_.at(VT.pointee); if(PT.kind==Type::Kind::Struct && PT.struct_name==sName) ok=true; } if(!ok){ TypeId expectedStruct = ctx_.get_struct(sName); type_mismatch(r,*n,"E1409","sum-is value", expectedStruct, vt); r.success=false; } } }
        else { error_code(r,*n,"E1409","sum-is arity","value must be %var"); r.success=false; }
        if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1409","sum-is arity","destination already defined"); r.success=false; } TypeId b=ctx_.get_base(BaseType::I1); var_types_[dst.substr(1)]=b; attach(n,b);} else { error_code(r,*n,"E1409","sum-is arity","dst must be %var"); r.success=false; }
        continue;
    }
    if(op=="sum-get"){ // (sum-get %dst SumType %value Variant <index>) -> %dst field type
        if(il.size()!=6){ error_code(r,*n,"E1409","sum-get arity","expected (sum-get %dst SumType %val Variant <index>)"); r.success=false; continue; }
        std::string dst=sym(1), sName=sym(2), val=sym(3), vName=sym(4);
        if(dst.empty()||sName.empty()||val.empty()||vName.empty()){ error_code(r,*n,"E1409","sum-get arity","supply %dst SumName %val Variant <index>"); r.success=false; continue; }
        auto sit=sums_.find(sName); if(sit==sums_.end()){ error_code(r,*n,"E1400","unknown sum type","declare sum before use"); r.success=false; continue; }
        auto vit=sit->second.variant_map.find(vName); if(vit==sit->second.variant_map.end()){ error_code(r,*n,"E1405","unknown variant","check variant name"); r.success=false; continue; }
        // index must be integer literal
        size_t idx=SIZE_MAX; if(std::holds_alternative<int64_t>(il[5]->data)){ auto v=(int64_t)std::get<int64_t>(il[5]->data); if(v>=0) idx=(size_t)v; }
        if(idx==SIZE_MAX){ error_code(r,*n,"E1409","sum-get index must be int literal","use 0-based field index"); r.success=false; continue; }
        // value must be pointer to this sum struct
        if(val[0]=='%'){ auto vt=get_var(val.substr(1)); if(vt==(TypeId)-1){ error_code(r,*n,"E1409","sum-get value undefined","define value earlier"); r.success=false; }
            else { const Type& VT=ctx_.at(vt); bool ok=false; if(VT.kind==Type::Kind::Pointer){ const Type& PT=ctx_.at(VT.pointee); if(PT.kind==Type::Kind::Struct && PT.struct_name==sName) ok=true; }
                if(!ok){ TypeId expected = ctx_.get_pointer(ctx_.get_struct(sName)); type_mismatch(r,*n,"E1409","sum-get value", expected, vt); r.success=false; }
            }
        } else { error_code(r,*n,"E1409","sum-get value must be %var","prefix with %"); r.success=false; continue; }
        // bounds check and set result type
        auto &fields = vit->second->fields; if(idx>=fields.size()){ error_code(r,*n,"E1409","sum-get index out of range","variant has fewer fields"); r.success=false; continue; }
        if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1409","sum-get dst already defined","use fresh SSA name"); r.success=false; } var_types_[dst.substr(1)]=fields[idx]; attach(n,fields[idx]); }
        else { error_code(r,*n,"E1409","sum-get dst must be %var","prefix destination with %"); r.success=false; }
        continue;
    }
    if(op=="block"){ 
        // Enter new lexical scope: collect :locals then type-check :body with extended symbol table.
        std::unordered_map<std::string,TypeId> saved=var_types_;
        for(size_t i=1;i<il.size(); ++i){
            if(!il[i]||!std::holds_alternative<keyword>(il[i]->data)) break;
            std::string kw=std::get<keyword>(il[i]->data).name;
            if(++i>=il.size()) break; 
            auto val=il[i];
            if(kw=="locals"){
                if(val && std::holds_alternative<vector_t>(val->data)){
                    for(auto &d: std::get<vector_t>(val->data).elems){
                        if(!d||!std::holds_alternative<list>(d->data)) continue; 
                        auto &dl=std::get<list>(d->data).elems;
                        // (local <type> %name)
                        if(dl.size()==3 && std::holds_alternative<symbol>(dl[0]->data) && std::get<symbol>(dl[0]->data).name=="local"){
                            // Parse type for each local (allow parse failures to surface diagnostics)
                            TypeId lty = parse_type_node(dl[1], r);
                            if(std::holds_alternative<symbol>(dl[2]->data)){
                                std::string vn=std::get<symbol>(dl[2]->data).name;
                                if(!vn.empty()&&vn[0]=='%') vn.erase(0,1);
                                if(var_types_.count(vn)){
                                    error(r,*d,"duplicate local"); r.success=false;
                                } else {
                                    var_types_[vn]=lty;
                                }
                            }
                        }
                    }
                }
            } else if(kw=="body"){
                if(val && std::holds_alternative<vector_t>(val->data))
                    check_instruction_list(r, std::get<vector_t>(val->data).elems, fn, loop_depth);
            }
        }
        var_types_=saved; 
        continue; 
    }
    if(op=="if"){ if(il.size()<3){ error_code(r,*n,"E1000","if arity","expected (if %cond [ then ] [ else ])"); r.success=false; continue; } std::string cond=sym(1); if(cond.empty()||cond[0] != '%'){ error_code(r,*n,"E1001","if cond must be %var","prefix condition with %"); r.success=false; continue; } auto ct=get_var(cond.substr(1)); if(ct!=(TypeId)-1){ const Type& T=ctx_.at(ct); if(!(T.kind==Type::Kind::Base && T.base==BaseType::I1)){ error_code(r,*n,"E1002","if cond must be i1","use boolean (i1) value"); r.success=false; } } if(il.size()>=3 && std::holds_alternative<vector_t>(il[2]->data)) check_instruction_list(r, std::get<vector_t>(il[2]->data).elems, fn, loop_depth); if(il.size()>=4 && std::holds_alternative<vector_t>(il[3]->data)) check_instruction_list(r, std::get<vector_t>(il[3]->data).elems, fn, loop_depth); continue; }
    if(op=="try"){ // (try :body [ ... ] :catch [ ... ])
        node_ptr bodyNode=nullptr, catchNode=nullptr; bool haveBody=false, haveCatch=false;
        for(size_t i=1;i<il.size(); ++i){ if(!il[i] || !std::holds_alternative<keyword>(il[i]->data)) break; std::string kw=std::get<keyword>(il[i]->data).name; if(++i>=il.size()) break; auto val=il[i];
            if(kw=="body"){ if(val && std::holds_alternative<vector_t>(val->data)){ bodyNode=val; haveBody=true; } else { error_code(r,*n,"E1451","try :body must be vector","wrap body in [ ... ]"); r.success=false; } }
            else if(kw=="catch"){ if(val && std::holds_alternative<vector_t>(val->data)){ catchNode=val; haveCatch=true; } else { error_code(r,*n,"E1452","try :catch must be vector","wrap handler in [ ... ]"); r.success=false; } }
        }
        if(!haveBody){ error_code(r,*n,"E1450","try missing :body","add :body [ ... ]"); r.success=false; }
        if(!haveCatch){ error_code(r,*n,"E1453","try missing :catch","add :catch [ ... ]"); r.success=false; }
        if(bodyNode && std::holds_alternative<vector_t>(bodyNode->data)) check_instruction_list(r, std::get<vector_t>(bodyNode->data).elems, fn, loop_depth);
        if(catchNode && std::holds_alternative<vector_t>(catchNode->data)) check_instruction_list(r, std::get<vector_t>(catchNode->data).elems, fn, loop_depth);
        continue;
    }
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
    // (Removed duplicate legacy switch/match handling block)
    if(op=="break"){ if(loop_depth==0){ error_code(r,*n,"E1006","break outside loop","use inside (while ...) body"); r.success=false; } if(il.size()!=1){ error_code(r,*n,"E1007","break takes no operands","remove extra tokens"); r.success=false; } continue; }
    if(op=="and"||op=="or"||op=="xor"||op=="shl"||op=="lshr"||op=="ashr"){ if(il.size()!=5){ error_code(r,*n,"E0600","bit/logical op arity","expected ("+op+" %dst <int-type> %a %b)"); r.success=false; continue; } std::string dst=sym(1), a=sym(3), b=sym(4); if(dst.empty()||a.empty()||b.empty()){ error_code(r,*n,"E0601","bit/logical expects symbols","use %dst %lhs %rhs"); r.success=false; continue; } TypeId ty=parse_type_node(il[2],r); if(ty==(TypeId)-1){ r.success=false; continue; } const Type& T=ctx_.at(ty); if(!(T.kind==Type::Kind::Base && (T.base==BaseType::I1||T.base==BaseType::I8||T.base==BaseType::I16||T.base==BaseType::I32||T.base==BaseType::I64))){ error_code(r,*n,"E0602","bit/logical op type must be integer","choose i1/i8/i16/i32/i64"); r.success=false; }
        auto check_operand=[&](const std::string& v){ if(v[0]=='%'){ auto vt=get_var(v.substr(1)); if(vt!=(TypeId)-1 && vt!=ty){ error_code(r,*n,"E0603","operand type mismatch","operands must match annotated type"); r.success=false; } } else { error_code(r,*n,"E0604","operand must be %var","prefix with %"); r.success=false; } };
        check_operand(a); check_operand(b); if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0605","redefinition of variable","rename destination"); r.success=false; } var_types_[dst.substr(1)]=ty; attach(n,ty);} else { error_code(r,*n,"E0606","dest must be %var","prefix destination with %"); r.success=false; } continue; }
    if(op=="fadd"||op=="fsub"||op=="fmul"||op=="fdiv"){ if(il.size()!=5){ error_code(r,*n,"E0700","fbinop arity","expected ("+op+" %dst <float-type> %a %b)"); r.success=false; continue; } std::string dst=sym(1),a=sym(3),b=sym(4); if(dst.empty()||a.empty()||b.empty()){ error_code(r,*n,"E0701","fbinop symbol expected","use % for dst and operands"); r.success=false; continue; } TypeId ty=parse_type_node(il[2],r); const Type& T=ctx_.at(ty); if(!(T.kind==Type::Kind::Base && (T.base==BaseType::F32||T.base==BaseType::F64))){ error_code(r,*n,"E0702","fbinop type must be f32/f64","choose f32 or f64"); r.success=false; }
        if(a[0]=='%'){ auto at=get_var(a.substr(1)); if(at!=(TypeId)-1 && at!=ty){ type_mismatch(r,*n,"E0703","lhs",ty,at); r.success=false; } }
        if(b[0]=='%'){ auto bt=get_var(b.substr(1)); if(bt!=(TypeId)-1 && bt!=ty){ type_mismatch(r,*n,"E0704","rhs",ty,bt); r.success=false; } }
        if(dst[0]=='%'){ if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E0705","redefinition of variable","rename destination"); r.success=false; } var_types_[dst.substr(1)]=ty; attach(n,ty);} else { error_code(r,*n,"E0706","dest must be %var","prefix destination with %"); r.success=false; } continue; }
    // (duplicate integer binop block removed)
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
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1305","redefinition of variable","rename destination"); r.success=false; }
        var_types_[dst.substr(1)]=annot; attach(n,annot); continue;
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
        auto st=get_var(src.substr(1)); if(st==(TypeId)-1){ error_code(r,*n,"E1314","addr source undefined","define source earlier"); r.success=false; continue; }
    else { const Type& srcT=ctx_.at(st); (void)srcT; if(AT.kind==Type::Kind::Pointer && AT.pointee!=st){ type_mismatch(r,*n,"E1315","addr source",AT.pointee,st); r.success=false; } }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1316","redefinition of variable","rename destination"); r.success=false; }
        var_types_[dst.substr(1)]=annot; attach(n,annot); continue; }
    if(op=="deref"){ // (deref %dst <T> %ptr)
        if(il.size()!=4){ error_code(r,*n,"E1317","deref arity","expected (deref %dst <type> %ptr)" ); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1318","deref dst must be %var","prefix destination with %"); r.success=false; continue; }
        TypeId ty=parse_type_node(il[2],r); std::string ptr=sym(3); if(ptr.empty()||ptr[0] != '%'){ error_code(r,*n,"E1319","deref ptr must be %var","prefix pointer with %"); r.success=false; continue; }
        auto pt=get_var(ptr.substr(1)); if(pt==(TypeId)-1){ error_code(r,*n,"E1319","deref ptr must be %var","pointer var undefined"); r.success=false; }
        else { const Type& PT=ctx_.at(pt); if(PT.kind!=Type::Kind::Pointer || PT.pointee!=ty){ if(PT.kind==Type::Kind::Pointer) type_mismatch(r,*n,"E1319","deref ptr",ty,PT.pointee); else error_code(r,*n,"E1319","deref ptr type mismatch","pointer pointee must match <type>"); r.success=false; } }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1316","redefinition of variable","rename destination"); r.success=false; }
        var_types_[dst.substr(1)]=ty; attach(n,ty); continue; }
    if(op=="cstr"){ // (cstr %dst "literal") => %dst : (ptr i8)
        if(il.size()!=3){ error_code(r,*n,"E1500","cstr arity","expected (cstr %dst \"literal\")"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1501","cstr dst must be %var","prefix destination with %"); r.success=false; continue; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1502","cstr dst already defined","use fresh SSA name"); r.success=false; }
        if(!std::holds_alternative<symbol>(il[2]->data)){ error_code(r,*n,"E1503","cstr literal must be symbol","string literal token expected"); r.success=false; continue; }
        std::string lit = std::get<symbol>(il[2]->data).name; if(lit.size()<2 || lit.front()!='"' || lit.back()!='"'){ error_code(r,*n,"E1504","cstr literal malformed","wrap in quotes"); r.success=false; }
        TypeId pI8 = ctx_.get_pointer(ctx_.get_base(BaseType::I8)); var_types_[dst.substr(1)] = pI8; attach(n,pI8); continue; }
    if(op=="bytes"){ // (bytes %dst [ ints ]) => %dst : (ptr i8)
        if(il.size()!=3){ error_code(r,*n,"E1510","bytes arity","expected (bytes %dst [ i8* ])"); r.success=false; continue; }
        std::string dst=sym(1); if(dst.empty()||dst[0] != '%'){ error_code(r,*n,"E1511","bytes dst must be %var","prefix destination with %"); r.success=false; continue; }
        if(var_types_.count(dst.substr(1))){ error_code(r,*n,"E1512","bytes dst already defined","use fresh SSA name"); r.success=false; }
        if(!std::holds_alternative<vector_t>(il[2]->data)){ error_code(r,*n,"E1513","bytes expects vector","wrap values in [ ]"); r.success=false; continue; }
        auto &vec = std::get<vector_t>(il[2]->data).elems; if(vec.empty()){ error_code(r,*n,"E1514","bytes vector empty","provide at least one element"); r.success=false; }
        for(auto &v : vec){ if(!v || !std::holds_alternative<int64_t>(v->data)){ error_code(r,*n,"E1515","bytes element must be int","use integer 0..255"); r.success=false; break; } else { auto val = std::get<int64_t>(v->data); if(val<0 || val>255){ error_code(r,*n,"E1516","bytes element out of range","values must be 0..255"); r.success=false; break; } } }
        TypeId pI8 = ctx_.get_pointer(ctx_.get_base(BaseType::I8)); var_types_[dst.substr(1)] = pI8; attach(n,pI8); continue; }
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
        TypeId ret=parse_type_node(il[2],r); std::string fptr=sym(3); if(fptr.empty()||fptr[0] != '%'){ error_code(r,*n,"E1322","call-indirect fptr must be %var","prefix function pointer with %"); r.success=false; continue; }
        auto fp=get_var(fptr.substr(1)); if(fp==(TypeId)-1){ error_code(r,*n,"E1323","call-indirect fptr undefined","define fn pointer earlier"); r.success=false; continue; }
        const Type& FPT=ctx_.at(fp); if(FPT.kind!=Type::Kind::Pointer){ error_code(r,*n,"E1323","call-indirect fptr not pointer","pointer to (fn-type ...) required"); r.success=false; }
        Type fnTy; if(FPT.kind==Type::Kind::Pointer) fnTy=ctx_.at(FPT.pointee); if(FPT.kind!=Type::Kind::Pointer || fnTy.kind!=Type::Kind::Function){ error_code(r,*n,"E1323","call-indirect fptr not function pointer","use (ptr (fn-type ...))"); r.success=false; }
        if(fnTy.kind==Type::Kind::Function){
            if(ret!=fnTy.ret){ type_mismatch(r,*n,"E1324","call-indirect return",fnTy.ret,ret); r.success=false; }
            size_t expected = fnTy.params.size();
            size_t provided = (il.size()>4)? il.size()-4:0;
            if(!fnTy.variadic){
                if(provided!=expected){ error_code(r,*n,"E1325","call-indirect arg count mismatch","expected "+std::to_string(expected)+" got "+std::to_string(provided)); r.success=false; }
            } else {
                if(provided < expected){ error_code(r,*n,"E1360","variadic missing required args","requires "+std::to_string(expected)+" fixed args, got "+std::to_string(provided)); r.success=false; }
            }
            size_t checkN = std::min(expected, provided);
            for(size_t ai=0; ai<checkN; ++ai){
                std::string av = sym(4+ai);
                if(av.empty()||av[0] != '%'){ error_code(r,*n,"E0406","call arg must be %var","prefix each arg with %"); r.success=false; continue; }
                auto at=get_var(av.substr(1));
                if(at==(TypeId)-1){
                    size_t before=r.errors.size();
                    error_code(r,*n,"E0407","unknown arg var","define arg value earlier");
                    if(before<r.errors.size()){
                        std::vector<std::string> vars; for(auto &kv: var_types_) if(kv.second==fnTy.params[ai]) vars.push_back("%"+kv.first);
                        append_suggestions(r.errors.back(), fuzzy_candidates(av, vars));
                    }
                    r.success=false; continue;
                }
                if(at!=fnTy.params[ai]){ type_mismatch(r,*n,"E0408","call arg",fnTy.params[ai],at); r.success=false; }
            }
            // Extra variadic args: ensure each is a defined %var
            if(fnTy.variadic && provided>expected){
                for(size_t ai=expected; ai<provided; ++ai){
                    std::string av=sym(4+ai);
                    if(av.empty()||av[0] != '%'){ error_code(r,*n,"E1361","variadic arg must be %var","prefix each variadic arg with %"); r.success=false; }
                    else { auto at=get_var(av.substr(1)); if(at==(TypeId)-1){ size_t before=r.errors.size(); error_code(r,*n,"E1362","variadic arg undefined","define argument earlier"); if(before<r.errors.size()){ std::vector<std::string> cands; for(auto &kv: var_types_) cands.push_back("%"+kv.first); append_suggestions(r.errors.back(), fuzzy_candidates(av, cands)); } r.success=false; } }
                }
            }
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
inline TypeCheckResult TypeChecker::check_module(const node_ptr& m){ TypeCheckResult res{true,{},{}}; if(!m||!std::holds_alternative<list>(m->data)){ res.success=false; res.errors.push_back(TypeError{"EMOD1","module not list","",-1,-1,{}}); return res; } auto &l=std::get<list>(m->data).elems; if(l.empty()||!std::holds_alternative<symbol>(l[0]->data) || std::get<symbol>(l[0]->data).name!="module"){ res.success=false; res.errors.push_back(TypeError{"EMOD2","expected (module ...)","start file with (module ...)",-1,-1,{}}); return res; } reset(); collect_typedefs(res,l); collect_enums(res,l); collect_structs(res,l); collect_unions(res,l); collect_sums(res,l); collect_globals(res,l); collect_functions_headers(res,l); if(res.success) check_functions(res,l); 
    // Emit lints for unused globals (W1405) unless disabled
    if(const char* lintEnv = std::getenv("EDN_LINT")){ if(lintEnv[0]=='0') return res; }
    {
        ErrorReporter rep{&res.errors,&res.warnings};
        for(const auto& kv : globals_){ if(!used_globals_.count(kv.first)){
                rep.emit_warning(rep.make_warning("W1405","unused global '"+kv.first+"'","remove or reference it", -1, -1));
            }
        }
    }
    return res; }

} // namespace edn
