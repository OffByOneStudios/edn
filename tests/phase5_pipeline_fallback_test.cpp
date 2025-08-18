#include <cassert>
#include <iostream>
#include <string>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

void run_phase5_pipeline_fallback_test(){
    std::cout << "[phase5] pipeline parse fallback test...\n";
    const char* SRC = R"EDN((module
      (fn :name "dead2" :ret i32 :params [ (param i32 %a) ] :body [
         (const %z i32 0)
         (add %d i32 %a %z)
         (ret i32 %a)
      ])
    ))EDN";
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_PASSES", "1");
    _putenv_s("EDN_OPT_LEVEL", "1");
    _putenv_s("EDN_PASS_PIPELINE", "not-a-valid-pipeline");
#else
    setenv("EDN_ENABLE_PASSES", "1", 1);
    setenv("EDN_OPT_LEVEL", "1", 1);
    setenv("EDN_PASS_PIPELINE", "not-a-valid-pipeline", 1);
#endif
    // Should fall back to presets and still succeed
    TypeContext tctx; IREmitter em(tctx); TypeCheckResult r; auto *m = em.emit(parse(SRC), r);
    assert(r.success && m);
    assert(m->getFunction("dead2") != nullptr);
    std::cout << "[phase5] pipeline parse fallback test passed\n";
}
