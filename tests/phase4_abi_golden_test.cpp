#include <cassert>
#include <iostream>
#include <string>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/traits.hpp"

using namespace edn;

static std::string module_to_ir(llvm::Module* m){
    std::string buf; llvm::raw_string_ostream os(buf); m->print(os, nullptr); os.flush(); return buf;
}

static void expect_contains(const std::string& ir, const char* needle){
    if(ir.find(needle)==std::string::npos){
        std::cerr << "ABI golden IR missing snippet:\n" << needle << "\n----- IR dump -----\n" << ir << "\n-------------------\n";
    }
    assert(ir.find(needle)!=std::string::npos);
}

void run_phase4_abi_golden_test(){
    std::cout << "[phase4] ABI golden test...\n";
        // Build a module exercising closure, trait object, and sum types
        auto ast = parse(R"EDN(
        (module :id "abi"
          (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) (variant :name B :fields [ ]) ])
          (sum :name U :variants [ (variant :name X :fields [ (ptr i8) ]) (variant :name Y :fields [ i32 (ptr i8) ]) ])
          (trait :name Show :methods [ (method :name print :type (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32))) ])
                    (fn :name "callee" :ret i32 :params [ (param i32 %env) (param i32 %x) ] :body [ (add %s i32 %env %x) (ret i32 %s) ])
                    (fn :name "main" :ret i32 :params [ (param i32 %x) ] :body [
                        (const %cap i32 7)
                        (make-closure %c callee [ %cap ])
                        (sum-new %s T A [ %x %cap ])
                        (const %z (ptr i8) 0)
                        (sum-new %u U Y [ %x %z ])
                        (ret i32 0)
                    ])
        ))EDN");

        // Expand traits to generate ShowVT and related structs prior to type checking
        auto expanded = expand_traits(ast);
        TypeContext tctx; IREmitter em(tctx); TypeCheckResult r; auto *m = em.emit(expanded, r);
    assert(r.success && m);
    auto ir = module_to_ir(m);

    // Sum layout: struct.T = type { i32, [N x i8] }
    expect_contains(ir, "struct.T = type { i32,");
    // Nested/second sum layout present too
    expect_contains(ir, "struct.U = type { i32,");

    // Closure: identified env struct should exist for callee
    expect_contains(ir, "struct.__edn.closure.callee");
    // Callee signature uses env then x as parameters (current closure lowering convention)
    expect_contains(ir, "define i32 @callee(i32 %env, i32 %x)");

    // Trait vtable struct for Show (emitted as %struct.ShowVT = type { ptr } for single method)
    expect_contains(ir, "%struct.ShowVT = type { ptr }");
    // Ensure vtable field order keeps 'print' first
    expect_contains(ir, "%struct.ShowVT = type { ptr }");

    // Closure record layout: { i8*, <env-ty> }
    expect_contains(ir, "%struct.__edn.closure.callee = type { ptr, i32 }");

    std::cout << "[phase4] ABI golden test passed\n";
}
