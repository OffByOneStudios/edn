// Type checker scaffold (Phase 1)
#pragma once
#include "edn/edn.hpp"
#include "edn/types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace edn {

struct TypeError { std::string message; int line=-1; int col=-1; };

struct TypeCheckResult { bool success; std::vector<TypeError> errors; };

struct FieldInfo { std::string name; TypeId type; size_t index; };
struct StructInfo { std::string name; std::vector<FieldInfo> fields; std::unordered_map<std::string,FieldInfo*> field_map; };
struct ParamInfoTC { std::string name; TypeId type; };
struct FunctionInfoTC { std::string name; TypeId ret; std::vector<ParamInfoTC> params; };
struct GlobalInfoTC { std::string name; TypeId type; };

class TypeChecker {
public:
    explicit TypeChecker(TypeContext& ctx): ctx_(ctx){}
    TypeCheckResult check_module(const node_ptr& module_ast);
private:
    TypeContext& ctx_;
    void error(TypeCheckResult& r, const node& n, std::string msg);
    // symbol tables (cleared per module)
    std::unordered_map<std::string, StructInfo> structs_;
    std::unordered_map<std::string, FunctionInfoTC> functions_;
    std::unordered_map<std::string, GlobalInfoTC> globals_;
    // per-function variable types (params + instruction results)
    std::unordered_map<std::string, TypeId> var_types_;
    void reset();
    void collect_structs(TypeCheckResult& r, const std::vector<node_ptr>& elems);
    void collect_globals(TypeCheckResult& r, const std::vector<node_ptr>& elems);
    void collect_functions_headers(TypeCheckResult& r, const std::vector<node_ptr>& elems);
    void check_functions(TypeCheckResult& r, const std::vector<node_ptr>& elems);
    bool parse_struct(TypeCheckResult& r, const node_ptr& n);
    bool parse_function_header(TypeCheckResult& r, const node_ptr& fn_list, FunctionInfoTC& out_fn);
    void check_function_body(TypeCheckResult& r, const node_ptr& fn_node, const FunctionInfoTC& fn_info);
    TypeId parse_type_node(const node_ptr& n, TypeCheckResult& r);
    bool lookup_function(const std::string& name, FunctionInfoTC*& out){ auto it = functions_.find(name); if(it==functions_.end()) return false; out=&it->second; return true; }
    bool lookup_global(const std::string& name, GlobalInfoTC*& out){ auto it=globals_.find(name); if(it==globals_.end()) return false; out=&it->second; return true; }
    void check_instruction_list(TypeCheckResult& r, const std::vector<node_ptr>& insts, const FunctionInfoTC& fn_info, int loop_depth=0);
};

} // namespace edn
