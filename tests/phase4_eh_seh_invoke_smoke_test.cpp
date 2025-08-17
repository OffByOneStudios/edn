#include <cassert>
#include <iostream>
#include <cstdlib>
#include <string>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

void run_phase4_eh_seh_invoke_smoke_test(){
    std::cout << "[phase4] EH SEH invoke/cleanuppad smoke test...\n";
    const char* SRC = R"((module
      (fn :name "callee" :ret i32 :params [] :body [ (const %z i32 1) (ret i32 %z) ])
      (fn :name "caller" :ret i32 :params [] :body [ (call %r i32 callee) (ret i32 %r) ])
    ))";
    auto ast = parse(SRC);
    _putenv("EDN_EH_MODEL=seh");
    _putenv("EDN_ENABLE_EH=1");
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
    assert(tcres.success && mod);
    // Verify personality on caller
    bool hasPersonality=false;
    for(auto &F : *mod){
        if(F.getName()=="caller"){ auto *p=F.getPersonalityFn(); hasPersonality = (p && p->getName()=="__C_specific_handler"); }
    }
    assert(hasPersonality);
    std::string ir;
    llvm::raw_string_ostream os(ir);
    mod->print(os, nullptr);
    os.flush();
    // Look for SEH funclet constructs
    assert(ir.find("invoke ") != std::string::npos);
    assert(ir.find("cleanuppad ") != std::string::npos);
    assert(ir.find("cleanupret ") != std::string::npos);
    _putenv("EDN_ENABLE_EH=");
    _putenv("EDN_EH_MODEL=");
    std::cout << "[phase4] EH SEH invoke/cleanuppad smoke test passed\n";
}
