#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/diagnostics_json.hpp"

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/TargetSelect.h>

using namespace edn;

static std::string read_file(const std::string& path){ std::ifstream ifs(path); std::stringstream ss; ss<<ifs.rdbuf(); return ss.str(); }

int main(int argc, char** argv){
    if(argc<2){ std::cerr << "usage: phase3_driver <edn-file> [function=main]\n"; return 1; }
    std::string file = argv[1];
    std::string entry = (argc>2)? argv[2] : std::string("main");
    std::string src = read_file(file); if(src.empty()){ std::cerr << "failed to read file\n"; return 1; }
    auto ast = parse(src);
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; (void)emitter.emit(ast, tcres);
    auto print_diagnostics=[&](){
        for(auto &e : tcres.errors){
            std::cerr << "error"; if(!e.code.empty()) std::cerr << "["<<e.code<<"]"; std::cerr << ": " << e.message; if(e.line>=0) std::cerr << " (line "<<e.line<<":"<<e.col<<")"; std::cerr << "\n"; if(!e.hint.empty()) std::cerr << "  hint: " << e.hint << "\n"; for(auto &n : e.notes){ std::cerr << "  note: " << n.message; if(n.line>=0) std::cerr << " (line "<<n.line<<":"<<n.col<<")"; std::cerr << "\n"; } }
        for(auto &w : tcres.warnings){
            std::cerr << "warning"; if(!w.code.empty()) std::cerr << "["<<w.code<<"]"; std::cerr << ": " << w.message; if(w.line>=0) std::cerr << " (line "<<w.line<<":"<<w.col<<")"; std::cerr << "\n"; if(!w.hint.empty()) std::cerr << "  hint: " << w.hint << "\n"; for(auto &n : w.notes){ std::cerr << "  note: " << n.message; if(n.line>=0) std::cerr << " (line "<<n.line<<":"<<n.col<<")"; std::cerr << "\n"; } }
    };
    if(!tcres.success){ std::cerr << "Type check failed:\n"; print_diagnostics(); return 2; }
    if(!tcres.warnings.empty()) print_diagnostics();
    llvm::InitializeNativeTarget(); llvm::InitializeNativeTargetAsmPrinter();
    auto jitExp = llvm::orc::LLJITBuilder().create(); if(!jitExp){ std::cerr << "Failed to create JIT\n"; return 3; }
    auto jit = std::move(*jitExp);
    // Install symbol resolver: first queries JIT dylibs, then host process (for malloc/free, etc.)
    jit->getMainJITDylib().addGenerator(llvm::cantFail(
        llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            jit->getDataLayout().getGlobalPrefix())));
    if(auto err = jit->addIRModule(emitter.toThreadSafeModule())){ std::cerr << "Failed to add module\n"; return 4; }
    auto sym = jit->lookup(entry); if(!sym){ std::cerr << "Entry function not found: " << entry << "\n"; return 5; }
    using FnTy = int(*)(void); auto fn = reinterpret_cast<FnTy>(sym->toPtr<void*>()); int result = fn();
    std::cout << "Result: " << result << "\n"; return 0; }
