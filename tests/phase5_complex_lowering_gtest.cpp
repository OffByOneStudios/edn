#include <gtest/gtest.h>
#include <cstdlib>
#include <string>
#include <array>
#include <memory>
#include <cstdio>
#include <vector>
#include <sstream>

// Helper to run the rustlite JIT driver on the complex_lowering sample and capture stdout.
static std::string locate_driver(){
    // Common relative locations when test binary lives in build/tests/:
    const char* candidates[] = {
        "../languages/rustlite/rustlite_jit_driver", // build/tests -> build/languages/...
        "../../languages/rustlite/rustlite_jit_driver", // in case of deeper nesting
    "languages/rustlite/rustlite_jit_driver", // fallback if launched from repo root
    "build/languages/rustlite/rustlite_jit_driver" // running from repository root without chdir
    };
    for(const char* c : candidates){
        if(FILE *f = fopen(c, "rb")){ fclose(f); return c; }
    }
    return candidates[0];
}

static int run_complex_lowering(std::string *fullOut=nullptr){
    std::string exe = locate_driver();
    // Try a sequence of sample paths relative to typical invocation CWDs (build/, build/tests/, repo root)
    const char* sample_candidates[] = {
        "../../languages/rustlite/samples/complex_lowering.rl.rs", // when CWD=build/tests
        "../languages/rustlite/samples/complex_lowering.rl.rs",    // when CWD=build
        "languages/rustlite/samples/complex_lowering.rl.rs",       // when CWD=repo root
        "./languages/rustlite/samples/complex_lowering.rl.rs"      // explicit relative
    };
    const char* sample = sample_candidates[0];
    for(auto cand : sample_candidates){ if(FILE *f = fopen(cand, "rb")){ fclose(f); sample = cand; break; } }
    std::string cmd = exe + " " + sample + " --debug";
    std::array<char, 512> buf{};
#if defined(_WIN32)
    FILE *p = _popen(cmd.c_str(), "r");
#else
    FILE *p = popen(cmd.c_str(), "r");
#endif
    if(!p) return -1;
    std::string out;
    while(fgets(buf.data(), (int)buf.size(), p)){ out += buf.data(); }
#if defined(_WIN32)
    _pclose(p);
#else
    pclose(p);
#endif
    if(fullOut) *fullOut = out;
    // Parse last "Result: N" line.
    int result = -999;
    std::istringstream iss(out);
    std::string line;
    while(std::getline(iss, line)){
        auto pos = line.find("Result:");
        if(pos != std::string::npos){
            std::istringstream ls(line.substr(pos+7));
            ls >> result;
        }
    }
    return result;
}

TEST(ComplexLoweringGTest, ReturnsSix){
    std::string out; int r = run_complex_lowering(&out);
    ASSERT_NE(r, -999) << "Did not parse Result line. Output was:\n" << out;
    EXPECT_EQ(r, 6) << out;
}

TEST(ComplexLoweringGTest, NoFixedAddArtifacts){
    std::string out; int r = run_complex_lowering(&out);
    ASSERT_EQ(r, 6) << "Unexpected result: " << r << " output:\n" << out;
    EXPECT_EQ(out.find(".fixed"), std::string::npos) << "Unexpected leftover '.fixed' artifact after EDN-0001 cleanup. Output:\n" << out;
}
