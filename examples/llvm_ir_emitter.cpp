#include <iostream>
#include "edn/edn.hpp"
#include "edn/transform.hpp"

// Example: EDN representation of a tiny LLVM-like IR module.
// We emit a pseudo LLVM IR string without depending on LLVM libraries.
// EDN schema:
// (module (fn :name "add" :ret i32 :params [(param i32 %a) (param i32 %b)] :body [ (add %res i32 %a %b) (ret i32 %res) ]) ...)
// Supported instruction forms: (add|sub|mul|sdiv %dst ty op1 op2), (const %dst ty literal), (ret ty val)

using namespace edn;

static std::string emit_module(const node_ptr& module_ast){
    std::string ir_output;
    Transformer tr;
    // Defaults (can be overridden by module keywords)
    std::string mod_id = "edn_module";
    std::string mod_source = mod_id;
    std::string target_triple = "x86_64-pc-windows-msvc"; // pick a reasonable default
    std::string data_layout = "e-m:x-p270:32:32-p271:32:32-p272:64:64-i64:64-n8:16:32:64-S128"; // simplified

    auto symbol_name = [](const node_ptr& n)->std::string{
        if(std::holds_alternative<symbol>(n->data)) return std::get<symbol>(n->data).name; return {}; };
    auto string_value = [](const node_ptr& n)->std::string{
        if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; };

    auto emit_instruction = [&](const list& inst_list)->std::string {
        if(inst_list.elems.empty()) return "";
        if(!std::holds_alternative<symbol>(inst_list.elems[0]->data)) return "";
        std::string op = std::get<symbol>(inst_list.elems[0]->data).name;
        auto want = [&](size_t idx){ return idx < inst_list.elems.size() ? inst_list.elems[idx] : node_ptr{}; };
        auto sym_or = [&](const node_ptr& n){
            if(!n) return std::string{};
            if(std::holds_alternative<symbol>(n->data)) return std::get<symbol>(n->data).name;
            if(std::holds_alternative<int64_t>(n->data)) return std::to_string(std::get<int64_t>(n->data));
            return std::string{};
        };
        if(op=="add"||op=="sub"||op=="mul"||op=="sdiv"){
            if(inst_list.elems.size()!=5) return "; bad arity for binop\n";
            auto d = sym_or(want(1)); auto ty = sym_or(want(2)); auto a = sym_or(want(3)); auto b = sym_or(want(4));
            return "  "+d+" = "+op+" "+ty+" "+a+", "+b+"\n"; // insert required comma
        }
        if(op=="const"){
            // (const %dst ty literal) -> 4 elements total
            if(inst_list.elems.size()!=4) return "; bad const arity\n";
            auto d = sym_or(want(1)); auto ty = sym_or(want(2)); auto lit = sym_or(want(3));
            return "  "+d+" = add "+ty+" 0, "+lit+"\n"; // pseudo constant materialization
        }
        if(op=="ret"){
            if(inst_list.elems.size()!=3) return "; bad ret arity\n";
            auto ty = sym_or(want(1)); auto val = sym_or(want(2));
            return "  ret "+ty+" "+val+"\n";
        }
        return std::string{"  ; unknown op "}+op+"\n";
    };

    tr.add_visitor("module", [&](node& n, list& l, const symbol&){
        // First pass: gather leading keyword pairs (module metadata)
        size_t i = 1;
        while(i+1 < l.elems.size() && l.elems[i] && std::holds_alternative<keyword>(l.elems[i]->data)){
            std::string k = std::get<keyword>(l.elems[i]->data).name;
            auto val = l.elems[i+1];
            if(k=="id") mod_id = string_value(val);
            else if(k=="source") mod_source = string_value(val);
            else if(k=="triple") target_triple = string_value(val);
            else if(k=="datalayout") data_layout = string_value(val);
            i += 2;
        }
        // Emit header now
        ir_output += "; ModuleID = '"+mod_id+"'\n";
        ir_output += "source_filename = \""+mod_source+"\"\n";
        ir_output += "target triple = \""+target_triple+"\"\n";
        ir_output += "target datalayout = \""+data_layout+"\"\n\n";
        // Remaining elements from i onward should be fn forms
        for(; i<l.elems.size(); ++i){
            auto& fn_node = l.elems[i];
            if(!fn_node || !std::holds_alternative<list>(fn_node->data)) continue;
            auto& fn_list = std::get<list>(fn_node->data);
            if(fn_list.elems.empty()) continue;
            if(!std::holds_alternative<symbol>(fn_list.elems[0]->data)) continue;
            if(std::get<symbol>(fn_list.elems[0]->data).name != "fn") continue;
            std::string name; std::string ret_type="void"; std::vector<std::pair<std::string,std::string>> params; std::vector<node_ptr> body;
            for(size_t j=1;j<fn_list.elems.size(); ++j){
                auto& cur = fn_list.elems[j];
                if(cur && std::holds_alternative<keyword>(cur->data)){
                    std::string k = std::get<keyword>(cur->data).name; if(++j>=fn_list.elems.size()) break; auto val = fn_list.elems[j];
                    if(k=="name") name = string_value(val);
                    else if(k=="ret") ret_type = symbol_name(val);
                    else if(k=="params"){
                        if(val && std::holds_alternative<vector_t>(val->data)){
                            for(auto& p : std::get<vector_t>(val->data).elems){
                                if(!p || !std::holds_alternative<list>(p->data)) continue; auto& pl = std::get<list>(p->data);
                                if(pl.elems.size()==3 && std::holds_alternative<symbol>(pl.elems[0]->data) && std::get<symbol>(pl.elems[0]->data).name=="param"){
                                    std::string ty = symbol_name(pl.elems[1]); std::string vs = symbol_name(pl.elems[2]); if(!vs.empty() && vs[0]=='%') vs = vs.substr(1); params.emplace_back(ty, vs);
                                }
                            }
                        }
                    } else if(k=="body"){
                        if(val && std::holds_alternative<vector_t>(val->data)){
                            for(auto& inst : std::get<vector_t>(val->data).elems) body.push_back(inst);
                        }
                    }
                }
            }
            ir_output += "define "+ret_type+" @"+name+"(";
            for(size_t pi=0; pi<params.size(); ++pi){ if(pi) ir_output += ", "; ir_output += params[pi].first+" %"+params[pi].second; }
            ir_output += ") {\nentry:\n";
            for(auto& inst_node : body){ if(inst_node && std::holds_alternative<list>(inst_node->data)) ir_output += emit_instruction(std::get<list>(inst_node->data)); }
            ir_output += "}\n\n";
        }
    });

    tr.traverse(module_ast);
    return ir_output;
}

int main(){
    const char* edn_module = R"((module
        :id "example"
        :source "example.edn"
        :triple "x86_64-pc-linux-gnu"
        :datalayout "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-n8:16:32:64-S128"
        (fn :name "add" :ret i32 :params [(param i32 %a) (param i32 %b)]
            :body [ (add %res i32 %a %b) (ret i32 %res) ])
        (fn :name "main" :ret i32 :params []
            :body [ (const %c1 i32 40) (const %c2 i32 2) (add %sum i32 %c1 %c2) (ret i32 %sum) ])
    ))";
    auto ast = parse(edn_module);
    std::string ir = emit_module(ast);
    std::cout << "; Pseudo LLVM IR generated from EDN\n" << ir;
    return 0;
}
