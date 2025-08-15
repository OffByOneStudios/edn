#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.inl"

using namespace edn;

static void expect_error_with_notes(const char* src, const std::string& code, size_t noteCount){
    auto ast = parse(src);
    TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
    assert(!res.success);
    bool found=false; for(auto &e: res.errors){ if(e.code==code){ found=true; if(e.notes.size()<noteCount){
                std::cerr << "Expected at least "<<noteCount<<" notes for code "<<code<<" got "<<e.notes.size()<<"\n"; assert(false); }
            break; } }
    if(!found){ std::cerr << "Did not find expected error code "<<code<<"\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; assert(false); }
}

static const char* mod_global_scalar_mismatch = R"((module
    (global :name G :type i32 :const true :init 3.14)
))";

static const char* mod_phi_mismatch = R"((module
    (fn :name "f" :ret i32 :params [ (param i32 %a) (param i32 %b) ]
        :body [ (phi %x i64 [ (%a %L0) (%b %L1) ]) (ret i32 %a) ])
))"; // phi incoming mismatch: %a/%b i32 vs phi i64

void run_phase3_diagnostics_notes_tests(){
    std::cout << "[phase3] diagnostics mismatch notes...\n";
    expect_error_with_notes(mod_global_scalar_mismatch, "E1220", 2);
    expect_error_with_notes(mod_phi_mismatch, "E0309", 2);
    std::cout << "Phase 3 diagnostics mismatch notes tests passed\n";
}
