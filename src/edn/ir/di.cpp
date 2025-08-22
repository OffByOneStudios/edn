// di.cpp - Debug Info (DI) helper module implementation (skeleton)

#include "edn/ir/di.hpp"

#include <llvm/IR/DIBuilder.h>
#include <llvm/BinaryFormat/Dwarf.h>

namespace edn::ir::di {

llvm::DISubprogram* attach_function_debug(debug::DebugManager& dbg,
                                          llvm::Function& F,
                                          const std::string& fname,
                                          edn::TypeId retTy,
                                          const std::vector<std::pair<std::string, edn::TypeId>>& params,
                                          unsigned lineNo)
{
    if(!dbg.enableDebugInfo || !dbg.DIB || !dbg.DI_File)
        return nullptr;
    if(lineNo == 0) lineNo = 1;
    // Build signature (return type + param types)
    std::vector<llvm::Metadata*> sigTys;
    sigTys.push_back(static_cast<llvm::Metadata*>(dbg.diTypeOf(retTy))); // may be nullptr for void
    for(auto &pr : params){ sigTys.push_back(static_cast<llvm::Metadata*>(dbg.diTypeOf(pr.second))); }
    auto *subTy = dbg.DIB->createSubroutineType(dbg.DIB->getOrCreateTypeArray(sigTys));
    auto *SP = dbg.DIB->createFunction(
        /*Scope*/ dbg.currentScope() ? dbg.currentScope() : dbg.DI_File,
        /*Name*/ fname,
        /*LinkageName*/ fname,
        /*File*/ dbg.DI_File,
        /*LineNo*/ lineNo,
        /*Type*/ subTy,
        /*ScopeLine*/ lineNo,
        /*Flags*/ llvm::DINode::FlagZero,
        /*SPFlags*/ llvm::DISubprogram::SPFlagDefinition);
    F.setSubprogram(SP);
    dbg.pushFunctionScope(SP);
    if(!F.getSubprogram()) {
        fprintf(stderr, "[dbg] attach_function_debug: failed to set subprogram for %s\n", fname.c_str());
    }
    return SP;
}

void emit_parameter_debug(debug::DebugManager& dbg,
                          llvm::Function& F,
                          llvm::IRBuilder<>& builder,
                          const std::unordered_map<std::string, edn::TypeId>& vtypes)
{
    if(!dbg.enableDebugInfo || !dbg.DIB || !F.getSubprogram()) return;
    builder.SetCurrentDebugLocation(llvm::DILocation::get(F.getContext(), F.getSubprogram()->getLine(), 1, dbg.currentScope() ? dbg.currentScope() : F.getSubprogram()));
    unsigned argIndex = 1; // DWARF argument indices start at 1
    auto *SP = F.getSubprogram();
    for(auto &arg : F.args()){
        std::string an = std::string(arg.getName());
        llvm::DIType* diTy = nullptr;
        if(auto it = vtypes.find(an); it != vtypes.end()) diTy = dbg.diTypeOf(it->second);
        auto *pvar = dbg.DIB->createParameterVariable(SP, an, argIndex, dbg.DI_File, SP->getLine(), diTy, true);
        auto *expr = dbg.DIB->createExpression();
        (void)dbg.DIB->insertDbgValueIntrinsic(&arg, pvar, expr, builder.getCurrentDebugLocation(), builder.GetInsertBlock());
        ++argIndex;
    }
}

void declare_local(debug::DebugManager& dbg,
                   llvm::IRBuilder<>& builder,
                   llvm::Value* value,
                   const std::string& name,
                   edn::TypeId ty,
                   unsigned lineNo)
{
    if(!dbg.enableDebugInfo || !dbg.DIB) return; // skeleton: unimplemented detailed local semantics
    if(auto *SP = builder.GetInsertBlock()->getParent()->getSubprogram()){
        if(lineNo == 0) lineNo = SP->getLine();
        llvm::DIType* diTy = dbg.diTypeOf(ty);
    auto *scope = dbg.currentScope();
    auto *var = dbg.DIB->createAutoVariable(scope ? scope : SP, name, dbg.DI_File, lineNo, diTy, true);
        auto *expr = dbg.DIB->createExpression();
        dbg.DIB->insertDbgValueIntrinsic(value, var, expr, builder.getCurrentDebugLocation(), builder.GetInsertBlock());
    }
}

} // namespace edn::ir::di
