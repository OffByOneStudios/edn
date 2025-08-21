#pragma once
#include <cstdint>
#include <llvm/IR/Type.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>

namespace edn::ir::layout {

inline bool is_sized(const llvm::Type* T){ return T && T->isSized(); }

inline uint64_t alloc_size_bytes(const llvm::Module& M, const llvm::Type* T){
    if(!T || !is_sized(T)) return 0;
    return M.getDataLayout().getTypeAllocSize(const_cast<llvm::Type*>(T));
}

inline uint32_t abi_align_bits(const llvm::Module& M, const llvm::Type* T){
    if(!T || !is_sized(T)) return 0;
    return static_cast<uint32_t>(M.getDataLayout().getABITypeAlign(const_cast<llvm::Type*>(T)).value()) * 8;
}

inline const llvm::StructLayout* struct_layout(const llvm::Module& M, const llvm::StructType* ST){
    if(!ST || !is_sized(ST)) return nullptr;
    return M.getDataLayout().getStructLayout(const_cast<llvm::StructType*>(ST));
}

inline uint64_t pointer_size_bits(const llvm::Module& M){
    auto bits = M.getDataLayout().getPointerSizeInBits();
    return bits ? bits : 64;
}

inline uint32_t pointer_abi_align_bits(const llvm::Module& M){
    return static_cast<uint32_t>(M.getDataLayout().getPointerABIAlignment(0).value()) * 8;
}

} // namespace edn::ir::layout
