#include <cassert>
#include <iostream>
#include <string>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/IR/Module.h>

using namespace edn;

void run_phase4_debug_info_smoke_test(){
    std::cout << "[phase4] debug info smoke test...\n";
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_DEBUG", "1");
#else
    setenv("EDN_ENABLE_DEBUG", "1", 1);
#endif

    const char* SRC = R"EDN((module
      (fn :name "add" :ret i32 :params [ (param i32 %a) (param i32 %b) ] :body [
         (add %c i32 %a %b)
         (ret i32 %c)
      ])
    ))EDN";

    auto ast = parse(SRC);
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
    assert(tcres.success && mod);

    // Module should have a debug compile unit
    auto *nmd = mod->getNamedMetadata("llvm.dbg.cu");
    assert(nmd && nmd->getNumOperands() >= 1 && "missing DI compile unit");

    // Function should have a DISubprogram and instructions with !dbg
    auto *F = mod->getFunction("add");
    assert(F && F->getSubprogram() && "function missing DISubprogram");
    bool hasDbg = false;
    for(auto &BB : *F){
        for(auto &I : BB){
            if(I.getDebugLoc()){ hasDbg = true; break; }
        }
        if(hasDbg) break;
    }
    assert(hasDbg && "no instruction carried debug location");

    std::cout << "[phase4] debug info smoke test passed\n";
}
