#include <iostream>
#include <cstdlib>

static bool env_enabled(const char* name){
    if(const char* v = std::getenv(name)){
        return v[0]=='1'||v[0]=='y'||v[0]=='Y'||v[0]=='t'||v[0]=='T';
    }
    return false;
}

void run_phase4_sum_types_tests();
void run_phase4_sum_ir_golden_tests();
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
    // Core Phase 4
    run_phase4_eh_disabled_no_invoke_test();
    run_phase4_sum_types_tests();
    run_phase4_sum_ir_golden_tests();
    run_phase4_lints_tests();
    run_phase4_generics_macro_test();
    run_phase4_generics_two_params_test();
    run_phase4_generics_dedup_test();
    run_phase4_generics_negative_tests();
    run_phase4_traits_macro_test();

    // Closures (IR + negative)
    run_phase4_closures_min_test();
    run_phase4_closures_record_test();
    run_phase4_closures_negative_tests();
    run_phase4_closures_capture_mismatch_test();

    // Passes demos
    run_phase4_passes_dce_test();
    run_phase4_passes_mem2reg_test();

    // Debug info
    run_phase4_debug_info_smoke_test();
    run_phase4_debug_info_locals_test();
    run_phase4_debug_info_struct_members_test();

    // EH matrix
    run_phase4_eh_panic_test();
    run_phase4_eh_panic_negative_test();
    run_phase4_eh_personality_test();
    run_phase4_target_triple_test();
    run_phase4_eh_invoke_smoke_test();
    run_phase4_eh_seh_invoke_smoke_test();
    run_phase4_eh_panic_unwind_test();
    run_phase4_eh_panic_unwind_seh_test();
    run_phase4_eh_seh_cleanup_consolidation_test();
    run_phase4_eh_seh_try_catch_smoke_test();
    run_phase4_eh_itanium_try_catch_smoke_test();
    run_phase4_eh_panic_inside_try_itanium_test();
    run_phase4_eh_panic_inside_try_seh_test();

    // Coroutines (IR + lowering + negatives)
    run_phase4_coro_ir_golden_test();
    run_phase4_coro_lowering_smoke_test();
    run_phase4_coro_negative_tests();

    // ABI reference
    run_phase4_abi_golden_test();

    // Optional JIT smokes that could be flaky in Release; gate behind EDN_RUN_JIT=1
    if(env_enabled("EDN_RUN_JIT")){
        run_phase4_closures_jit_smoke_test();
        run_phase4_coro_jit_smoke_test();
    } else {
        std::cout << "[phase4-full] Skipping JIT smoke tests (set EDN_RUN_JIT=1 to enable)\n";
    }

    std::cout << "[phase4-full] All tests passed\n";
    return 0;
}
