#include "edn/ir/collect.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/ir/types.hpp"
#include "edn/ir/layout.hpp"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>

namespace edn
{
    namespace ir
    {
        namespace collect
        {

            static std::string symName(const node_ptr &n)
            {
                if (!n)
                    return {};
                if (std::holds_alternative<symbol>(n->data))
                    return std::get<symbol>(n->data).name;
                if (std::holds_alternative<std::string>(n->data))
                    return std::get<std::string>(n->data);
                return {};
            }

            void run(const std::vector<edn::node_ptr> &top, edn::IREmitter *E)
            {
                // Access IREmitter internals directly (friend) to populate caches and create types/globals
                auto &tctx = E->tctx_;
                auto &llctx = *E->llctx_;
                auto &mod = *E->module_;

                // Helper to map a type id via emitter's context
                auto mapType = [&](TypeId id) { return edn::ir::map_type(tctx, llctx, id); };

                // Pass 1: structs
                auto collect_structs = [&](const std::vector<node_ptr> &elems) {
                    for (auto &n : elems) {
                        if (!n || !std::holds_alternative<list>(n->data)) continue;
                        auto &l = std::get<list>(n->data).elems;
                        if (l.empty()) continue;
                        if (!std::holds_alternative<symbol>(l[0]->data) || std::get<symbol>(l[0]->data).name != "struct") continue;
                        std::string sname;
                        std::vector<TypeId> ftypes;
                        std::vector<std::string> fnames;
                        std::vector<unsigned> flines;
                        for (size_t i = 1; i < l.size(); ++i) {
                            if (!std::holds_alternative<keyword>(l[i]->data)) continue;
                            std::string kw = std::get<keyword>(l[i]->data).name;
                            if (++i >= l.size()) break;
                            auto val = l[i];
                            if (kw == "name") sname = symName(val);
                            else if (kw == "fields" && std::holds_alternative<vector_t>(val->data)) {
                                for (auto &f : std::get<vector_t>(val->data).elems) {
                                    if (!f || !std::holds_alternative<list>(f->data)) continue;
                                    auto &fl = std::get<list>(f->data).elems;
                                    std::string fname; TypeId fty = 0;
                                    for (size_t k = 0; k < fl.size(); ++k) {
                                        if (!std::holds_alternative<keyword>(fl[k]->data)) continue;
                                        std::string fkw = std::get<keyword>(fl[k]->data).name;
                                        if (++k >= fl.size()) break;
                                        auto v = fl[k];
                                        if (fkw == "name") fname = symName(v);
                                        else if (fkw == "type") {
                                            try { fty = tctx.parse_type(v); } catch (...) {}
                                        }
                                        if (!fname.empty() && fty) {
                                            fnames.push_back(fname);
                                            ftypes.push_back(fty);
                                            unsigned ln = (unsigned)edn::line(*f);
                                            if (ln == 0) ln = 1;
                                            flines.push_back(ln);
                                        }
                                    }
                                }
                            }
                        }
                        if (!sname.empty() && !ftypes.empty()) {
                            edn::ir::get_or_create_struct(llctx, tctx, E->struct_types_, sname, ftypes);
                            E->struct_field_types_[sname] = ftypes;
                            auto &m = E->struct_field_index_[sname];
                            for (size_t ix = 0; ix < fnames.size(); ++ix) m[fnames[ix]] = ix;
                            E->struct_field_lines_[sname] = flines;
                        }
                    }
                };
                // Collect sums: represent as { i32 tag, [N x i8] payload }
                auto collect_sums = [&](const std::vector<node_ptr> &elems)
                {
                    for (auto &n : elems)
                    {
                        if (!n || !std::holds_alternative<list>(n->data))
                            continue;
                        auto &l = std::get<list>(n->data).elems;
                        if (l.empty())
                            continue;
                        if (!std::holds_alternative<symbol>(l[0]->data) || std::get<symbol>(l[0]->data).name != "sum")
                            continue;
                        std::string sname;
                        node_ptr variantsNode;
                        for (size_t i = 1; i < l.size(); ++i)
                        {
                            if (!std::holds_alternative<keyword>(l[i]->data))
                                continue;
                            std::string kw = std::get<keyword>(l[i]->data).name;
                            if (++i >= l.size())
                                break;
                            auto val = l[i];
                            if (kw == "name")
                                sname = symName(val);
                            else if (kw == "variants")
                                variantsNode = val;
                        }
                        if (sname.empty() || !variantsNode || !std::holds_alternative<vector_t>(variantsNode->data))
                            continue;
                        std::vector<std::vector<TypeId>> variants;
                        std::unordered_map<std::string, int> vtags;
                        uint64_t maxPayload = 0;
                        int tag = 0;
                        for (auto &vn : std::get<vector_t>(variantsNode->data).elems)
                        {
                            if (!vn || !std::holds_alternative<list>(vn->data))
                                continue;
                            auto &vl = std::get<list>(vn->data).elems;
                            if (vl.empty() || !std::holds_alternative<symbol>(vl[0]->data) || std::get<symbol>(vl[0]->data).name != "variant")
                                continue;
                            std::string vname;
                            node_ptr fieldsNode;
                            for (size_t k = 1; k < vl.size(); ++k)
                            {
                                if (!std::holds_alternative<keyword>(vl[k]->data))
                                    break;
                                std::string kw = std::get<keyword>(vl[k]->data).name;
                                if (++k >= vl.size())
                                    break;
                                auto val = vl[k];
                                if (kw == "name")
                                    vname = symName(val);
                                else if (kw == "fields")
                                    fieldsNode = val;
                            }
                            std::vector<TypeId> ftys;
                            if (fieldsNode && std::holds_alternative<vector_t>(fieldsNode->data))
                            {
                for (auto &tf : std::get<vector_t>(fieldsNode->data).elems)
                                {
                                    try
                                    {
                    ftys.push_back(tctx.parse_type(tf));
                                    }
                                    catch (...)
                                    {
                                    }
                                }
                            }
                            variants.push_back(ftys);
                            if (!vname.empty())
                                vtags[vname] = tag; // compute payload size
                            uint64_t sz = 0;
                            for (auto &tid : ftys)
                            {
                                llvm::Type *ll = mapType(tid);
                                sz += edn::ir::size_in_bytes(mod, ll);
                            }
                            if (sz > maxPayload)
                                maxPayload = sz;
                            ++tag;
                        }
                        // create struct type if absent
                        auto *tagTy = llvm::Type::getInt32Ty(llctx);
                        uint64_t payloadBytes = maxPayload ? maxPayload : 1;
                        auto *payloadArr = llvm::ArrayType::get(llvm::Type::getInt8Ty(llctx), payloadBytes);
                        auto *ST = llvm::StructType::getTypeByName(llctx, "struct." + sname);
                        if (!ST)
                            ST = llvm::StructType::create(llctx, {tagTy, payloadArr}, "struct." + sname);
                        else if (ST->isOpaque()) ST->setBody({tagTy, payloadArr}, false);
                        E->struct_field_types_[sname] = {tctx.get_base(BaseType::I32), tctx.get_array(tctx.get_base(BaseType::I8), payloadBytes)};
                        E->struct_field_index_[sname]["tag"] = 0;
                        E->struct_field_index_[sname]["payload"] = 1;
                        E->sum_variant_field_types_[sname] = variants;
                        E->sum_variant_tag_[sname] = vtags;
                        E->sum_payload_size_[sname] = payloadBytes;
                    }
                };
                // Represent unions as single-field struct of byte array big enough to hold largest field (simplified); for loads we bitcast
                auto collect_unions = [&](const std::vector<node_ptr> &elems) {
                    for (auto &n : elems) {
                        if (!n || !std::holds_alternative<list>(n->data)) continue;
                        auto &l = std::get<list>(n->data).elems;
                        if (l.empty()) continue;
                        if (!std::holds_alternative<symbol>(l[0]->data) || std::get<symbol>(l[0]->data).name != "union") continue;
                        std::string uname;
                        std::vector<std::pair<std::string, TypeId>> fields;
                        for (size_t i = 1; i < l.size(); ++i) {
                            if (!std::holds_alternative<keyword>(l[i]->data)) continue;
                            std::string kw = std::get<keyword>(l[i]->data).name;
                            if (++i >= l.size()) break;
                            auto val = l[i];
                            if (kw == "name") uname = symName(val);
                            else if (kw == "fields" && std::holds_alternative<vector_t>(val->data)) {
                                for (auto &f : std::get<vector_t>(val->data).elems) {
                                    if (!f || !std::holds_alternative<list>(f->data)) continue;
                                    auto &fl = std::get<list>(f->data).elems;
                                    if (fl.empty() || !std::holds_alternative<symbol>(fl[0]->data) || std::get<symbol>(fl[0]->data).name != "ufield") continue;
                                    std::string fname; TypeId fty = 0;
                                    for (size_t k = 1; k < fl.size(); ++k) {
                                        if (!std::holds_alternative<keyword>(fl[k]->data)) break;
                                        std::string fkw = std::get<keyword>(fl[k]->data).name;
                                        if (++k >= fl.size()) break;
                                        auto v = fl[k];
                                        if (fkw == "name") fname = symName(v);
                                        else if (fkw == "type") { try { fty = tctx.parse_type(v); } catch (...) {} }
                                    }
                                    if (!fname.empty() && fty) fields.emplace_back(fname, fty);
                                }
                            }
                        }
                        if (uname.empty() || fields.empty()) continue;
                        uint64_t maxSize = 0; std::unordered_map<std::string, TypeId> ftypes;
                        for (auto &p : fields) {
                            llvm::Type *llT = mapType(p.second);
                            uint64_t sz = edn::ir::size_in_bytes(mod, llT);
                            if (sz > maxSize) maxSize = sz;
                            ftypes[p.first] = p.second;
                        }
                        uint64_t bytes = maxSize ? maxSize : 1;
                        llvm::ArrayType *storageArr = llvm::ArrayType::get(llvm::Type::getInt8Ty(llctx), bytes);
                        auto *ST = llvm::StructType::getTypeByName(llctx, "struct." + uname);
                        if (!ST) ST = llvm::StructType::create(llctx, {storageArr}, "struct." + uname);
                        else if (ST->isOpaque()) ST->setBody({storageArr}, false);
                        E->struct_field_types_[uname] = { tctx.get_array(tctx.get_base(BaseType::I8), bytes) };
                        E->struct_field_index_[uname]["__storage"] = 0;
                        E->union_field_types_[uname] = ftypes;
                    }
                };
                auto emit_globals = [&](const std::vector<node_ptr> &elems) {
                    for (auto &n : elems) {
                        if (!n || !std::holds_alternative<list>(n->data)) continue;
                        auto &l = std::get<list>(n->data).elems;
                        if (l.empty()) continue;
                        if (!std::holds_alternative<symbol>(l[0]->data) || std::get<symbol>(l[0]->data).name != "global") continue;
                        std::string gname; TypeId gty = 0; node_ptr init; bool isConst = false;
                        for (size_t i = 1; i < l.size(); ++i) {
                            if (!std::holds_alternative<keyword>(l[i]->data)) continue;
                            std::string kw = std::get<keyword>(l[i]->data).name;
                            if (++i >= l.size()) break;
                            auto v = l[i];
                            if (kw == "name") gname = symName(v);
                            else if (kw == "type") { try { gty = tctx.parse_type(v); } catch (...) {} }
                            else if (kw == "init") init = v;
                            else if (kw == "const" && std::holds_alternative<bool>(v->data)) isConst = std::get<bool>(v->data);
                        }
                        if (gname.empty() || !gty) continue;
                        llvm::Type *lty = mapType(gty);
                        llvm::Constant *c = nullptr;
                        if (init) {
                            if (std::holds_alternative<int64_t>(init->data)) c = llvm::ConstantInt::get(lty, (uint64_t)std::get<int64_t>(init->data), true);
                            else if (std::holds_alternative<double>(init->data)) c = llvm::ConstantFP::get(lty, std::get<double>(init->data));
                            else if (std::holds_alternative<vector_t>(init->data)) {
                                const Type &T = tctx.at(gty);
                                if (T.kind == Type::Kind::Array) {
                                    auto &elemsV = std::get<vector_t>(init->data).elems;
                                    if (elemsV.size() == T.array_size) {
                                        std::vector<llvm::Constant *> elemsC; elemsC.reserve(elemsV.size());
                                        llvm::Type *eltTy = mapType(T.elem);
                                        bool ok = true;
                                        for (auto &e : elemsV) {
                                            if (!e) { ok = false; break; }
                                            if (std::holds_alternative<int64_t>(e->data)) elemsC.push_back(llvm::ConstantInt::get(eltTy, (uint64_t)std::get<int64_t>(e->data), true));
                                            else if (std::holds_alternative<double>(e->data)) elemsC.push_back(llvm::ConstantFP::get(eltTy, std::get<double>(e->data)));
                                            else { ok = false; break; }
                                        }
                                        if (ok) c = llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(lty), elemsC);
                                    }
                                } else if (T.kind == Type::Kind::Struct) {
                                    auto &elemsV = std::get<vector_t>(init->data).elems;
                                    auto ftIt = E->struct_field_types_.find(T.struct_name);
                                    if (ftIt != E->struct_field_types_.end() && elemsV.size() == ftIt->second.size()) {
                                        std::vector<llvm::Constant *> fieldConsts; fieldConsts.reserve(elemsV.size()); bool ok = true;
                                        for (size_t fi = 0; fi < elemsV.size(); ++fi) {
                                            auto &e = elemsV[fi]; if (!e) { ok = false; break; }
                                            const Type &FT = tctx.at(ftIt->second[fi]);
                                            if (FT.kind != Type::Kind::Base) { ok = false; break; }
                                            llvm::Type *flty = mapType(ftIt->second[fi]);
                                            if (std::holds_alternative<int64_t>(e->data) && is_integer_base(FT.base)) fieldConsts.push_back(llvm::ConstantInt::get(flty, (uint64_t)std::get<int64_t>(e->data), true));
                                            else if (std::holds_alternative<double>(e->data) && is_float_base(FT.base)) fieldConsts.push_back(llvm::ConstantFP::get(flty, std::get<double>(e->data)));
                                            else if (std::holds_alternative<int64_t>(e->data) && is_float_base(FT.base)) fieldConsts.push_back(llvm::ConstantFP::get(flty, (double)std::get<int64_t>(e->data)));
                                            else { ok = false; break; }
                                        }
                                        if (ok) {
                                            auto *ST = llvm::StructType::getTypeByName(llctx, "struct." + T.struct_name);
                                            if (!ST) {
                                                std::vector<llvm::Type *> lt; for (auto tid : ftIt->second) lt.push_back(mapType(tid));
                                                ST = llvm::StructType::create(llctx, lt, "struct." + T.struct_name);
                                            }
                                            c = llvm::ConstantStruct::get(ST, fieldConsts);
                                        }
                                    }
                                }
                            }
                        }
                        if (!c) c = llvm::Constant::getNullValue(lty);
                        auto *gv = new llvm::GlobalVariable(mod, lty, isConst, llvm::GlobalValue::ExternalLinkage, c, gname);
                        (void)gv;
                    }
                };
                collect_structs(top);
                collect_unions(top);
                collect_sums(top);
                emit_globals(top);
            }

        }
    }
} // namespace edn::ir::collect
