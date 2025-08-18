#include <iostream>
#include <exception>

void run_phase5_opt_presets_test();
void run_phase5_debug_info_preserve_test();
void run_phase5_pipeline_override_test();
void run_phase5_pipeline_fallback_test();
void run_phase5_verify_ir_test();

int main(){
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
    }catch(const std::exception& e){ std::cerr << "[phase5] exception: " << e.what() << "\n"; return 1; }
    return 0;
}
