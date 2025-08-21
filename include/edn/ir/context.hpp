#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace edn {

struct EmitEnv {
    bool enableDebugInfo = false;
    bool enableEHItanium = false;
    bool enableEHSEH = false;
    // Personality selection may be requested even when EH emission is disabled.
    bool personalityItanium = false;
    bool personalitySEH = false;
    bool panicUnwind = false;
    bool enableCoro = false;
    std::string targetTriple; // empty = default
};

class Context {
public:
    Context();
    ~Context();

    llvm::LLVMContext& llctx() { return *llctx_; }
    llvm::Module& module() { return *module_; }
    const llvm::Module& module() const { return *module_; }

    void newModule(const std::string& name);
    void setTargetTriple(const std::string& triple);

    const EmitEnv& env() const { return env_; }
    void setEnv(EmitEnv e) { env_ = std::move(e); }

private:
    std::unique_ptr<llvm::LLVMContext> llctx_;
    std::unique_ptr<llvm::Module> module_;
    EmitEnv env_{};
};

// Detect emission environment from process env vars (see README for semantics).
// Populates flags for debug info, EH model gating, panic mode, coroutines, and an optional target triple.
EmitEnv detectEnv();

// Apply environment configuration to a module (e.g., target triple). Safe to call with defaults.
void applyEnvToModule(llvm::Module& M, const EmitEnv& env);

} // namespace edn
