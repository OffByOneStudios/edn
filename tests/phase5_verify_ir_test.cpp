#include <cassert>
#include <iostream>
#include <string>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

void run_phase5_verify_ir_test(){
    std::cout << "[phase5] verify IR test...\n";
    const char* SRC = R"EDN((module
      (fn :name "add" :ret i32 :params [ (param i32 %a) (param i32 %b) ] :body [
         (add %c i32 %a %b)
         (ret i32 %c)
      ])
    ))EDN";
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_PASSES", "1");
    _putenv_s("EDN_OPT_LEVEL", "1");
    _putenv_s("EDN_VERIFY_IR", "1");
#else
    setenv("EDN_ENABLE_PASSES", "1", 1);
    setenv("EDN_OPT_LEVEL", "1", 1);
    setenv("EDN_VERIFY_IR", "1", 1);
#endif
    TypeContext tctx; IREmitter em(tctx); TypeCheckResult r; auto *m = em.emit(parse(SRC), r);
    assert(r.success && m);
    std::cout << "[phase5] verify IR test passed\n";
}
