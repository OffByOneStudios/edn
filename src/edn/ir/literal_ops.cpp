#include "edn/ir/literal_ops.hpp"
#include "edn/ir/types.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <unordered_map>

namespace edn::ir::literal_ops {

static std::string symName(const edn::node_ptr &n){ if(!n) return {}; if(std::holds_alternative<edn::symbol>(n->data)) return std::get<edn::symbol>(n->data).name; if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; }
static std::string trimPct(const std::string& s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }

// Simple unique id for synthesized globals (per-process) used only for first creation
static uint64_t g_lit_counter = 0;
// Intern tables so identical literals reuse one global
static std::unordered_map<std::string, llvm::GlobalVariable*> g_cstr_intern; // key: decoded bytes (excluding terminator)
static std::unordered_map<std::string, llvm::GlobalVariable*> g_bytes_intern; // key: raw bytes

static std::string decode_cstr_inner(const std::string& inner){
    std::string out; out.reserve(inner.size());
    for(size_t i=0;i<inner.size(); ++i){
        char c = inner[i];
        if(c=='\\' && i+1<inner.size()){
            char n = inner[++i];
            switch(n){
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case '0': out.push_back('\0'); break;
                case 'x': {
                    if(i+2<inner.size()){
                        auto hex1 = inner[i+1]; auto hex2 = inner[i+2];
                        auto hexVal=[&](char h)->int{ if(h>='0'&&h<='9') return h-'0'; if(h>='a'&&h<='f') return 10+(h-'a'); if(h>='A'&&h<='F') return 10+(h-'A'); return -1; };
                        int v1=hexVal(hex1), v2=hexVal(hex2); if(v1>=0 && v2>=0){ out.push_back(static_cast<char>((v1<<4)|v2)); i+=2; }
                        else { out.push_back('x'); }
                    } else { out.push_back('x'); }
                } break;
                default: out.push_back(n); break; // unknown escape: treat literally
            }
        } else {
            out.push_back(c);
        }
    }
    return out;
}

bool handle_cstr(builder::State& S, const std::vector<edn::node_ptr>& il){
    // (cstr %dst "literal") – string token currently represented as symbol with quotes preserved.
    if(il.size()!=3) return false;
    if(!il[0] || !std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="cstr") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    std::string lit = symName(il[2]); if(lit.size()<2 || lit.front()!='"' || lit.back()!='"') return false;
    std::string innerRaw = lit.substr(1, lit.size()-2); // strip quotes
    std::string decoded = decode_cstr_inner(innerRaw);
    llvm::GlobalVariable* gv=nullptr;
    if(auto it = g_cstr_intern.find(decoded); it!=g_cstr_intern.end()){
        gv = it->second;
    } else {
        std::string gname = "__edn.cstr." + std::to_string(++g_lit_counter);
        llvm::ArrayType *arrTy = llvm::ArrayType::get(llvm::Type::getInt8Ty(S.llctx), decoded.size()+1);
        llvm::Constant *data = llvm::ConstantDataArray::getString(S.llctx, decoded, true); // adds terminator
        gv = new llvm::GlobalVariable(S.module, arrTy, /*isConstant*/ true, llvm::GlobalValue::PrivateLinkage, data, gname);
        gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
        gv->setAlignment(llvm::MaybeAlign(1));
        g_cstr_intern[decoded]=gv;
    }
    llvm::ArrayType *arrTy = llvm::cast<llvm::ArrayType>(gv->getValueType());
    // GEP to first element -> i8*
    llvm::IRBuilder<> &B = S.builder;
    llvm::Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx), 0);
    llvm::Value *gep = B.CreateInBoundsGEP(arrTy, gv, {zero, zero}, dst);
    edn::TypeId pI8 = S.tctx.get_pointer(S.tctx.get_base(BaseType::I8));
    S.vmap[dst]=gep; S.vtypes[dst]=pI8; return true;
}

bool handle_bytes(builder::State& S, const std::vector<edn::node_ptr>& il){
    // (bytes %dst [ i64* ]) – each element 0..255
    if(il.size()!=3) return false;
    if(!il[0] || !std::holds_alternative<edn::symbol>(il[0]->data) || std::get<edn::symbol>(il[0]->data).name!="bytes") return false;
    std::string dst = trimPct(symName(il[1])); if(dst.empty()) return false;
    if(!il[2] || !std::holds_alternative<edn::vector_t>(il[2]->data)) return false;
    auto &vals = std::get<edn::vector_t>(il[2]->data).elems; if(vals.empty()) return false;
    std::vector<uint8_t> raw; raw.reserve(vals.size());
    for(auto &v : vals){ if(!v || !std::holds_alternative<int64_t>(v->data)) return false; auto x = std::get<int64_t>(v->data); if(x<0 || x>255) return false; raw.push_back((uint8_t)x); }
    std::string key(reinterpret_cast<const char*>(raw.data()), raw.size());
    llvm::GlobalVariable* gv=nullptr;
    if(auto it = g_bytes_intern.find(key); it!=g_bytes_intern.end()){
        gv = it->second;
    } else {
        std::string gname = "__edn.bytes." + std::to_string(++g_lit_counter);
        llvm::ArrayType *arrTyNew = llvm::ArrayType::get(llvm::Type::getInt8Ty(S.llctx), raw.size());
        llvm::Constant *data = llvm::ConstantDataArray::get(S.llctx, raw);
        gv = new llvm::GlobalVariable(S.module, arrTyNew, /*isConstant*/ true, llvm::GlobalValue::PrivateLinkage, data, gname);
        gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
        gv->setAlignment(llvm::MaybeAlign(1));
        g_bytes_intern[key]=gv;
    }
    auto *arrTy = llvm::cast<llvm::ArrayType>(gv->getValueType());
    llvm::IRBuilder<> &B = S.builder;
    llvm::Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(S.llctx), 0);
    llvm::Value *gep = B.CreateInBoundsGEP(arrTy, gv, {zero, zero}, dst);
    edn::TypeId pI8 = S.tctx.get_pointer(S.tctx.get_base(BaseType::I8));
    S.vmap[dst]=gep; S.vtypes[dst]=pI8; return true;
}

} // namespace edn::ir::literal_ops
