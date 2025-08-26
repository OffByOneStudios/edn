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

TEST(Phase5ComplexLowering, TerminatesAndReturnsSix){
    const char* cmd = "./build/languages/rustlite/rustlite_jit_driver languages/rustlite/samples/complex_lowering.rl.rs";
    auto output = run_cmd(cmd);
    // Expect Result: 6 somewhere in output (jit driver prints just 'Result: X' when not in --debug)
    ASSERT_NE(output.find("Result: 6"), std::string::npos) << "complex_lowering did not terminate with expected result. Output=\n" << output;
}
