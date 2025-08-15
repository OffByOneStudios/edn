#pragma once
#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.inl"
#include <memory>
#include <unordered_map>
#include <vector>

// Always have LLVM available
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>

namespace edn {

// Simple IR emitter for a single EDN module -> LLVM Module (subset of instructions)
class IREmitter {
public:
    explicit IREmitter(TypeContext& tctx);
    ~IREmitter();
    // Emits a module; runs type checking first (caller may also pre-run).
    // Returns nullptr on failure (errors filled into tc_result).
    llvm::Module* emit(const node_ptr& module_ast, TypeCheckResult& tc_result);
    // Ownership transfer into ORC JIT
    llvm::orc::ThreadSafeModule toThreadSafeModule();
    // Expose struct creation for helper utilities
    llvm::StructType* get_or_create_struct(const std::string& name, const std::vector<TypeId>& field_types);
private:
    TypeContext& tctx_;
    std::unique_ptr<llvm::LLVMContext> llctx_;
    std::unique_ptr<llvm::Module> module_;
    std::unordered_map<std::string, llvm::StructType*> struct_types_;
    std::unordered_map<std::string, std::vector<TypeId>> struct_field_types_;
    std::unordered_map<std::string, std::unordered_map<std::string,size_t>> struct_field_index_; // struct name -> field name -> index
    // For unions we represent each union as a single-field struct containing a byte array big enough to hold the largest field.
    // We still need to remember each field's logical type so that (union-member ...) can bitcast and load correctly.
    std::unordered_map<std::string, std::unordered_map<std::string,TypeId>> union_field_types_; // union name -> field name -> TypeId

    llvm::Type* map_type(TypeId id);
    
};

} // namespace edn
