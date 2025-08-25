// Match binding offsets test: verify payload field raw GEP offsets follow packed layout expectation.
#include <cassert>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/DebugInfoMetadata.h>

void run_phase4_match_binding_offsets_test(){
  std::cout << "[phase4] match binding offsets test ...\n";
    const char* SRC = R"EDN((module
      (sum :name T :variants [ (variant :name A :fields [ i32 i64 ]) (variant :name B :fields [ i32 ]) ])
      (fn :name "m" :ret void :params [ (param (ptr (struct-ref T)) %p) ] :body [
        (match T %p :cases [
          (case A :binds [ (bind %x 0) (bind %y 1) ] :body [ (zext %x64 i64 %x) (add %z i64 %x64 %y) ])
          (case B [ (const %one i32 1) ])
        ] :default [ ])
      ])
    ))EDN";
  auto ast = edn::parse(SRC);
  edn::TypeContext tctx; edn::IREmitter emitter(tctx); edn::TypeCheckResult tcr; auto *M = emitter.emit(ast, tcr);
  assert(tcr.success && M);

  auto *F = M->getFunction("m");
  assert(F && "expected function m");

  std::optional<uint64_t> off0; // first field (i32) expected 0
  std::optional<uint64_t> off1; // second field (i64) expected 4 (packed after i32)

  for(auto &BB : *F){
    for(auto &I : BB){
      if(auto *GEP = llvm::dyn_cast<llvm::GetElementPtrInst>(&I)){
        std::string nm = GEP->getName().str();
                if(nm == "x.raw"){
          if(GEP->getNumOperands() >= 2){
            if(auto *CI = llvm::dyn_cast<llvm::ConstantInt>(GEP->getOperand(1))) off0 = CI->getZExtValue();
          }
                } else if(nm == "y.raw"){
          if(GEP->getNumOperands() >= 2){
            if(auto *CI = llvm::dyn_cast<llvm::ConstantInt>(GEP->getOperand(1))) off1 = CI->getZExtValue();
          }
        }
      }
    }
  }

  assert(off0 && off1 && "expected to locate raw GEPs for variant A payload fields f0,f1");
  if(*off0 != 0){ std::cerr << "[phase4][match_binding_offsets] f0 offset=" << *off0 << " expected 0\n"; }
  if(*off1 != 4){ std::cerr << "[phase4][match_binding_offsets] f1 offset=" << *off1 << " expected 4\n"; }
  assert(*off0 == 0);
  assert(*off1 == 4);

  // --- DI offset validation (if debug info was enabled) ---
  // Traverse dbg.value intrinsics to locate DILocalVariable for %x and %y and
  // infer their underlying variant field offsets. The current emission uses dbg.value
  // directly on the loaded SSA value, so we approximate by confirming the GEP raw offsets
  // (already asserted) and ensuring variables exist. If future DI attaches composite
  // types with explicit member offsets, extend this to compare DIDerivedType::getOffsetInBits.
  unsigned foundVars = 0;
  for(auto &BB : *F){
    for(auto &I : BB){
      if(auto *dvi = llvm::dyn_cast<llvm::DbgVariableIntrinsic>(&I)){
        if(auto *var = dvi->getVariable()){
          auto name = var->getName();
          if(name == "x" || name == "y") ++foundVars;
        }
      }
    }
  }
  // We expect at least the two bound variables to have DI entries if debug was enabled; do not hard fail if disabled.
  if(foundVars < 2){
    std::cout << "[phase4][match_binding_offsets] (info) debug info disabled or variables not materialized (found=" << foundVars << ")\n";
  }

  // --- DI member offset cross-check (struct 'T') ---
  if(auto *cuMD = M->getNamedMetadata("llvm.dbg.cu")){
    bool checkedT = false;
    for(unsigned i=0;i<cuMD->getNumOperands() && !checkedT; ++i){
      if(auto *cu = llvm::dyn_cast<llvm::DICompileUnit>(cuMD->getOperand(i))){
        auto retained = cu->getRetainedTypes();
        for(auto *ty : retained){
          if(auto *ct = llvm::dyn_cast<llvm::DICompositeType>(ty)){
            if(ct->getName()=="T"){
              // Expect at least two elements (variant A members)
              auto elems = ct->getElements();
              std::vector<uint64_t> memberOffsets;
              for(auto *dn : elems){
                if(auto *md = llvm::dyn_cast<llvm::DIDerivedType>(dn)){
                  memberOffsets.push_back(md->getOffsetInBits()/8); // bytes
                }
              }
              if(memberOffsets.size() >= 2){
                uint64_t di_off0 = memberOffsets[0];
                uint64_t di_off1 = memberOffsets[1];
                if(di_off0 != *off0){
                  std::cerr << "[phase4][match_binding_offsets][DI] member0 offset=" << di_off0 << " expected=" << *off0 << "\n";
                }
                if(di_off1 != *off1){
                  std::cerr << "[phase4][match_binding_offsets][DI] member1 offset=" << di_off1 << " expected=" << *off1 << "\n";
                }
                assert(di_off0 == *off0 && "DI offset mismatch for T.f0");
                assert(di_off1 == *off1 && "DI offset mismatch for T.f1");
              } else {
                std::cout << "[phase4][match_binding_offsets] (info) DI composite 'T' has insufficient members to validate offsets (" << memberOffsets.size() << ")\n";
              }
              checkedT = true;
              break;
            }
          }
        }
      }
    }
    if(!checkedT){
      std::cout << "[phase4][match_binding_offsets] (info) did not locate DI composite for 'T' (possibly stripped)\n";
    }
  }

  std::cout << "[phase4] match binding offsets test passed\n";
}
