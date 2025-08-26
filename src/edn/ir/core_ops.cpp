#include "edn/ir/core_ops.hpp"
#include "edn/ir/layout.hpp"
#include "edn/ir/types.hpp"
#include "edn/ir/resolver.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

// Force rebuild marker (EDN-0001 slot-preference iteration). If this comment is present,
// core_ops.cpp has been recompiled after integer arithmetic operand resolution changes.

namespace edn::ir::core_ops {

using edn::Type;
using edn::TypeId;
using edn::BaseType;

static std::string symName(const edn::node_ptr &n){
    if(!n) return {};
    if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name;
    if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data);
    return {};
}
static std::string trimPct(const std::string& s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }

bool handle_integer_arith(builder::State& S, const std::vector<edn::node_ptr>& il){
    if(il.size()!=5 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    const std::string op = std::get<edn::symbol>(il[0]->data).name;
    if(op!="add" && op!="sub" && op!="mul" && op!="sdiv" && op!="udiv" && op!="srem" && op!="urem") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId ty{}; try{ ty = S.tctx.parse_type(il[2]); }catch(...){ return false; }
    auto resolveOperand = [&](const edn::node_ptr& n)->llvm::Value*{
        if(n && std::holds_alternative<edn::symbol>(n->data)){
            std::string name = trimPct(std::get<edn::symbol>(n->data).name);
            if(!name.empty()){
                if(auto vsIt = S.varSlots.find(name); vsIt != S.varSlots.end()){
                    auto tyIt = S.vtypes.find(name);
                    if(tyIt != S.vtypes.end()){
                        if(const char* dbg2 = std::getenv("EDN_DEBUG_ARITH")){
                            fprintf(stderr, "[arith][slot-load] sym=%s tyId=%llu slot=%p depth=%d\n", name.c_str(), (unsigned long long)tyIt->second, (void*)vsIt->second, S.lexicalDepth);
                        }
                        return S.builder.CreateLoad(S.map_type(tyIt->second), vsIt->second, name);
                    } else if(const char* dbg3 = std::getenv("EDN_DEBUG_ARITH")) {
                        fprintf(stderr, "[arith][slot-has-no-type] sym=%s slot=%p\n", name.c_str(), (void*)vsIt->second);
                    }
                } else if(const char* dbg4 = std::getenv("EDN_DEBUG_ARITH")) {
                    fprintf(stderr, "[arith][no-slot] sym=%s vtypeKnown=%d varSlots.size=%zu -> keys:", name.c_str(), (int)S.vtypes.count(name), S.varSlots.size());
                    size_t shown=0; for(auto &kv : S.varSlots){ if(shown<8) fprintf(stderr, " %s", kv.first.c_str()); else { fprintf(stderr, " ..."); break;} ++shown; }
                    fprintf(stderr, "\n");
                    // EDN-0001: Lazy slot promotion. If this appears to be a user variable (not internal __rl_ prefix)
                    // with a current SSA value, promote it to a slot so subsequent mutations operate on memory.
                    if(!name.empty() && !S.varSlots.count(name) && S.vmap.count(name) && !S.vtypes.empty() && name.rfind("__rl_",0)!=0){
                        auto tIt = S.vtypes.find(name);
                        if(tIt != S.vtypes.end()){
                            if(const char* dbgProm = std::getenv("EDN_DEBUG_ARITH")) fprintf(stderr, "[arith][promote-slot] sym=%s tyId=%llu\n", name.c_str(), (unsigned long long)tIt->second);
                            // Allocate slot at entry and initialize from current SSA value.
                            auto *slot = resolver::ensure_slot(S, name, tIt->second, /*initFromCurrent=*/true);
                            if(slot){
                                // Replace current SSA value with a load to keep consistency.
                                auto *loaded = S.builder.CreateLoad(S.map_type(tIt->second), slot, name);
                                S.vmap[name] = loaded;
                                return loaded; // Use loaded value as operand immediately.
                            }
                        }
                    }
                }
            }
        }
        if(const char* dbg5 = std::getenv("EDN_DEBUG_ARITH")){
            if(n && std::holds_alternative<edn::symbol>(n->data)){
                fprintf(stderr, "[arith][resolver-fallback] sym=%s\n", trimPct(std::get<edn::symbol>(n->data).name).c_str());
            }
        }
        return resolver::get_value(S, n);
    };
    auto *va = resolveOperand(il[3]);
    auto *vb = resolveOperand(il[4]);
    // EDN-0001: If either operand corresponds to a promoted variable but we still captured its
    // pre-promotion constant (e.g. %__rl_c26.cst.load), attempt a second resolution pass now that
    // the slot exists to force a load from the slot.
    auto refreshIfPromoted = [&](llvm::Value* &v, const edn::node_ptr& src){
        if(!src || !std::holds_alternative<edn::symbol>(src->data)) return;
        std::string name = trimPct(std::get<edn::symbol>(src->data).name);
        if(name.empty()) return;
        if(S.varSlots.count(name)){
            // Re-load from slot to avoid stale constant folding of initializer inside loops.
            auto tIt = S.vtypes.find(name);
            if(tIt!=S.vtypes.end()){
                if(const char* dbg2 = std::getenv("EDN_DEBUG_ARITH")) fprintf(stderr, "[arith][refresh-slot] sym=%s\n", name.c_str());
                v = S.builder.CreateLoad(S.map_type(tIt->second), S.varSlots[name], name);
            }
        }
    };
    refreshIfPromoted(va, il[3]);
    refreshIfPromoted(vb, il[4]);
    if(const char* dbg = std::getenv("EDN_DEBUG_ARITH")){
        (void)dbg;
        if(va && vb){
            fprintf(stderr, "[arith] op=%s dst=%s lhs.ty=%u rhs.ty=%u\n", op.c_str(), dst.c_str(), (unsigned)va->getType()->getTypeID(), (unsigned)vb->getType()->getTypeID());
        }
    }
    if(!va||!vb) return false;
    llvm::Value* r=nullptr;
    if(op=="add") r=S.builder.CreateAdd(va,vb,dst);
    else if(op=="sub") r=S.builder.CreateSub(va,vb,dst);
    else if(op=="mul") r=S.builder.CreateMul(va,vb,dst);
    else if(op=="sdiv") r=S.builder.CreateSDiv(va,vb,dst);
    else if(op=="udiv") r=S.builder.CreateUDiv(va,vb,dst);
    else if(op=="srem") r=S.builder.CreateSRem(va,vb,dst);
    else r=S.builder.CreateURem(va,vb,dst);
    S.vmap[dst]=r; S.vtypes[dst]=ty; return true;
}

bool handle_float_arith(builder::State& S, const std::vector<edn::node_ptr>& il){
    if(il.size()!=5 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    const std::string op = std::get<edn::symbol>(il[0]->data).name;
    if(op!="fadd" && op!="fsub" && op!="fmul" && op!="fdiv") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId ty{}; try{ ty = S.tctx.parse_type(il[2]); }catch(...){ return false; }
    auto *va = edn::ir::resolver::get_value(S, il[3]);
    auto *vb = edn::ir::resolver::get_value(S, il[4]);
    if(!va||!vb) return false;
    llvm::Value* r=nullptr;
    if(op=="fadd") r=S.builder.CreateFAdd(va,vb,dst);
    else if(op=="fsub") r=S.builder.CreateFSub(va,vb,dst);
    else if(op=="fmul") r=S.builder.CreateFMul(va,vb,dst);
    else r=S.builder.CreateFDiv(va,vb,dst);
    S.vmap[dst]=r; S.vtypes[dst]=ty; return true;
}

bool handle_bitwise_shift(builder::State& S, const std::vector<edn::node_ptr>& il, const edn::node_ptr& inst){
    if(il.size()!=5 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    const std::string op = std::get<edn::symbol>(il[0]->data).name;
    if(op!="and" && op!="or" && op!="xor" && op!="shl" && op!="lshr" && op!="ashr") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId ty{}; try{ ty = S.tctx.parse_type(il[2]); }catch(...){ return false; }
    auto *va = edn::ir::resolver::get_value(S, il[3]);
    auto *vb = edn::ir::resolver::get_value(S, il[4]);
    if(!va||!vb) return false;
    llvm::Value* r=nullptr;
    if(op=="and") r=S.builder.CreateAnd(va,vb,dst);
    else if(op=="or") r=S.builder.CreateOr(va,vb,dst);
    else if(op=="xor") r=S.builder.CreateXor(va,vb,dst);
    else if(op=="shl") r=S.builder.CreateShl(va,vb,dst);
    else if(op=="lshr") r=S.builder.CreateLShr(va,vb,dst);
    else r=S.builder.CreateAShr(va,vb,dst);
    S.vmap[dst]=r; S.vtypes[dst]=ty; if(op=="and"||op=="or"||op=="xor") S.defNode[dst]=inst; return true;
}

bool handle_ptr_add_sub(builder::State& S, const std::vector<edn::node_ptr>& il){
    if(il.size()!=5 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    const std::string op = std::get<edn::symbol>(il[0]->data).name;
    if(op!="ptr-add" && op!="ptr-sub") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId annot{}; try{ annot = S.tctx.parse_type(il[2]); }catch(...){ return false; }
    std::string baseName = trimPct(symName(il[3])); std::string offName = trimPct(symName(il[4]));
    if(baseName.empty()||offName.empty()) return false;
    auto bit = S.vmap.find(baseName); auto oit = S.vmap.find(offName);
    if(bit==S.vmap.end()||oit==S.vmap.end()) return false;
    if(!S.vtypes.count(baseName)||!S.vtypes.count(offName)) return false;
    edn::TypeId bty=S.vtypes[baseName]; const Type &BT=S.tctx.at(bty);
    const Type &AT=S.tctx.at(annot);
    if(BT.kind!=Type::Kind::Pointer || AT.kind!=Type::Kind::Pointer || BT.pointee!=AT.pointee) return false;
    edn::TypeId oty=S.vtypes[offName]; const Type &OT=S.tctx.at(oty);
    if(!(OT.kind==Type::Kind::Base && is_integer_base(OT.base))) return false;
    llvm::Value* offsetVal = oit->second;
    if(op=="ptr-sub") offsetVal = S.builder.CreateNeg(offsetVal, offName+".neg");
    llvm::Value* gep = S.builder.CreateGEP(S.map_type(AT.pointee), bit->second, offsetVal, dst);
    S.vmap[dst]=gep; S.vtypes[dst]=annot; return true;
}

bool handle_ptr_diff(builder::State& S, const std::vector<edn::node_ptr>& il){
    if(il.size()!=5 || !il[0] || !std::holds_alternative<edn::symbol>(il[0]->data)) return false;
    if(std::get<edn::symbol>(il[0]->data).name!="ptr-diff") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    edn::TypeId rty{}; try{ rty = S.tctx.parse_type(il[2]); }catch(...){ return false; }
    std::string aName = trimPct(symName(il[3])); std::string bName = trimPct(symName(il[4]));
    if(aName.empty()||bName.empty()) return false;
    if(!S.vtypes.count(aName)||!S.vtypes.count(bName)) return false;
    const Type &AT=S.tctx.at(S.vtypes[aName]); const Type &BT=S.tctx.at(S.vtypes[bName]);
    if(AT.kind!=Type::Kind::Pointer || BT.kind!=Type::Kind::Pointer || AT.pointee!=BT.pointee) return false;
    auto *aV = S.vmap[aName]; auto *bV=S.vmap[bName];
    llvm::Type* intPtrTy = llvm::Type::getInt64Ty(S.llctx);
    auto *aInt = S.builder.CreatePtrToInt(aV, intPtrTy, aName+".pi");
    auto *bInt = S.builder.CreatePtrToInt(bV, intPtrTy, bName+".pi");
    auto *rawDiff = S.builder.CreateSub(aInt, bInt, dst+".raw");
    llvm::Type* elemLL = S.map_type(AT.pointee);
    uint64_t elemSize = edn::ir::size_in_bytes(S.module, elemLL);
    llvm::Value* scale = llvm::ConstantInt::get(intPtrTy, elemSize);
    llvm::Value* elemCount = S.builder.CreateSDiv(rawDiff, scale, dst+".elts");
    llvm::Type* destTy = S.map_type(rty);
    llvm::Value* finalV = elemCount;
    if(destTy != elemCount->getType()){
        unsigned fromBits = elemCount->getType()->getIntegerBitWidth();
        unsigned toBits = destTy->getIntegerBitWidth();
        if(toBits > fromBits) finalV = S.builder.CreateSExt(elemCount, destTy, dst+".ext");
        else if(toBits < fromBits) finalV = S.builder.CreateTrunc(elemCount, destTy, dst+".trunc");
    }
    S.vmap[dst]=finalV; S.vtypes[dst]=rty; return true;
}

} // namespace edn::ir::core_ops
