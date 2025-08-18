#include <cassert>
#include <iostream>
#include <string>
#include <stdexcept>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

static std::string mod_to_str(llvm::Module* m){ std::string s; llvm::raw_string_ostream os(s); m->print(os, nullptr); os.flush(); return s; }

void run_phase5_pipeline_override_test(){
    std::cout << "[phase5] pipeline override test...\n";
    const char* SRC = R"EDN((module
      (fn :name "f" :ret i32 :params [ (param i32 %a) ] :body [
         (const %z i32 0)
         (add %d i32 %a %z)
         (ret i32 %d)
      ])
    ))EDN";

#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_PASSES", "1");
    _putenv_s("EDN_OPT_LEVEL", "0");
    _putenv_s("EDN_PASS_PIPELINE", "default<O2>");
#else
    setenv("EDN_ENABLE_PASSES", "1", 1);
    setenv("EDN_OPT_LEVEL", "0", 1);
    setenv("EDN_PASS_PIPELINE", "default<O2>", 1);
#endif
    // Even with O0, custom pipeline should run and be able to optimize away %z or fold add
    TypeContext tctx; IREmitter em(tctx); TypeCheckResult r; auto *m = em.emit(parse(SRC), r); assert(r.success && m);
    auto ir = mod_to_str(m);
    // Heuristic: with O2 default pipeline, the add with 0 should fold to %a; ensure the instruction text changes
    if(ir.find("%d = add i32") != std::string::npos){
        throw std::runtime_error("custom pipeline didn't seem to run: add survived at O0 with pipeline override");
    }
    std::cout << "[phase5] pipeline override test passed\n";
}
