#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/TargetSelect.h>

using namespace edn;

void run_phase4_closures_jit_smoke_test(){
    // Build a module that constructs a closure and calls it; expect 10 + 5 = 15
    const char* SRC = R"((module
      (fn :name "adder" :ret i32 :params [ (param i32 %env) (param i32 %x) ] :body [
         (add %s i32 %env %x)
         (ret i32 %s)
      ])
      (fn :name "main" :ret i32 :params [ ] :body [
         (const %ten i32 10)
         (const %five i32 5)
         (make-closure %c adder [ %ten ])
         (call-closure %res i32 %c %five)
         (ret i32 %res)
      ])
    ))";
    auto ast = parse(SRC);
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
    assert(tcres.success && mod);
    llvm::InitializeNativeTarget(); llvm::InitializeNativeTargetAsmPrinter();
    auto jitExp = llvm::orc::LLJITBuilder().create(); assert(jitExp && "Failed to create JIT");
    auto jit = std::move(*jitExp);
    // Resolve host symbols if needed
    jit->getMainJITDylib().addGenerator(llvm::cantFail(
        llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            jit->getDataLayout().getGlobalPrefix())));
    auto err = jit->addIRModule(emitter.toThreadSafeModule()); assert(!err && "Failed to add module to JIT");
    auto sym = jit->lookup("main"); assert(sym && "main not found");
    using FnTy = int(*)(void); auto fn = reinterpret_cast<FnTy>(sym->toPtr<void*>());
    int result = fn();
    assert(result == 15);
    std::cout << "[phase4] closures JIT smoke passed\n";
}
