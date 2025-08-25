#include <cassert>
#include <iostream>
#include <map>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>

// (Sentinel removed) Previously had a #error to verify the build used this updated source.

using namespace edn;

// Validates that DI member offsets for a struct match the LLVM DataLayout computed
// element offsets for the lowered LLVM struct type.
void run_phase4_debug_info_struct_member_offsets_test(){
    std::cout << "[phase4] debug info struct member offsets test...\n";
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_DEBUG", "1");
#else
    setenv("EDN_ENABLE_DEBUG", "1", 1);
#endif

    const char* SRC = R"EDN((module
      (struct :name "Point" :fields [ (:name x :type i32) (:name y :type i64) (:name z :type i32) ])
      (fn :name "make" :ret i32 :params [ (param i32 %a) ] :body [
         (struct-lit %p Point [ x %a y %a z %a ])
         (ret i32 %a)
      ])
    ))EDN";

    auto ast = parse(SRC);
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
    assert(tcres.success && mod);

    // Get struct type and layout
    auto *ST = llvm::StructType::getTypeByName(*emitter.llctx_, "struct.Point");
    assert(ST && "expected LLVM struct type for Point");
    const llvm::DataLayout &DL = mod->getDataLayout();

    // Gather expected element offsets in bits
    std::vector<uint64_t> expectedBits;
    for(unsigned i=0;i<ST->getNumElements();++i){
        uint64_t offBytes = DL.getStructLayout(ST)->getElementOffset(i);
        expectedBits.push_back(offBytes * 8);
    }

    // Find DICompositeType for Point via dbg.declare of local %p
    auto *F = mod->getFunction("make");
    assert(F && F->getSubprogram());
    llvm::DICompositeType* pointDI = nullptr;
    for(auto &BB : *F){
        for(auto &I : BB){
            if(auto *dvi = llvm::dyn_cast<llvm::DbgVariableIntrinsic>(&I)){
                if(auto *var = dvi->getVariable()){ // DILocalVariable*
                    llvm::DIType *ty = var->getType();
                    while(ty && !llvm::isa<llvm::DICompositeType>(ty)){
                        if(auto *dt = llvm::dyn_cast<llvm::DIDerivedType>(ty)) ty = dt->getBaseType(); else break;
                    }
                    if(auto *ct = llvm::dyn_cast_or_null<llvm::DICompositeType>(ty)){
                        if(ct->getName() == "Point"){ pointDI = ct; break; }
                    }
                }
            }
        }
        if(pointDI) break;
    }
    assert(pointDI && "expected DICompositeType for Point");
    auto members = pointDI->getElements();
    assert(members.size() == expectedBits.size() && "member count mismatch");

    for(unsigned i=0;i<static_cast<unsigned>(members.size());++i){
        llvm::Metadata *md = members[i];
        auto *mem = llvm::dyn_cast<llvm::DIDerivedType>(md);
        assert(mem && "expected DIDerivedType member");
        uint64_t diOff = mem->getOffsetInBits();
        uint64_t exp = expectedBits[i];
        if(diOff != exp){
            std::cerr << "[offset] mismatch field " << i << ": diOff=" << diOff << " expected=" << exp << "\n";
        }
        assert(diOff == exp && "DI member offset mismatch with DataLayout");
    }

    std::cout << "[phase4] debug info struct member offsets test passed\n";
}
