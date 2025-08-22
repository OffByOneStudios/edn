#include "edn/ir/debug_pipeline.hpp"
#include <cstdlib>
#include <string>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>

namespace edn::ir::debug_pipeline {

void run_pass_pipeline(llvm::Module& M){
    if(const char* enable = std::getenv("EDN_ENABLE_PASSES"); !(enable && std::string(enable)=="1")) return;
    llvm::PassBuilder PB;
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    if(const char* pipeline = std::getenv("EDN_PASS_PIPELINE"); pipeline && *pipeline){
        llvm::ModulePassManager MPM;
        if(auto Err = PB.parsePassPipeline(MPM, pipeline)){
            llvm::consumeError(std::move(Err));
        } else {
            if(const char* v = std::getenv("EDN_VERIFY_IR"); v && std::string(v)=="1"){
                if(llvm::verifyModule(M, &llvm::errs())) llvm::errs() << "[edn] IR verify failed before custom pipeline\n"; }
            MPM.run(M, MAM);
            if(const char* v2 = std::getenv("EDN_VERIFY_IR"); v2 && std::string(v2)=="1"){
                if(llvm::verifyModule(M, &llvm::errs())) llvm::errs() << "[edn] IR verify failed after custom pipeline\n"; }
            return;
        }
    }
    llvm::OptimizationLevel optLevel = llvm::OptimizationLevel::O1;
    if(const char* lvl = std::getenv("EDN_OPT_LEVEL"); lvl && *lvl){
        std::string s = lvl; for(char &c: s) c = (char)tolower((unsigned char)c);
        if(s=="0"||s=="o0") optLevel = llvm::OptimizationLevel::O0;
        else if(s=="2"||s=="o2") optLevel = llvm::OptimizationLevel::O2;
        else if(s=="3"||s=="o3") optLevel = llvm::OptimizationLevel::O3;
        else optLevel = llvm::OptimizationLevel::O1;
    }
    if(optLevel == llvm::OptimizationLevel::O0) return; // leave unoptimized
    llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(optLevel);
    if(const char* v = std::getenv("EDN_VERIFY_IR"); v && std::string(v)=="1"){
        if(llvm::verifyModule(M, &llvm::errs())) llvm::errs() << "[edn] IR verify failed before preset pipeline\n"; }
    MPM.run(M, MAM);
    if(const char* v2 = std::getenv("EDN_VERIFY_IR"); v2 && std::string(v2)=="1"){
        if(llvm::verifyModule(M, &llvm::errs())) llvm::errs() << "[edn] IR verify failed after preset pipeline\n"; }
}

} // namespace edn::ir::debug_pipeline
