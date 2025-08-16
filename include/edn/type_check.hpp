// Type checker scaffold (Phase 1)
#pragma once
#include "edn/edn.hpp"
#include "edn/types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace edn {

struct TypeNote { std::string message; int line=-1; int col=-1; };
struct TypeError { std::string code; std::string message; std::string hint; int line=-1; int col=-1; std::vector<TypeNote> notes; };
struct TypeWarning { std::string code; std::string message; std::string hint; int line=-1; int col=-1; std::vector<TypeNote> notes; };

// M6 Diagnostics: central reporter (lightweight wrapper so future components share formatting)
struct ErrorReporter {
    std::vector<TypeError>* errors=nullptr;
    std::vector<TypeWarning>* warnings=nullptr;
    void emit_error(const TypeError& e){ if(errors) errors->push_back(e); }
    void emit_warning(const TypeWarning& w){ if(warnings) warnings->push_back(w); }
    TypeError make_error(std::string code, std::string message, std::string hint, int line, int col){ return TypeError{std::move(code),std::move(message),std::move(hint),line,col,{}}; }
    TypeWarning make_warning(std::string code, std::string message, std::string hint, int line, int col){ return TypeWarning{std::move(code),std::move(message),std::move(hint),line,col,{}}; }
};

struct TypeCheckResult { bool success; std::vector<TypeError> errors; std::vector<TypeWarning> warnings; };

struct FieldInfo { std::string name; TypeId type; size_t index; };
struct StructInfo { std::string name; std::vector<FieldInfo> fields; std::unordered_map<std::string,FieldInfo*> field_map; };
// Union: overlapping fields (Phase 3). For now only base scalar field types allowed (simplifies layout sizing in emitter).
struct UnionFieldInfo { std::string name; TypeId type; };
struct UnionInfo { std::string name; std::vector<UnionFieldInfo> fields; std::unordered_map<std::string,UnionFieldInfo*> field_map; };
struct ParamInfoTC { std::string name; TypeId type; };
struct FunctionInfoTC { std::string name; TypeId ret; std::vector<ParamInfoTC> params; bool variadic=false; bool external=false; };
struct GlobalInfoTC { std::string name; TypeId type; bool is_const=false; node_ptr init; };
// Sum types (tagged unions / ADTs)
struct SumVariantInfo { std::string name; std::vector<TypeId> fields; };
struct SumInfo { std::string name; std::vector<SumVariantInfo> variants; std::unordered_map<std::string, SumVariantInfo*> variant_map; };

class TypeChecker {
public:
    explicit TypeChecker(TypeContext& ctx): ctx_(ctx){}
    TypeCheckResult check_module(const node_ptr& module_ast);
private:
    TypeContext& ctx_;
    void error(TypeCheckResult& r, const node& n, std::string msg);
    void warn(TypeCheckResult& r, const node& n, std::string msg);
    void error_code(TypeCheckResult& r, const node& n, std::string code, std::string msg, std::string hint="");
    // Enhanced mismatch emission: attaches expected/actual notes (Phase 3 diagnostics upgrade)
    void type_mismatch(TypeCheckResult& r, const node& n, const std::string& code,
                       const std::string& role, TypeId expected, TypeId actual){
        ErrorReporter rep{&r.errors,&r.warnings};
        std::string expStr = ctx_.to_string(expected);
        std::string actStr = ctx_.to_string(actual);
        auto err = rep.make_error(code, role+" type mismatch", "ensure "+role+" has type "+expStr, line(n), col(n));
        err.notes.push_back(TypeNote{"expected: "+expStr, line(n), col(n)});
        err.notes.push_back(TypeNote{"   found: "+actStr, line(n), col(n)});
        rep.emit_error(err);
    }
    // M6 extended helpers (unused yet) for operand-specific diagnostics
    void operand_type_error(TypeCheckResult& r, const node& n, const std::string& code, const std::string& operandRole, const std::string& expected, const std::string& actual){
        error_code(r,n,code, operandRole+" type mismatch: expected "+expected+" got "+actual, "ensure "+operandRole+" expression has type "+expected);
    }
    // symbol tables (cleared per module)
    std::unordered_map<std::string, StructInfo> structs_;
    std::unordered_map<std::string, UnionInfo> unions_;
    std::unordered_map<std::string, SumInfo> sums_;
    std::unordered_map<std::string, FunctionInfoTC> functions_;
    std::unordered_map<std::string, GlobalInfoTC> globals_;
    // typedef aliases: name -> underlying TypeId
    std::unordered_map<std::string, TypeId> typedefs_;
    // enums: name -> underlying TypeId, plus constants map (flattened constant name -> (TypeId,value))
    struct EnumInfo { std::string name; TypeId underlying; std::unordered_map<std::string,int64_t> constants; };
    std::unordered_map<std::string, EnumInfo> enums_;
    std::unordered_map<std::string, std::pair<TypeId,int64_t>> enum_constants_; // constant -> (type,value)
    // per-function variable types (params + instruction results)
    std::unordered_map<std::string, TypeId> var_types_;
    void reset();
    void collect_structs(TypeCheckResult& r, const std::vector<node_ptr>& elems);
    void collect_unions(TypeCheckResult& r, const std::vector<node_ptr>& elems);
    void collect_sums(TypeCheckResult& r, const std::vector<node_ptr>& elems);
    void collect_globals(TypeCheckResult& r, const std::vector<node_ptr>& elems);
    void collect_typedefs(TypeCheckResult& r, const std::vector<node_ptr>& elems);
    void collect_enums(TypeCheckResult& r, const std::vector<node_ptr>& elems);
    void collect_functions_headers(TypeCheckResult& r, const std::vector<node_ptr>& elems);
    void check_functions(TypeCheckResult& r, const std::vector<node_ptr>& elems);
    bool parse_struct(TypeCheckResult& r, const node_ptr& n);
    bool parse_function_header(TypeCheckResult& r, const node_ptr& fn_list, FunctionInfoTC& out_fn);
    void check_function_body(TypeCheckResult& r, const node_ptr& fn_node, const FunctionInfoTC& fn_info);
    // Lints (M4.10):
    // - W1400: top-level unreachable after return
    // - W1401: missing top-level return in non-void function
    // - W1402: unreachable code inside nested blocks after ret/break/continue
    // - W1403: unused variable (defined but never used)
    // - W1404: unused parameter
    void analyze_fn_lints(TypeCheckResult& r, const std::vector<node_ptr>& insts, const FunctionInfoTC& fn_info);
    TypeId parse_type_node(const node_ptr& n, TypeCheckResult& r);
    bool lookup_function(const std::string& name, FunctionInfoTC*& out){ auto it = functions_.find(name); if(it==functions_.end()) return false; out=&it->second; return true; }
    bool lookup_global(const std::string& name, GlobalInfoTC*& out){ auto it=globals_.find(name); if(it==globals_.end()) return false; out=&it->second; return true; }
    void check_instruction_list(TypeCheckResult& r, const std::vector<node_ptr>& insts, const FunctionInfoTC& fn_info, int loop_depth=0);
    // M6 suggestion helpers (implemented inline for header-only distribution)
    int edit_distance(const std::string& a, const std::string& b);
    std::vector<std::string> fuzzy_candidates(const std::string& target, const std::vector<std::string>& pool, int maxDist=2);
    void append_suggestions(TypeError& err, const std::vector<std::string>& suggs);
};

} // namespace edn
