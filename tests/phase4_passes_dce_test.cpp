#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"

// Prove that enabling the opt pipeline performs dead code elimination (DCE)
// by removing an unused integer add instruction.
void run_phase4_passes_dce_test(){
    using namespace edn;
    std::cout << "[phase4] passes: DCE test...\n";

    const std::string src = R"EDN(
        (module
          (fn :name "dead" :ret i32 :params [ (param i32 %a) (param i32 %b) ]
              :body [
                (add %t i32 %a %b) ; dead: result is never used
                (const %z i32 0)
                (ret i32 %z)
              ]))
    )EDN";

  // Helper to count 'add' instructions in function 'dead' with EDN_ENABLE_PASSES toggled
  auto count_add_with_flag = [&](const char* flag)->int{
#if defined(_WIN32)
        _putenv_s("EDN_ENABLE_PASSES", flag);
#else
        setenv("EDN_ENABLE_PASSES", flag, 1);
#endif
        auto ast = parse(src);
    edn::TypeContext tctx; edn::TypeCheckResult tcres; edn::IREmitter em(tctx);
    auto *m = em.emit(ast, tcres);
    assert(tcres.success && m);
    auto F = m->getFunction("dead");
    assert(F && "function dead should exist");
    int addCount = 0;
    for(auto &bb : *F){ for(auto &ins : bb){ if(std::string(ins.getOpcodeName()) == "add") { ++addCount; } } }
    return addCount;
    };

    // Baseline: passes disabled -> the add should be present
  int addsBefore = count_add_with_flag("0");
  assert(addsBefore >= 1 && "expected an add instruction before optimization");

    // With passes enabled -> the dead add should be eliminated
  int addsAfter = count_add_with_flag("1");
  assert(addsAfter == 0 && "dead add should be removed by optimization pipeline (DCE)");

    std::cout << "[phase4] passes: DCE test passed\n";
}
