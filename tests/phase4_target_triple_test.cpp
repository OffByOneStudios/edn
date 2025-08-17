#include <cassert>
#include <iostream>
#include <cstdlib>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

void run_phase4_target_triple_test(){
    std::cout << "[phase4] target triple override test...\n";
    const char* SRC = R"((module
      (fn :name "t" :ret i32 :params [] :body [ (const %z i32 0) (ret i32 %z) ])
    ))";
    auto ast = parse(SRC);
    _putenv("EDN_TARGET_TRIPLE=x86_64-apple-darwin");
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
    assert(tcres.success && mod);
  {
    auto triple = mod->getTargetTriple();
    assert(triple.find("apple-darwin") != std::string::npos);
  }
    _putenv("EDN_TARGET_TRIPLE=");
    std::cout << "[phase4] target triple override test passed\n";
}
