#include <memory>
#include <string>
#include <iostream>

#include <llvm/IR/Type.h>


// Only need minimal headers; avoid circular inclusion.
#include "edn/ir/debug.hpp"
#include "edn/ir/types.hpp"
#include "edn/ir/layout.hpp"
#include "edn/ir_emitter.hpp"

namespace edn::ir::debug {




DebugManager::DebugManager(bool enableDebugInfo, std::unique_ptr<llvm::Module>& module, edn::IREmitter* emitter)
    : module(module), emitter(emitter), enableDebugInfo(enableDebugInfo)
{
    
}

void DebugManager::initialize()
{
    if (const char *dbg = std::getenv("EDN_ENABLE_DEBUG"); dbg && std::string(dbg) == "1")
    {
        enableDebugInfo = true;
    }
    // Reset members first (in case of re-init)
    DIB.reset();
    DI_File = nullptr;
    if (enableDebugInfo)
    {
    if(const char* skip = std::getenv("EDN_SKIP_PAYLOAD_ARRAY_DI")){ if(std::string(skip)=="1") { skipPayloadArrayDI = true; std::cerr << "[di][cfg] skipping payload array DI entries\n"; } }
        // Minimal, backend-agnostic setup
        module->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
        module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 5u);
        DIB = std::make_unique<llvm::DIBuilder>(*module);
        // Use synthetic file info (tests often embed strings). Allow override via env.
        std::string srcFile = "inline.edn";
        std::string srcDir = ".";
        if (const char *sf = std::getenv("EDN_DEBUG_FILE")) { srcFile = sf; }
        if (const char *sd = std::getenv("EDN_DEBUG_DIR")) { srcDir = sd; }
        DI_File = DIB->createFile(srcFile, srcDir);
        (void)DIB->createCompileUnit(llvm::dwarf::DW_LANG_C, DI_File, "edn", /*isOptimized*/ false, "", 0);
    }
}

