#include <cassert>
#include <iostream>
#include <string>
#include <cstdlib>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

// Ensure that without EDN_ENABLE_EH=1 we don't emit invoke/funclets, even if a personality is selected.
void run_phase4_eh_disabled_no_invoke_test(){
    std::cout << "[phase4] EH disabled: no invoke/funclets test...\n";
    const char* SRC = R"((module
      (fn :name "callee" :ret i32 :params [] :body [ (const %z i32 1) (ret i32 %z) ])
      (fn :name "caller" :ret i32 :params [] :body [ (call %r i32 callee) (ret i32 %r) ])
    ))";
    auto ast = parse(SRC);

    // Model set, but not enabling EH emission
    _putenv("EDN_EH_MODEL=seh");
    _putenv("EDN_ENABLE_EH=");

    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
    assert(tcres.success && mod);

    // Personality should be present, but IR should use 'call' and contain no invoke/cleanuppad/cleanupret
    bool hasPersonality=false;
    for(auto &F : *mod){
        if(F.getName()=="caller"){ auto *p=F.getPersonalityFn(); hasPersonality = (p && p->getName()=="__C_specific_handler"); }
    }
    assert(hasPersonality);

    std::string ir; { std::string s; llvm::raw_string_ostream rso(s); mod->print(rso, nullptr); rso.flush(); ir = std::move(s); }
    assert(ir.find("invoke ") == std::string::npos);
    assert(ir.find("cleanuppad ") == std::string::npos);
    assert(ir.find("cleanupret ") == std::string::npos);

    _putenv("EDN_EH_MODEL=");
    std::cout << "[phase4] EH disabled: no invoke/funclets test passed\n";
}
