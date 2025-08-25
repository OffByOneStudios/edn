#include <cassert>
#include <iostream>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/IR/Module.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>

// (Sentinel removed) Previously had a #error to verify the build used this updated source.

using namespace edn;

// This test exercises nested lexical scopes and variable shadowing.
// We build a function with nested blocks introducing locals with the same name
// and ensure distinct DILocalVariable instances exist and that at least one
// DILexicalBlock chain of depth >= 2 is present.
void run_phase4_debug_info_scopes_test(){
    std::cout << "[phase4] debug info scopes test...\n";
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_DEBUG", "1");
#else
    setenv("EDN_ENABLE_DEBUG", "1", 1);
#endif

    const char* SRC = R"EDN((module
      (fn :name "scopes" :ret i32 :params [ (param i32 %a) ] :body [
         (alloca %x i32)
         (store i32 %x %a)
         (block ; inner 1
           [ (alloca %x i32) ; shadows outer %x
             (store i32 %x %a)
             (block ; inner 2
               [ (alloca %x i32) ; shadows again
                 (store i32 %x %a)
               ])
           ])
         (ret i32 %a)
      ])
    ))EDN";

    auto ast = parse(SRC);
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
    assert(tcres.success && mod);

    auto *F = mod->getFunction("scopes");
    assert(F && F->getSubprogram());

    // Collect dbg.declare intrinsics and associated variables named 'x'
    std::vector<llvm::DILocalVariable*> xVars;
    unsigned lexicalBlockDepthMax = 0;

    for(auto &BB : *F){
        for(auto &I : BB){
            if(auto *dvi = llvm::dyn_cast<llvm::DbgVariableIntrinsic>(&I)){
                if(auto *lv = dvi->getVariable()){
                    if(lv->getName() == "x"){
                        xVars.push_back(lv);
                        unsigned depth = 0; llvm::Metadata *rawScope = lv->getScope();
                        while(rawScope){
                            ++depth;
                            if(auto *lb = llvm::dyn_cast<llvm::DILexicalBlock>(rawScope)) rawScope = lb->getScope();
                            else if(auto *sp = llvm::dyn_cast<llvm::DISubprogram>(rawScope)) rawScope = sp->getScope();
                            else if(auto *lex = llvm::dyn_cast<llvm::DILexicalBlockFile>(rawScope)) rawScope = lex->getScope();
                            else if(llvm::isa<llvm::DIFile>(rawScope)) break;
                            else if(auto *ns = llvm::dyn_cast<llvm::DINamespace>(rawScope)) rawScope = ns->getScope();
                            else break;
                        }
                        if(depth > lexicalBlockDepthMax) lexicalBlockDepthMax = depth;
                    }
                }
            }
        }
    }

    // Expect three distinct shadowed 'x' locals (outer + two nested blocks).
    assert(xVars.size() == 3 && "expected three shadowed locals named x");
    assert(xVars[0] != xVars[1] && xVars[1] != xVars[2] && xVars[0] != xVars[2] && "shadowed locals should be distinct DILocalVariable nodes");

    // Expect at least depth >= 2 (function + two nested lexical blocks yields >=3 counting function scope; our simple depth metric >=3).
    assert(lexicalBlockDepthMax >= 3 && "expected nested lexical block depth >= 3");

    std::cout << "[phase4] debug info scopes test passed\n";
}