llvm::DIType* DebugManager::diTypeOf(edn::TypeId id)
    {
        if (!enableDebugInfo || !DIB)
            return nullptr;
        if (auto it = DITypeCache.find(id); it != DITypeCache.end())
            return it->second;
        llvm::DIType *out = nullptr;
        
    const edn::Type &T = emitter->tctx_.at(id);
        switch (T.kind)
        {
        case Type::Kind::Base:
        {
            if (T.base == BaseType::Void)
            {
                out = nullptr;
                break;
            }
            unsigned bits = base_type_bit_width(T.base);
            unsigned enc = 0;
            switch (T.base)
            {
            case BaseType::F32:
            case BaseType::F64:
                enc = llvm::dwarf::DW_ATE_float;
                break;
            case BaseType::U8:
            case BaseType::U16:
            case BaseType::U32:
            case BaseType::U64:
                enc = llvm::dwarf::DW_ATE_unsigned;
                break;
            default:
                enc = llvm::dwarf::DW_ATE_signed;
                break;
            }
            out = DIB->createBasicType(emitter->tctx_.to_string(id), bits, enc);
            break;
        }
        case Type::Kind::Pointer:
        {
            auto *pointee = diTypeOf(T.pointee);
            uint64_t psz = edn::ir::pointer_size_in_bits(*module);
            uint32_t palignBits = edn::ir::pointer_abi_alignment_in_bits(*module);
            out = DIB->createPointerType(pointee, psz, palignBits);
            break;
        }
        case Type::Kind::Struct:
        {
            // Build member list with offsets/sizes from DataLayout
            auto ftIt = emitter->struct_field_types_.find(T.struct_name);
            llvm::StructType *ST = llvm::StructType::getTypeByName(*emitter->llctx_, "struct." + T.struct_name);
            if (!ST && ftIt != emitter->struct_field_types_.end())
            {
                std::vector<llvm::Type *> elemLL;
                for (auto tid : ftIt->second)
                    elemLL.push_back(map_type(emitter->tctx_, *emitter->llctx_, tid));
                ST = llvm::StructType::create(*emitter->llctx_, elemLL, "struct." + T.struct_name);
            }
            uint64_t szBits = 0;
            uint32_t aBits = 0;
            if (ST)
            {
                szBits = edn::ir::layout::alloc_size_bytes(*module, ST) * 8;
                aBits = edn::ir::layout::abi_align_bits(*module, ST);
            }
            std::vector<llvm::Metadata *> membersMD;
            if (ftIt != emitter->struct_field_types_.end() && ST)
            {
                std::cout << "[di] building struct DI for '" << T.struct_name << "' with " << ftIt->second.size() << " fields\n";
                auto idxIt = emitter->struct_field_index_.find(T.struct_name);
                // Rebuild ordered field name list by index
                std::vector<std::string> fieldNames(ftIt->second.size());
                if (idxIt != emitter->struct_field_index_.end())
                {
                    for (const auto &p : idxIt->second)
                    {
                        if (p.second < fieldNames.size())
                            fieldNames[p.second] = p.first;
                    }
                }
                auto *SL = edn::ir::layout::struct_layout(*module, ST);
                for (size_t i = 0; i < ftIt->second.size(); ++i)
                {
                    edn::TypeId fid = ftIt->second[i];
                    auto *fDI = diTypeOf(fid);
                    llvm::Type *fLL = edn::ir::map_type(emitter->tctx_, *emitter->llctx_, fid);
                    std::cout << "[di][member] struct=" << T.struct_name << " i=" << i << " fid=" << fid
                              << " fLL.kind=" << (unsigned)fLL->getTypeID() << " fDI=" << (void*)fDI << "\n";
                    if(skipPayloadArrayDI && T.struct_name=="T" && i==1){
                        std::cout << "[di][member] SKIP array DI for struct T field 1 (diagnostic)\n";
                        fDI = nullptr; // force null DIType for this field
                    }
                    uint64_t fSizeBits = edn::ir::layout::alloc_size_bytes(*module, fLL) * 8;
                    uint32_t fAlignBits = edn::ir::layout::abi_align_bits(*module, fLL);
                    uint64_t offBits = SL ? SL->getElementOffsetInBits((unsigned)i) : 0;
                    std::string fname = (i < fieldNames.size() && !fieldNames[i].empty()) ? fieldNames[i] : ("field" + std::to_string(i));
                    unsigned fLine = 1;
                    {
                        auto flIt = emitter->struct_field_lines_.find(T.struct_name);
                        if (flIt != emitter->struct_field_lines_.end() && i < flIt->second.size() && flIt->second[i] > 0)
                        {
                            fLine = flIt->second[i];
                        }
                    }
                    auto *mem = DIB->createMemberType(/*Scope*/ DI_File, fname, DI_File, /*Line*/ fLine, fSizeBits, fAlignBits, offBits, llvm::DINode::FlagZero, fDI);
                    membersMD.push_back(mem);
                }
            }
            std::cout << "[di] built " << membersMD.size() << " member DI entries for struct '" << T.struct_name << "'\n";
            out = DIB->createStructType(DI_File, T.struct_name, DI_File, /*Line*/ 1, /*SizeInBits*/ szBits, /*AlignInBits*/ aBits, llvm::DINode::FlagZero, nullptr, DIB->getOrCreateArray(membersMD));
            break;
        }
        case Type::Kind::Array:
        {
            auto *elemTy = diTypeOf(T.elem);
            uint64_t ebits = 0;
            const Type &ET = emitter->tctx_.at(T.elem);
            if (ET.kind == Type::Kind::Base)
                ebits = base_type_bit_width(ET.base);
            uint64_t sizeBits = ebits * T.array_size;
            auto subrange = DIB->getOrCreateSubrange(0, (int64_t)T.array_size);
            // Compute ABI alignment for the full array type
            uint32_t aalignBits = 0;
            {
                llvm::Type *arrLL = llvm::ArrayType::get(edn::ir::map_type(emitter->tctx_, *emitter->llctx_, T.elem), (uint64_t)T.array_size);
                aalignBits = edn::ir::layout::abi_align_bits(*module, arrLL);
            }
            out = DIB->createArrayType(sizeBits, aalignBits, elemTy, DIB->getOrCreateArray({subrange}));
            break;
        }
        case Type::Kind::Function:
        {
            auto *retTy = diTypeOf(T.ret);
            std::vector<llvm::Metadata *> all;
            all.push_back(retTy ? static_cast<llvm::Metadata *>(retTy) : nullptr);
            for (auto pid : T.params)
            {
                auto *pti = diTypeOf(pid);
                all.push_back(static_cast<llvm::Metadata *>(pti));
            }
            auto arr = DIB->getOrCreateTypeArray(all);
            out = DIB->createSubroutineType(arr);
            break;
        }
        }
        DITypeCache[id] = out;
        return out;
    }

// -------------------- Scope stack helpers ----------------------------------
llvm::DIScope* DebugManager::currentScope() const {
    if (!scopeStack.empty()) return scopeStack.back();
    return DI_File; // may be null when debug disabled
}

void DebugManager::pushFunctionScope(llvm::DISubprogram* SP) {
    if (!enableDebugInfo || !SP) return;
    // Clear any previous stack (fresh function)
    scopeStack.clear();
    scopeStack.push_back(SP);
}

void DebugManager::pushLexicalBlock(unsigned line, unsigned col, llvm::IRBuilder<>* builder) {
    if (!enableDebugInfo || !DIB) return;
    auto *parent = currentScope();
    if (!parent) return;
    if (line == 0) line = 1;
    auto *LB = DIB->createLexicalBlock(parent, DI_File, line, col);
    scopeStack.push_back(LB);
    if (builder) {
        builder->SetCurrentDebugLocation(llvm::DILocation::get(builder->getContext(), line, col, LB));
    }
}

void DebugManager::popScope(llvm::IRBuilder<>* builder) {
    if (!enableDebugInfo) return;
    if (scopeStack.size() <= 1) return; // keep at least function scope
    scopeStack.pop_back();
    if (builder) {
        auto *scope = currentScope();
        if (scope) {
            unsigned line = 1;
            builder->SetCurrentDebugLocation(llvm::DILocation::get(builder->getContext(), line, 1, scope));
        }
    }
}

} // namespace edn::ir::debug