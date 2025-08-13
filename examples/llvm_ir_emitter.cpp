// Refactored example: use IRBuilderBackend abstraction so we can later
// swap in a real LLVM ORC JIT backend without changing traversal logic.
#include <iostream>
#include "edn/edn.hpp"
#include "edn/transform.hpp"
#include "edn/ir.hpp"

// Example: EDN representation of a tiny LLVM-like IR module.
// This now routes emission through TextLLVMBackend implementing IRBuilderBackend.
// Supported instruction forms (Phase 1):
//  (add|sub|mul|sdiv %dst ty op1 op2)
//  (const %dst ty literal)
//  (ret ty val)

using namespace edn;

static std::string emit_module(const node_ptr& module_ast){
    using namespace edn;
    Transformer tr;
    TextLLVMBackend backend;

    auto symbol_name = [](const node_ptr& n)->std::string{ if(std::holds_alternative<symbol>(n->data)) return std::get<symbol>(n->data).name; return {}; };
    auto string_value = [](const node_ptr& n)->std::string{ if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; };

    tr.add_visitor("module", [&](node& n, list& l, const symbol&){
        ModuleInfo minfo{ "edn_module", "edn_module", "x86_64-pc-windows-msvc", "e-m:x-p270:32:32-p271:32:32-p272:64:64-i64:64-n8:16:32:64-S128" };
        size_t i=1;
        while(i+1<l.elems.size() && l.elems[i] && std::holds_alternative<keyword>(l.elems[i]->data)){
            std::string k = std::get<keyword>(l.elems[i]->data).name; auto val=l.elems[i+1];
            if(k=="id") minfo.id = string_value(val);
            else if(k=="source") minfo.source = string_value(val);
            else if(k=="triple") minfo.triple = string_value(val);
            else if(k=="datalayout") minfo.dataLayout = string_value(val);
            i += 2;
        }
        backend.beginModule(minfo);
        for(; i<l.elems.size(); ++i){
            auto& fn_node = l.elems[i];
            if(!fn_node || !std::holds_alternative<list>(fn_node->data)) continue;
            auto& fn_list = std::get<list>(fn_node->data);
            if(fn_list.elems.empty()) continue;
            if(!std::holds_alternative<symbol>(fn_list.elems[0]->data)) continue;
            if(std::get<symbol>(fn_list.elems[0]->data).name != "fn") continue;
            FunctionInfo finfo; finfo.returnType = "void"; finfo.name = "fn";
            std::vector<node_ptr> body;
            for(size_t j=1;j<fn_list.elems.size(); ++j){
                auto& cur = fn_list.elems[j];
                if(cur && std::holds_alternative<keyword>(cur->data)){
                    std::string k = std::get<keyword>(cur->data).name; if(++j>=fn_list.elems.size()) break; auto val = fn_list.elems[j];
                    if(k=="name") finfo.name = string_value(val);
                    else if(k=="ret") finfo.returnType = symbol_name(val);
                    else if(k=="params"){
                        if(val && std::holds_alternative<vector_t>(val->data)){
                            for(auto& p : std::get<vector_t>(val->data).elems){
                                if(!p || !std::holds_alternative<list>(p->data)) continue; auto& pl = std::get<list>(p->data);
                                if(pl.elems.size()==3 && std::holds_alternative<symbol>(pl.elems[0]->data) && std::get<symbol>(pl.elems[0]->data).name=="param"){
                                    std::string ty = symbol_name(pl.elems[1]); std::string vs = symbol_name(pl.elems[2]); if(!vs.empty() && vs[0]=='%') vs = vs.substr(1); finfo.params.push_back(ParamInfo{ty, vs});
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
            backend.beginFunction(finfo);
            // translate instruction nodes
            for(auto& inst_node : body){
                if(!inst_node || !std::holds_alternative<list>(inst_node->data)) continue;
                auto& il = std::get<list>(inst_node->data);
                if(il.elems.empty() || !std::holds_alternative<symbol>(il.elems[0]->data)) continue;
                std::string op = std::get<symbol>(il.elems[0]->data).name;
                auto want=[&](size_t idx){ return idx<il.elems.size()? il.elems[idx]: node_ptr{}; };
                auto sym_or=[&](const node_ptr& nn){ if(!nn) return std::string{}; if(std::holds_alternative<symbol>(nn->data)) return std::get<symbol>(nn->data).name; if(std::holds_alternative<int64_t>(nn->data)) return std::to_string(std::get<int64_t>(nn->data)); return std::string{}; };
                Instruction inst; inst.opcode=op;
                if(op=="add"||op=="sub"||op=="mul"||op=="sdiv"){
                    if(il.elems.size()==5){ inst.result = sym_or(want(1)); inst.type = sym_or(want(2)); inst.args.push_back(sym_or(want(3))); inst.args.push_back(sym_or(want(4))); }
                } else if(op=="const"){
                    if(il.elems.size()==4){ inst.result = sym_or(want(1)); inst.type = sym_or(want(2)); inst.args.push_back(sym_or(want(3))); }
                } else if(op=="ret"){
                    if(il.elems.size()==3){ inst.args.push_back(sym_or(want(1))); inst.args.push_back(sym_or(want(2))); }
                }
                backend.emitInstruction(inst);
            }
            backend.endFunction();
        }
        backend.endModule();
        // store serialized output into metadata for retrieval if needed
        n.metadata["text-ir"] = std::make_shared<node>(node{ std::string{ backend.str() }, {} });
    });

    tr.traverse(module_ast);
    // retrieve from module metadata
    auto it = module_ast->metadata.find("text-ir");
    if(it!=module_ast->metadata.end() && std::holds_alternative<std::string>(it->second->data)) return std::get<std::string>(it->second->data);
    return {}; // fallback
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
