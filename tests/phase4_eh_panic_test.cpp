#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Casting.h>

using namespace edn;

void run_phase4_eh_panic_test(){
    std::cout << "[phase4] panic minimal test...\n";
    const char* SRC = R"((module
      (fn :name "main" :ret i32 :params [] :body [
        (panic)
        (const %z i32 0) ; unreachable
        (ret i32 %z)
      ])
    ))";
    auto ast = parse(SRC);
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
    if(!tcres.success || !mod){
        std::cerr << "panic test type/emission failed\n";
        for(auto &e: tcres.errors) std::cerr<<e.code<<" "<<e.message<<"\n";
    }
    assert(tcres.success && mod);
    // We don't execute it; just ensure it builds and contains llvm.trap
    bool sawTrap=false;
  for (auto &F : *mod){
    for (auto &BB : F){
      for (auto &I : BB){
        if (auto *CI = llvm::dyn_cast<llvm::CallInst>(&I)){
          if (CI->getCalledFunction() && CI->getCalledFunction()->getName().contains("llvm.trap"))
            sawTrap=true;
        }
      }
    }
  }
    assert(sawTrap);
    std::cout << "[phase4] panic minimal test passed\n";
}
