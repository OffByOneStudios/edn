// Phase 1 backend interface & simple text LLVM-like emitter.
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace edn {

struct ModuleInfo { std::string id; std::string source; std::string triple; std::string dataLayout; };
struct ParamInfo { std::string type; std::string name; };
struct FunctionInfo { std::string name; std::string returnType; std::vector<ParamInfo> params; };
struct Instruction { std::string opcode; std::string result; std::string type; std::vector<std::string> args; };

class IRBuilderBackend {
public:
    virtual ~IRBuilderBackend() = default;
    virtual void beginModule(const ModuleInfo&) {}
    virtual void endModule() {}
    virtual void beginFunction(const FunctionInfo&) {}
    virtual void emitInstruction(const Instruction&) {}
    virtual void endFunction() {}
};

class TextLLVMBackend : public IRBuilderBackend {
public:
    void beginModule(const ModuleInfo& m) override {
        out_ << "; Pseudo LLVM IR generated from EDN\n";
        out_ << "; ModuleID = '" << m.id << "'\n";
        out_ << "source_filename = \"" << m.source << "\"\n";
        out_ << "target triple = \"" << m.triple << "\"\n";
        out_ << "target datalayout = \"" << m.dataLayout << "\"\n\n";
    }
    void beginFunction(const FunctionInfo& f) override {
        out_ << "define " << f.returnType << " @" << f.name << "(";
        for(size_t i=0;i<f.params.size();++i){ if(i) out_ << ", "; out_ << f.params[i].type << " %" << f.params[i].name; }
        out_ << ") {\nentry:\n";
    }
    void emitInstruction(const Instruction& inst) override {
        if(inst.opcode=="ret"){
            if(inst.args.size()==2) out_ << "  ret " << inst.args[0] << ' ' << inst.args[1] << "\n";
            return;
        }
        if(inst.opcode=="const"){
            if(inst.result.size() && inst.args.size()==1) out_ << "  " << inst.result << " = add " << inst.type << " 0, " << inst.args[0] << "\n"; // pseudo const
            return;
        }
        if(inst.opcode=="add"||inst.opcode=="sub"||inst.opcode=="mul"||inst.opcode=="sdiv"){
            if(inst.result.empty() || inst.type.empty() || inst.args.size()!=2) { out_ << "  ; malformed binop\n"; return; }
            out_ << "  " << inst.result << " = " << inst.opcode << ' ' << inst.type << ' ' << inst.args[0] << ", " << inst.args[1] << "\n";
            return;
        }
        out_ << "  ; unknown op " << inst.opcode << "\n";
    }
    void endFunction() override { out_ << "}\n\n"; }
    std::string str() const { return out_.str(); }
private:
    std::ostringstream out_;
};

} // namespace edn
