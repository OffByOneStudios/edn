#include <cassert>
#include <iostream>
#include <cstdlib>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.hpp"
#include "edn/diagnostics_json.hpp"

using namespace edn;

// Helper: run checker and capture JSON via direct call (avoid needing to set env at process level for test portability)
static std::string to_json(const char* src){
    auto ast = parse(src);
    TypeContext ctx; TypeChecker tc(ctx);
    auto res = tc.check_module(ast);
    return diagnostics_to_json(res);
}

static void test_json_success(){
    auto js = to_json("(module (fn :name \"ok\" :ret i32 :params [] :body [ (const %a i32 1) (ret i32 %a) ]))");
    // Should contain success true and empty errors array
    assert(js.find("\"success\":true")!=std::string::npos);
    assert(js.find("\"errors\":[")!=std::string::npos);
}

static void test_json_error_with_notes(){
    // Trigger a global const mismatch producing expected/found notes (E1220 path)
    auto js = to_json("(module (global :name g :type i32 :const true :init 1.5))");
    // Expect success false, error code E1220 present, and notes array with at least two entries
    assert(js.find("\"success\":false")!=std::string::npos);
    assert(js.find("E1220")!=std::string::npos);
    // crude check: count occurrences of \"line\": in the notes section after E1220
    auto pos = js.find("E1220");
    assert(pos!=std::string::npos);
    auto notesPos = js.find("\"notes\":[", pos);
    assert(notesPos!=std::string::npos);
    auto closing = js.find(']', notesPos);
    assert(closing!=std::string::npos);
    auto segment = js.substr(notesPos, closing - notesPos);
    // require at least one comma inside notes to imply >=2 entries
    assert(segment.find(",")!=std::string::npos);
}

int run_phase3_diagnostics_json_tests(){
    std::cout << "[phase3] diagnostics JSON tests...\n";
    test_json_success();
    test_json_error_with_notes();
    std::cout << "[phase3] diagnostics JSON tests passed\n";
    return 0;
}
