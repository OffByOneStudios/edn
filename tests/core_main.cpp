#include <iostream>

// Core/unit test entry points
void run_type_tests();
void run_type_checker_tests();
void run_ir_emitter_test();
void run_cast_tests();
void run_globals_tests();
void run_diagnostics_tests();

int main(){
    run_type_tests();
    run_type_checker_tests();
    run_ir_emitter_test();
    run_cast_tests();
    run_globals_tests();
    run_diagnostics_tests();
    std::cout << "[core] All core tests passed\n";
    return 0;
}
