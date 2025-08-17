#include <cassert>
#include <iostream>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/IR/Module.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>

using namespace edn;

void run_phase4_debug_info_struct_members_test(){
    std::cout << "[phase4] debug info struct members test...\n";
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_DEBUG", "1");
#else
    setenv("EDN_ENABLE_DEBUG", "1", 1);
#endif

        const char* SRC = R"EDN((module
            (struct :name "Point" :fields [ (:name x :type i32) (:name y :type i32) ])
            (fn :name "make" :ret i32 :params [ (param i32 %a) ] :body [
                 (struct-lit %p Point [ x %a y %a ])
                 (ret i32 %a)
            ])
        ))EDN";

    auto ast = parse(SRC);
    std::cout << "[dbg] parsed AST for struct members test\n";
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres;
    auto *mod = emitter.emit(ast, tcres);
    std::cout << "[dbg] emit returned, success=" << tcres.success << ", mod=" << (void*)mod << "\n";
    assert(tcres.success && mod);

    // Find the dbg.declare for local %p and inspect its variable type
    auto *F = mod->getFunction("make");
    std::cout << "[dbg] module ptr=" << (void*)mod << ", F=" << (void*)F << "\n";
    if(F){ std::cout << "[dbg] has Subprogram? " << (F->getSubprogram() != nullptr) << "\n"; }
    assert(F && F->getSubprogram());
    llvm::DICompositeType* found = nullptr;
    llvm::DINodeArray members;
    int dbgDecls = 0;
    for(auto &BB : *F){
        for(auto &I : BB){
            if(auto *dbg = llvm::dyn_cast<llvm::DbgDeclareInst>(&I)){
                ++dbgDecls;
                if(auto *lv = dbg->getVariable()){
                    if(auto *ct = llvm::dyn_cast<llvm::DICompositeType>(lv->getType())){
                        if(ct->getName() == "Point"){ found = ct; members = ct->getElements(); break; }
                    }
                }
            }
        }
        if(found) break;
    }
    std::cout << "[dbg] dbg.declare count=" << dbgDecls << "\n";

    assert(found && "expected DICompositeType for Point");
    std::cout << "[dbg] DICompositeType 'Point' found; member count=" << members.size() << "\n";
    assert(members.size() >= 2 && "expected at least 2 struct members in DI");
    assert(members.size() >= 2 && "expected at least 2 struct members in DI");

    // Lines should be non-zero as we now wire field declaration lines
    for(auto *md : members){
        if(auto *m = llvm::dyn_cast<llvm::DIDerivedType>(md)){
            assert(m->getLine() > 0 && "member should carry a source line");
        }
    }

    std::cout << "[phase4] debug info struct members test passed\n";
}
