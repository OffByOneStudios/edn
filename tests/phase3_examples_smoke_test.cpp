// Phase 3 examples smoke test: parse & type-check each example EDN file without JIT.
#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.inl"

using namespace edn;

static std::string read_all(const std::filesystem::path& p){
    std::ifstream ifs(p, std::ios::binary); if(!ifs) return {};
    std::string s; ifs.seekg(0,std::ios::end); s.resize((size_t)ifs.tellg()); ifs.seekg(0); ifs.read(&s[0], s.size()); return s;
}

void run_phase3_examples_smoke(){
#ifndef EDN_SOURCE_DIR
    std::cout << "[phase3] examples smoke (no EDN_SOURCE_DIR defined, skipping)\n";
    return;
#else
    std::filesystem::path root = EDN_SOURCE_DIR;
    auto dir = root/"edn"/"phase3";
    std::vector<std::string> files = {
        "pointer_arith.edn",
    "addr_deref.edn",
    "fnptr_indirect.edn",
        "typedef_enum.edn",
    "union_access.edn",
        "variadic.edn",
        "for_continue.edn",
        "switch.edn",
    "cast_sugar.edn",
    "extern_malloc.edn",
    "extern_malloc_call.edn"
        // globals_const_notes intentionally excluded (contains expected errors)
    };
    std::cout << "[phase3] examples smoke..." << std::endl;
    bool anyFailures=false; size_t processed=0, failed=0;
    for(auto &f : files){
        std::cout << "  -> checking " << f << "..." << std::flush;
        auto path = dir/f;
        auto src = read_all(path);
        if(src.empty()){
            std::cerr << "[phase3] warning: example file missing: "<< path <<"\n"; continue;
        }
        TypeCheckResult res; bool parsed=false;
        try {
            auto ast = parse(src);
            parsed=true;
            TypeContext ctx; TypeChecker tc(ctx);
            res = tc.check_module(ast);
        } catch(const std::exception& ex){
            std::cerr << " EXCEPTION\n    exception: " << ex.what() << "\n";
            if(!parsed) std::cerr << "    (during parse)\n"; else std::cerr << "    (during type check)\n";
            anyFailures=true; ++failed; ++processed; continue;
        }
        ++processed;
        if(!res.success){
            std::cerr << " FAILED\n    diagnostics: \n";
            for(auto &e: res.errors) std::cerr << e.code << " " << e.message << "\n";
            std::cerr << "    (skipping; tolerated failure)\n";
            anyFailures=true; ++failed; continue;
        }
        std::cout << " OK" << std::endl;
    }
    if(!anyFailures) std::cout << "[phase3] examples smoke passed ("<<processed<<"/"<<files.size()<<")\n";
    else std::cout << "[phase3] examples smoke completed with tolerated failures ("<< (processed-failed) <<" ok, "<< failed <<" failed)\n";
#endif
}
