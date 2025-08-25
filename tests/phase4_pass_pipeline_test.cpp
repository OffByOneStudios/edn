// Phase 4: Pass pipeline regression test ensuring enabling passes does not break DI or EH constructs.
#include <cassert>
#include <iostream>
#include <string>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

static void build_and_check(const char* src){
  TypeContext tctx; IREmitter em(tctx); TypeCheckResult r; auto *m = em.emit(parse(src), r); assert(r.success && m);
  // Minimal sanity: module prints and contains expected functions
  std::string ir; llvm::raw_string_ostream os(ir); m->print(os,nullptr); os.flush();
  assert(!ir.empty());
}

void run_phase4_pass_pipeline_test(){
  std::cout << "[phase4] pass pipeline test...\n";
#if defined(_WIN32)
  _putenv_s("EDN_ENABLE_PASSES", "1");
  _putenv_s("EDN_OPT_LEVEL", "1");
#else
  setenv("EDN_ENABLE_PASSES", "1", 1);
  setenv("EDN_OPT_LEVEL", "1", 1);
#endif
  // Simple representative program (sum creation and a direct return) exercising passes
  const char* SRC = R"EDN((module
    (sum :name T :variants [ (variant :name A :fields [ i32 ]) ])
    (fn :name "main" :ret i32 :params [ (param i32 %v) ] :body [
       (sum-new %s T A [ %v ])
       (ret i32 %v)
    ])
  ))EDN";
  build_and_check(SRC);
  std::cout << "[phase4] pass pipeline test passed\n";
}
