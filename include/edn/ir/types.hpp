#pragma once
#include "edn/types.hpp"
#include <unordered_map>
#include <string>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>

#include "edn/ir/layout.hpp"

namespace edn::ir {

// Map an EDN TypeId to an LLVM type using the provided TypeContext and LLVMContext.
inline llvm::Type* map_type(TypeContext& tctx, llvm::LLVMContext& llctx, TypeId id){
    const Type& T = tctx.at(id);
    switch(T.kind){
        case Type::Kind::Base:
            switch(T.base){
                case BaseType::I1: return llvm::Type::getInt1Ty(llctx);
                case BaseType::I8: case BaseType::U8: return llvm::Type::getInt8Ty(llctx);
                case BaseType::I16: case BaseType::U16: return llvm::Type::getInt16Ty(llctx);
                case BaseType::I32: case BaseType::U32: return llvm::Type::getInt32Ty(llctx);
                case BaseType::I64: case BaseType::U64: return llvm::Type::getInt64Ty(llctx);
                case BaseType::F32: return llvm::Type::getFloatTy(llctx);
                case BaseType::F64: return llvm::Type::getDoubleTy(llctx);
                case BaseType::Void: return llvm::Type::getVoidTy(llctx);
            }
            break;
        case Type::Kind::Pointer:
            return llvm::PointerType::getUnqual(map_type(tctx, llctx, T.pointee));
        case Type::Kind::Struct: {
            auto* ST = llvm::StructType::getTypeByName(llctx, "struct."+T.struct_name);
            if(!ST) ST = llvm::StructType::create(llctx, "struct."+T.struct_name);
            return ST;
        }
        case Type::Kind::Function: {
            std::vector<llvm::Type*> ps; ps.reserve(T.params.size());
            for(auto p: T.params) ps.push_back(map_type(tctx, llctx, p));
            return llvm::FunctionType::get(map_type(tctx, llctx, T.ret), ps, T.variadic);
        }
        case Type::Kind::Array:
            return llvm::ArrayType::get(map_type(tctx, llctx, T.elem), (uint64_t)T.array_size);
    }
    return llvm::Type::getVoidTy(llctx);
}

// Get or create a named struct type with concrete body. Uses the provided cache.
inline llvm::StructType* get_or_create_struct(
    llvm::LLVMContext& llctx,
    TypeContext& tctx,
    std::unordered_map<std::string, llvm::StructType*>& cache,
    const std::string& name,
    const std::vector<TypeId>& fieldTypes){
    if(auto it = cache.find(name); it != cache.end()) return it->second;
    auto* ST = llvm::StructType::getTypeByName(llctx, "struct."+name);
    if(!ST) ST = llvm::StructType::create(llctx, "struct."+name);
    std::vector<llvm::Type*> elems; elems.reserve(fieldTypes.size());
    for(auto ft: fieldTypes) elems.push_back(map_type(tctx, llctx, ft));
    if(ST->isOpaque()) ST->setBody(elems, /*isPacked*/ false);
    cache[name] = ST;
    return ST;
}

// Safe DataLayout helpers (centralize DL access)
// Thin wrappers over edn::ir::layout helpers to keep existing call sites.
inline uint64_t size_in_bytes(llvm::Module& M, llvm::Type* T){
    return edn::ir::layout::alloc_size_bytes(M, T);
}

inline uint64_t pointer_size_in_bits(llvm::Module& M){
    return edn::ir::layout::pointer_size_bits(M);
}

inline uint32_t pointer_abi_alignment_in_bits(llvm::Module& M){
    return edn::ir::layout::pointer_abi_align_bits(M);
}

} // namespace edn::ir
