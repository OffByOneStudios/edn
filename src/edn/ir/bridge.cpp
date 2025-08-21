#include "edn/ir_emitter.hpp"
#include "edn/ir/types.hpp"

namespace edn {

// Provide definitions for IREmitter methods declared in ir_emitter.hpp
// but implemented in the modular ir layer, so linkers can resolve them
// without moving the whole emitter yet.

llvm::StructType* IREmitter::get_or_create_struct(const std::string& name, const std::vector<TypeId>& field_types){
    // Maintain a small cache to preserve previous behavior
    if(auto it = struct_types_.find(name); it != struct_types_.end()) return it->second;
    auto* ST = edn::ir::get_or_create_struct(*llctx_, tctx_, struct_types_, name, field_types);
    return ST;
}

llvm::Type* IREmitter::map_type(TypeId id){
    return edn::ir::map_type(tctx_, *llctx_, id);
}

} // namespace edn
