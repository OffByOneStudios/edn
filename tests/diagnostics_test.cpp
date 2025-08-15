#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.inl"

using namespace edn;

static void test_extended_fcmp_predicates(){
    const char* preds[] = {"ord","uno","ueq","une","ult","ugt","ule","uge"};
    for(auto p: preds){
        std::ostringstream oss;
        oss << "(module (fn :name \"f\" :ret i1 :params [ (param f32 %a) (param f32 %b) ] :body [ (fcmp %r f32 :pred " << p << " %a %b) (ret i1 %r) ]))";
        auto ast = parse(oss.str());
        TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
        assert(res.success && "expected fcmp predicate to be accepted");
    }
}

void run_diagnostics_tests(){
    test_extended_fcmp_predicates();
    // Suggestion tests: unknown global with near match
    {
        std::string src = "(module (global :name G1 :type i32 :const true :init 1) (fn :name \"m\" :ret i32 :params [ ] :body [ (gload %r i32 Gl ) (ret i32 %r) ]))"; // misspelled 'G1' as 'Gl'
        auto ast = parse(src);
        TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
        assert(!res.success);
        bool found=false, note=false; for(auto &e: res.errors){ if(e.code=="E0902"){ found=true; for(auto &n: e.notes){ if(n.message.find("did you mean")!=std::string::npos) note=true; } } }
        assert(found && note && "expected suggestion note for unknown global");
    }
    // Gating test: disable with EDN_SUGGEST=0
    {
        _putenv("EDN_SUGGEST=0");
        std::string src = "(module (global :name GG :type i32 :const true :init 1) (fn :name \"m2\" :ret i32 :params [ ] :body [ (gload %r i32 Gg ) (ret i32 %r) ]))"; // close to GG
        auto ast = parse(src);
        TypeContext ctx; TypeChecker tc(ctx); auto res = tc.check_module(ast);
        bool note=false; for(auto &e: res.errors){ if(e.code=="E0902") for(auto &n: e.notes) if(n.message.find("did you mean")!=std::string::npos) note=true; }
        assert(!note && "suggestions should be gated off when EDN_SUGGEST=0");
        _putenv("EDN_SUGGEST="); // unset
    }
    std::cout << "Diagnostics tests passed\n";
}
