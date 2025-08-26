#include <iostream>
#include "test_env.hpp"
#include <exception>
// Integrate GoogleTest tests that are compiled into this binary (we only link GTest::gtest, not gtest_main)
// so we provide an explicit invocation. This lets us keep the existing manual phase5 smoke harness while
// also exercising any GTest-based regressions compiled into the target (e.g. complex lowering).
#include <gtest/gtest.h>

void run_phase5_opt_presets_test();
void run_phase5_debug_info_preserve_test();
void run_phase5_pipeline_override_test();
void run_phase5_pipeline_fallback_test();
void run_phase5_verify_ir_test();
void run_phase5_pegtl_smoke_test();
int run_phase5_resolver_slot_alias_test();
void run_phase5_slot_lazy_init_test();

int main(int argc, char** argv){
    try{
        run_phase5_opt_presets_test();
    // Pipeline override wins
    run_phase5_pipeline_override_test();
    // Pipeline parse fallback to presets
    run_phase5_pipeline_fallback_test();
    // DI preservation under O1
    run_phase5_debug_info_preserve_test();
    // Verify IR hook
    run_phase5_verify_ir_test();
    // PEGTL smoke
    run_phase5_pegtl_smoke_test();
    // Resolver slot alias regression
    run_phase5_resolver_slot_alias_test();
    // Lazy slot initialization regression (synthetic const+bitcast)
    run_phase5_slot_lazy_init_test();
    }catch(const std::exception& e){ std::cerr << "[phase5] exception: " << e.what() << "\n"; return 1; }

    // Now dispatch any GoogleTest-based tests that were compiled into this binary.
    // (If none were compiled, RUN_ALL_TESTS will simply report 0 tests.)
    ::testing::InitGoogleTest(&argc, argv);
    int gtest_result = RUN_ALL_TESTS();
    return gtest_result; // propagate failure if any GTest test fails
}
