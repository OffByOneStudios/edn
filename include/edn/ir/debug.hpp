#pragma once

// Minimal includes to avoid circular dependency with ir_emitter.hpp
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Module.h>

#include <memory>
#include <unordered_map>

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

    public:
        bool enableDebugInfo = false; // Set to true if debug info should be generated
        std::unique_ptr<llvm::DIBuilder> DIB;
        llvm::DIFile *DI_File;

    public:
        std::unordered_map<edn::TypeId, llvm::DIType *> DITypeCache;

    public:
        DebugManager(bool enableDebugInfo, std::unique_ptr<llvm::Module> &module, edn::IREmitter *emitter);

        void initialize();

        llvm::DIType *diTypeOf(edn::TypeId id);
    };

}