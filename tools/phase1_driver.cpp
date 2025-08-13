#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Error.h>

using namespace edn;

static std::string read_file(const std::string& path){ std::ifstream ifs(path); std::stringstream ss; ss<<ifs.rdbuf(); return ss.str(); }

int main(int argc, char** argv){
    if(argc<2){ std::cerr << "usage: phase1_driver <edn-file> [function=main]\n"; return 1; }
    std::string file = argv[1];
    std::string entry = "main";
    if(argc>2) entry = argv[2];
    std::string src = read_file(file); if(src.empty()){ std::cerr << "failed to read file\n"; return 1; }
    auto ast = parse(src);
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; (void)emitter.emit(ast, tcres);
    if(!tcres.success){ std::cerr << "Type check failed:\n"; for(auto &e: tcres.errors) std::cerr << e.message << "\n"; return 2; }
    llvm::InitializeNativeTarget(); llvm::InitializeNativeTargetAsmPrinter();
    auto jitExp = llvm::orc::LLJITBuilder().create(); if(!jitExp){ std::cerr << "Failed to create JIT\n"; return 3; }
    auto jit = std::move(*jitExp);
    if(auto err = jit->addIRModule(emitter.toThreadSafeModule())){ std::cerr << "Failed to add module\n"; return 4; }
    auto sym = jit->lookup(entry);
    if(!sym){ std::cerr << "Entry function not found: " << entry << "\n"; return 5; }
    using FnTy = int(*)(void);
    auto fn = reinterpret_cast<FnTy>(sym->toPtr<void*>());
    int result = fn();
    std::cout << "Result: " << result << "\n";
    return 0;
}
