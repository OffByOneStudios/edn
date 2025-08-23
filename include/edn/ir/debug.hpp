#pragma once

// Minimal includes to avoid circular dependency with ir_emitter.hpp
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "edn/types.hpp"

#include "edn/ir_emitter.hpp"

namespace edn {
    class IREmitter;
}

namespace edn::ir::debug
{

    class DebugManager
    {
    private:
        std::unique_ptr<llvm::Module> &module;
        edn::IREmitter *emitter;
    // Stack of active lexical scopes (DISubprogram or DILexicalBlock etc.)
    std::vector<llvm::DIScope*> scopeStack;

    public:
        bool enableDebugInfo = false; // Set to true if debug info should be generated
        std::unique_ptr<llvm::DIBuilder> DIB;
        llvm::DIFile *DI_File;
    bool skipPayloadArrayDI = false; // diagnostic: optionally skip DI for sum payload array field

    public:
        std::unordered_map<edn::TypeId, llvm::DIType *> DITypeCache;

    public:
        DebugManager(bool enableDebugInfo, std::unique_ptr<llvm::Module> &module, edn::IREmitter *emitter);

        void initialize();

        llvm::DIType *diTypeOf(edn::TypeId id);

    // --- Lexical scope management -------------------------------------------------
    // Return the current (innermost) scope; falls back to DI_File when stack empty.
    llvm::DIScope* currentScope() const;
    // Push a function scope (called automatically when a DISubprogram is attached).
    void pushFunctionScope(llvm::DISubprogram* SP);
    // Push a lexical block under current scope. Optionally set IRBuilder debug location.
    void pushLexicalBlock(unsigned line, unsigned col = 1, llvm::IRBuilder<>* builder = nullptr);
    // Pop the current lexical scope if inside any nested block (never pops the outermost function scope).
    void popScope(llvm::IRBuilder<>* builder = nullptr);
    };

}