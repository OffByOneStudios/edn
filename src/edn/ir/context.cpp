#include "edn/ir/context.hpp"
#include <cstdlib>

namespace edn {

Context::Context() : llctx_(std::make_unique<llvm::LLVMContext>()) {}
Context::~Context() = default;

void Context::newModule(const std::string& name) {
    module_ = std::make_unique<llvm::Module>(name, *llctx_);
}

void Context::setTargetTriple(const std::string& triple){
    if(!module_) return;
    module_->setTargetTriple(triple);
}

} // namespace edn

namespace edn {

void applyEnvToModule(llvm::Module& M, const EmitEnv& env){
    if(!env.targetTriple.empty()){
        M.setTargetTriple(env.targetTriple);
    }
}

} // namespace edn
