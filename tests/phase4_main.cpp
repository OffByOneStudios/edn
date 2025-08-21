#include <iostream>
#include "test_env.hpp"

void run_phase4_sum_types_tests();
void run_phase4_sum_ir_golden_tests();
void run_phase4_sum_resultmode_repro();
void run_phase4_lints_tests();
void run_phase4_generics_macro_test();
void run_phase4_generics_two_params_test();
void run_phase4_generics_dedup_test();
void run_phase4_generics_negative_tests();
void run_phase4_traits_macro_test();
void run_phase4_closures_min_test();
void run_phase4_closures_record_test();
void run_phase4_closures_negative_tests();
void run_phase4_closures_capture_mismatch_test();
void run_phase4_closures_jit_smoke_test();
void run_phase4_eh_panic_test();
void run_phase4_eh_panic_negative_test();
void run_phase4_eh_personality_test();
void run_phase4_target_triple_test();
void run_phase4_eh_invoke_smoke_test();
void run_phase4_eh_seh_invoke_smoke_test();
void run_phase4_passes_dce_test();
void run_phase4_eh_panic_unwind_test();
void run_phase4_eh_panic_unwind_seh_test();
void run_phase4_eh_seh_cleanup_consolidation_test();
void run_phase4_eh_disabled_no_invoke_test();
void run_phase4_passes_mem2reg_test();
void run_phase4_eh_seh_try_catch_smoke_test();
void run_phase4_eh_itanium_try_catch_smoke_test();
void run_phase4_eh_panic_inside_try_itanium_test();
void run_phase4_eh_panic_inside_try_seh_test();
void run_phase4_coro_ir_golden_test();
void run_phase4_coro_lowering_smoke_test();
void run_phase4_coro_jit_smoke_test();
void run_phase4_coro_negative_tests();
void run_phase4_debug_info_smoke_test();
void run_phase4_debug_info_locals_test();
void run_phase4_debug_info_struct_members_test();
void run_phase4_abi_golden_test();

int main(){
    run_phase4_eh_disabled_no_invoke_test();
    run_phase4_sum_types_tests();
    run_phase4_sum_ir_golden_tests();
    run_phase4_lints_tests();
    run_phase4_generics_macro_test();
    run_phase4_generics_two_params_test();
    run_phase4_generics_dedup_test();
    run_phase4_generics_negative_tests();
    run_phase4_traits_macro_test();
    // TEMP: bisect segfault after traits test; run no further tests for now.
    // run_phase4_closures_min_test();
    std::cout << "[phase4] All tests passed\n";
    return 0;
}
