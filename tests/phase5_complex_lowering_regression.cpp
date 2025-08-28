#include <gtest/gtest.h>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <array>
#include <string>

// Simple helper to run the existing jit driver on complex_lowering sample
// and capture its stdout. Ensures EDN-0001 regression (loop termination + correct result = 6).
static std::string run_cmd(const char* cmd){
    std::array<char,256> buf{}; std::string out; FILE* p = popen(cmd, "r"); if(!p) return out; while(fgets(buf.data(), buf.size(), p)){ out += buf.data(); } pclose(p); return out;
}

static std::string locate_driver_reg(){
    const char* candidates[] = {
        "../languages/rustlite/rustlite_jit_driver",    // build/tests -> build/languages
        "../../languages/rustlite/rustlite_jit_driver", // deeper nesting safety
        "languages/rustlite/rustlite_jit_driver",       // repo root (ctest launched there)
        "build/languages/rustlite/rustlite_jit_driver"  // repo root with explicit build path
    };
    for(const char* c : candidates){ if(FILE *f = fopen(c, "rb")){ fclose(f); return c; } }
    return candidates[0];
}
static std::string locate_sample_reg(){
    const char* candidates[] = {
        "../languages/rustlite/samples/complex_lowering.rl.rs",    // build/
        "../../languages/rustlite/samples/complex_lowering.rl.rs", // build/tests
        "languages/rustlite/samples/complex_lowering.rl.rs",       // repo root
        "./languages/rustlite/samples/complex_lowering.rl.rs"      // explicit
    };
    for(const char* c : candidates){ if(FILE *f = fopen(c, "rb")){ fclose(f); return c; } }
    return candidates[0];
}

TEST(Phase5ComplexLowering, TerminatesAndReturnsSix){
    std::string cmd = locate_driver_reg() + std::string(" ") + locate_sample_reg();
    auto output = run_cmd(cmd.c_str());
    ASSERT_NE(output.find("Result: 6"), std::string::npos) << "complex_lowering did not terminate with expected result. Command=" << cmd << " Output=\n" << output;
}
