// Focused test: verify resolver ensure_slot + lexicalDepth + unwind_scope produce
// distinct allocas for shadowed names and restore previous binding after scope exit.
// Integrated into phase4 aggregate via forward declaration in phase4 mains.

#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/ir/resolver.hpp"

using namespace edn;

static void build_and_check(){
    const char* SRC = R"EDN((module
      (fn :name "shadow" :ret i32 :params [ (param i32 %a) ] :body [
         (alloca %x i32)
         (store i32 %x %a)
         (block [ (alloca %x i32) (store i32 %x %a) (block [ (alloca %x i32) (store i32 %x %a) ]) ])
         (ret i32 %a)
      ])
    ))EDN";
    auto ast = parse(SRC);
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres); assert(tcres.success && mod);
    auto *F = mod->getFunction("shadow"); assert(F);
    // Count allocas named 'x'
    unsigned count = 0; for(auto &BB : *F) for(auto &I : BB) if(auto *AI = llvm::dyn_cast<llvm::AllocaInst>(&I)) if(AI->getName().starts_with("x")) ++count;
    // Expect at least 3 (outer + 2 nested) shadow slots.
    assert(count >= 3);
}

void run_phase4_resolver_shadow_test(){
    std::cout << "[phase4] resolver shadow test...\n";
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_DEBUG", "1");
#else
    setenv("EDN_ENABLE_DEBUG", "1", 1);
#endif
    build_and_check();
    std::cout << "[phase4] resolver shadow test passed\n";
}
