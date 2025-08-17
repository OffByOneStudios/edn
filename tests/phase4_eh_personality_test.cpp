#include <cassert>
#include <iostream>
#include <cstdlib>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

void run_phase4_eh_personality_test(){
    std::cout << "[phase4] EH personality wiring test...\n";
    const char* SRC = R"((module
      (fn :name "foo" :ret i32 :params [] :body [ (const %z i32 0) (ret i32 %z) ])
    ))";
    auto ast = parse(SRC);

    // Test Itanium personality
    _putenv("EDN_EH_MODEL=itanium");
    {
        TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
        assert(tcres.success && mod);
        bool ok=false;
        for(auto &F : *mod){
            if(F.getName()=="foo"){ auto *p=F.getPersonalityFn(); ok = (p && p->getName()=="__gxx_personality_v0"); }
        }
        assert(ok);
    }

    // Test SEH personality name selection (only wiring; we don't emit funclets yet)
    _putenv("EDN_EH_MODEL=seh");
    {
        TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
        assert(tcres.success && mod);
        bool ok=false;
        for(auto &F : *mod){
            if(F.getName()=="foo"){ auto *p=F.getPersonalityFn(); ok = (p && p->getName()=="__C_specific_handler"); }
        }
        assert(ok);
    }

    // Reset env
    _putenv("EDN_EH_MODEL=");
    std::cout << "[phase4] EH personality wiring test passed\n";
}
