#include "edn/ir/function_builder.hpp"
#include <llvm/IR/Type.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Attributes.h>

namespace edn::ir::function_builder {

CreatedFunction create_function(edn::TypeContext& tctx,
                                std::function<llvm::Type*(edn::TypeId)> map_type,
                                llvm::Module& module,
                                llvm::LLVMContext& llctx,
                                edn::ir::debug::DebugManager* dbg,
                                const FunctionBuildInputs& in,
                                llvm::Function* personality,
                                bool enableCoro,
                                bool enableEHItanium,
                                bool enableEHSEH){
    CreatedFunction out;
    std::vector<edn::TypeId> paramIds; paramIds.reserve(in.params.size());
    for(auto &p: in.params) paramIds.push_back(p.second);
    auto ftyId = tctx.get_function(paramIds, in.retType, in.isVariadic);
    llvm::Type* rawTy = map_type(ftyId);
    if(!llvm::isa<llvm::FunctionType>(rawTy)){
        fprintf(stderr, "[guard][fnbuild] expected FunctionType for %s but got kind=%u; abort create.\n", in.name.c_str(), (unsigned)rawTy->getTypeID());
        return out;
    }
    auto *fty = llvm::cast<llvm::FunctionType>(rawTy);
    auto linkage = llvm::Function::ExternalLinkage;
    llvm::Function* F = llvm::Function::Create(fty, linkage, in.name, &module);
    out.function = F;
    if(in.isExternal){
        return out; // declaration only
    }
    // Debug info attach
    if(dbg && dbg->enableDebugInfo){
        unsigned fnLine = 1; // caller may patch real line if needed
        auto *SP = edn::ir::di::attach_function_debug(*dbg, *F, in.name, in.retType, in.params, fnLine);
        (void)SP;
    }
    if(enableCoro){
        F->addFnAttr(llvm::Attribute::AttrKind::PresplitCoroutine);
        F->addFnAttr("presplitcoroutine");
    }
    if(personality) F->setPersonalityFn(personality);
    if(personality && (enableEHItanium || enableEHSEH)) F->addFnAttr("uwtable");
    size_t ai=0; for(auto &arg : F->args()) if(ai<in.params.size()) arg.setName(in.params[ai++].first);
    auto *entry = llvm::BasicBlock::Create(llctx, "entry", F);
    auto *irb = new llvm::IRBuilder<>(entry);
    auto *state = new edn::ir::builder::State{*irb, llctx, module, tctx, map_type,
        *(new std::unordered_map<std::string, llvm::Value*>()), *(new std::unordered_map<std::string, edn::TypeId>()),
        *(new std::unordered_map<std::string, llvm::AllocaInst*>()), *(new std::unordered_map<std::string, std::string>()),
        *(new std::unordered_map<std::string, edn::node_ptr>()), std::shared_ptr<edn::ir::debug::DebugManager>(dbg?dbg:nullptr), 0, {}};
    // Seed param types & values
    for(auto &pr: in.params) state->vtypes[pr.first] = pr.second;
    for(auto &arg : F->args()) state->vmap[std::string(arg.getName())] = &arg;
    if(dbg && dbg->enableDebugInfo){ edn::ir::di::setup_function_entry_debug(*dbg, *F, *irb, state->vtypes); }
    out.state = state;
    return out;
}

} // namespace edn::ir::function_builder
