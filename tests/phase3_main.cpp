#include <iostream>

void run_phase3_pointer_arith_tests();
void run_phase3_addr_deref_tests();
void run_phase3_fnptr_tests();
int run_phase3_typedef_tests();
int run_phase3_enum_tests();
void phase3_for_continue_tests();
void run_phase3_switch_tests();
int run_phase3_union_tests();
int run_phase3_variadic_tests();
int run_phase3_variadic_runtime_test();
int run_phase3_cast_sugar_test();
void run_phase3_diagnostics_notes_tests();
int run_phase3_diagnostics_json_tests();
void run_phase3_examples_smoke();

int main(){
    run_phase3_pointer_arith_tests();
    run_phase3_addr_deref_tests();
    run_phase3_fnptr_tests();
    (void)run_phase3_typedef_tests();
    (void)run_phase3_enum_tests();
    phase3_for_continue_tests();
    run_phase3_switch_tests();
    (void)run_phase3_union_tests();
    (void)run_phase3_variadic_tests();
    (void)run_phase3_variadic_runtime_test();
    (void)run_phase3_cast_sugar_test();
    run_phase3_diagnostics_notes_tests();
    (void)run_phase3_diagnostics_json_tests();
    run_phase3_examples_smoke();
    std::cout << "[phase3] All tests passed\n";
    return 0;
}
