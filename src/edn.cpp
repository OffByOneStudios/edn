// edn.cpp - IR emitter implementation (reformatted for clarity)
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>

#include <cstdlib>
#include <iostream>

namespace edn {

IREmitter::IREmitter(TypeContext& tctx) : tctx_(tctx) {
	llctx_ = std::make_unique<llvm::LLVMContext>();
}

IREmitter::~IREmitter() = default;

llvm::Type* IREmitter::map_type(TypeId id) {
	const Type& T = tctx_.at(id);
	switch (T.kind) {
		case Type::Kind::Base: {
			switch (T.base) {
				case BaseType::I1:  return llvm::Type::getInt1Ty(*llctx_);
				case BaseType::I8:  return llvm::Type::getInt8Ty(*llctx_);
				case BaseType::I16: return llvm::Type::getInt16Ty(*llctx_);
				case BaseType::I32: return llvm::Type::getInt32Ty(*llctx_);
				case BaseType::I64: return llvm::Type::getInt64Ty(*llctx_);
				case BaseType::F32: return llvm::Type::getFloatTy(*llctx_);
				case BaseType::F64: return llvm::Type::getDoubleTy(*llctx_);
				case BaseType::Void:return llvm::Type::getVoidTy(*llctx_);
			}
			break; // fallthrough not intended
		}
		case Type::Kind::Pointer:
			return llvm::PointerType::getUnqual(map_type(T.pointee));
		case Type::Kind::Struct: {
			if (auto existing = llvm::StructType::getTypeByName(*llctx_, "struct." + T.struct_name))
				return existing;
			return llvm::StructType::create(*llctx_, "struct." + T.struct_name);
		}
		case Type::Kind::Function: {
			std::vector<llvm::Type*> params; params.reserve(T.params.size());
			for (auto p : T.params) params.push_back(map_type(p));
			return llvm::FunctionType::get(map_type(T.ret), params, T.variadic);
		}
		case Type::Kind::Array:
			return llvm::ArrayType::get(map_type(T.elem), (uint64_t)T.array_size);
	}
	return llvm::Type::getVoidTy(*llctx_);
}

llvm::StructType* IREmitter::get_or_create_struct(const std::string& name,
												  const std::vector<TypeId>& field_types) {
	if (auto it = struct_types_.find(name); it != struct_types_.end())
		return it->second;
	std::string llvmName = "struct." + name;
	auto* ST = llvm::StructType::getTypeByName(*llctx_, llvmName);
	if (!ST) ST = llvm::StructType::create(*llctx_, llvmName);
	std::vector<llvm::Type*> elems; elems.reserve(field_types.size());
	for (auto ft : field_types) elems.push_back(map_type(ft));
	if (ST->isOpaque()) ST->setBody(elems, false);
	struct_types_[name] = ST;
	return ST;
}

// Helper: collect struct declarations from module AST
static void collect_structs(IREmitter& em,
							TypeContext& tctx,
							const node_ptr& module_ast,
							std::unordered_map<std::string, std::vector<TypeId>>& field_types_out,
							std::unordered_map<std::string, std::unordered_map<std::string, size_t>>& field_index_out) {
	if (!module_ast || !std::holds_alternative<list>(module_ast->data)) return;
	auto& l = std::get<list>(module_ast->data).elems;
	for (size_t i = 1; i < l.size(); ++i) {
		auto sn = l[i];
		if (!sn || !std::holds_alternative<list>(sn->data)) continue;
		auto& sl = std::get<list>(sn->data).elems;
		if (sl.empty()) continue;
		if (!std::holds_alternative<symbol>(sl[0]->data) || std::get<symbol>(sl[0]->data).name != "struct") continue;

		std::string sname;
		std::vector<TypeId> ftypes;
		std::vector<std::string> fnames;
		for (size_t j = 1; j < sl.size(); ++j) {
			if (!(sl[j] && std::holds_alternative<keyword>(sl[j]->data))) continue;
			std::string kw = std::get<keyword>(sl[j]->data).name;
			if (++j >= sl.size()) break;
			auto val = sl[j];
			if (kw == "name") {
				if (std::holds_alternative<symbol>(val->data))      sname = std::get<symbol>(val->data).name;
				else if (std::holds_alternative<std::string>(val->data)) sname = std::get<std::string>(val->data);
			} else if (kw == "fields" && val && std::holds_alternative<vector_t>(val->data)) {
				for (auto& f : std::get<vector_t>(val->data).elems) {
					if (!f || !std::holds_alternative<list>(f->data)) continue;
					auto& fl = std::get<list>(f->data).elems;
					std::string fieldName;
					TypeId fieldTy = 0;
					for (size_t k = 0; k < fl.size(); ++k) {
						if (fl[k] && std::holds_alternative<keyword>(fl[k]->data)) {
							std::string fkw = std::get<keyword>(fl[k]->data).name;
							if (++k >= fl.size()) break;
							auto v = fl[k];
							if (fkw == "name" && std::holds_alternative<symbol>(v->data)) fieldName = std::get<symbol>(v->data).name;
							else if (fkw == "type") { try { fieldTy = tctx.parse_type(v); } catch (...) { } }
						}
					}
					if (fieldTy) { ftypes.push_back(fieldTy); fnames.push_back(fieldName); }
				}
			}
		}
		if (!sname.empty() && !ftypes.empty()) {
			em.get_or_create_struct(sname, ftypes);
			field_types_out[sname] = ftypes;
			std::unordered_map<std::string, size_t> fmap;
			for (size_t fi = 0; fi < fnames.size(); ++fi) fmap[fnames[fi]] = fi;
			field_index_out[sname] = std::move(fmap);
		}
	}
}

llvm::Module* IREmitter::emit(const node_ptr& module_ast, TypeCheckResult& tc_result) {
	// Type check first
	TypeChecker tc(tctx_);
	tc_result = tc.check_module(module_ast);
	if (!tc_result.success) return nullptr;

	module_ = std::make_unique<llvm::Module>("edn_module", *llctx_);

	// Collect structs
	collect_structs(*this, tctx_, module_ast, struct_field_types_, struct_field_index_);

	// Collect and emit globals ( (global :name myG :type i32 [:init 5]) )
	if (module_ast && std::holds_alternative<list>(module_ast->data)) {
		auto& l = std::get<list>(module_ast->data).elems;
		for (size_t i = 1; i < l.size(); ++i) {
			auto g = l[i];
			if (!g || !std::holds_alternative<list>(g->data)) continue;
			auto& gl = std::get<list>(g->data).elems;
			if (gl.empty() || !std::holds_alternative<symbol>(gl[0]->data) || std::get<symbol>(gl[0]->data).name != "global") continue;
			std::string gname; TypeId gty = tctx_.get_base(BaseType::I32); node_ptr initNode; bool haveInit=false;
			for (size_t j=1; j<gl.size(); ++j){ if(!(gl[j]&&std::holds_alternative<keyword>(gl[j]->data))) continue; std::string kw=std::get<keyword>(gl[j]->data).name; if(++j>=gl.size()) break; auto val=gl[j]; if(kw=="name" && std::holds_alternative<symbol>(val->data)) gname=std::get<symbol>(val->data).name; else if(kw=="type") gty=tctx_.parse_type(val); else if(kw=="init") { initNode=val; haveInit=true; } }
			if(gname.empty()) continue; llvm::Type* lty = map_type(gty); llvm::Constant* initC=nullptr; if(haveInit){ if(initNode && std::holds_alternative<int64_t>(initNode->data) && lty->isIntegerTy()){ initC = llvm::ConstantInt::get(lty, (uint64_t)std::get<int64_t>(initNode->data), true); } else if(initNode && std::holds_alternative<double>(initNode->data) && lty->isFloatingPointTy()){ initC = llvm::ConstantFP::get(lty, std::get<double>(initNode->data)); } }
			if(!initC) initC = llvm::Constant::getNullValue(lty);
			if(!module_->getGlobalVariable(gname)) new llvm::GlobalVariable(*module_, lty, false, llvm::GlobalValue::ExternalLinkage, initC, gname);
		}
	}

	// Set module id if present
	if (module_ast && std::holds_alternative<list>(module_ast->data)) {
		auto& l = std::get<list>(module_ast->data).elems;
		for (size_t i = 1; i + 1 < l.size(); ++i) {
			if (l[i] && std::holds_alternative<keyword>(l[i]->data) && std::get<keyword>(l[i]->data).name == "id") {
				if (std::holds_alternative<std::string>(l[i + 1]->data))
					module_->setModuleIdentifier(std::get<std::string>(l[i + 1]->data));
			}
		}
	}

	// Pre-pass: create all function prototypes to allow forward calls
	std::unordered_map<std::string, llvm::Function*> function_map;
	if (module_ast && std::holds_alternative<list>(module_ast->data)) {
		auto& l = std::get<list>(module_ast->data).elems;
		for (size_t i = 1; i < l.size(); ++i) {
			auto fnnode = l[i];
			if (!fnnode || !std::holds_alternative<list>(fnnode->data)) continue;
			auto& fl = std::get<list>(fnnode->data).elems;
			if (fl.empty()) continue;
			if (!std::holds_alternative<symbol>(fl[0]->data) || std::get<symbol>(fl[0]->data).name != "fn") continue;
			std::string fname; TypeId ret_ty = tctx_.get_base(BaseType::Void); std::vector<TypeId> param_types;
			for (size_t j = 1; j < fl.size(); ++j) {
				if (!(fl[j] && std::holds_alternative<keyword>(fl[j]->data))) continue;
				std::string kw = std::get<keyword>(fl[j]->data).name;
				if (++j >= fl.size()) break; auto val = fl[j];
				if (kw == "name") { if (std::holds_alternative<std::string>(val->data)) fname = std::get<std::string>(val->data); }
				else if (kw == "ret") { ret_ty = tctx_.parse_type(val); }
				else if (kw == "params" && val && std::holds_alternative<vector_t>(val->data)) {
					for (auto& p : std::get<vector_t>(val->data).elems) {
						if (!p || !std::holds_alternative<list>(p->data)) continue; auto& pl = std::get<list>(p->data).elems; if (pl.size() != 3) continue;
						if (!std::holds_alternative<symbol>(pl[0]->data) || std::get<symbol>(pl[0]->data).name != "param") continue;
						TypeId pty = tctx_.parse_type(pl[1]); param_types.push_back(pty);
					}
				}
			}
			if (fname.empty()) fname = "anon" + std::to_string(i);
			TypeId fnty_id = tctx_.get_function(param_types, ret_ty, false);
			auto* fty = llvm::cast<llvm::FunctionType>(map_type(fnty_id));
			if (!module_->getFunction(fname)) {
				auto* Fdecl = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, fname, module_.get());
				function_map[fname] = Fdecl;
			} else {
				function_map[fname] = module_->getFunction(fname);
			}
		}
	}

	// Emit function bodies
	if (module_ast && std::holds_alternative<list>(module_ast->data)) {
		auto& l = std::get<list>(module_ast->data).elems;
		for (size_t i = 1; i < l.size(); ++i) {
			auto fnnode = l[i];
			if (!fnnode || !std::holds_alternative<list>(fnnode->data)) continue;
			auto& fl = std::get<list>(fnnode->data).elems;
			if (fl.empty()) continue;
			if (!std::holds_alternative<symbol>(fl[0]->data) || std::get<symbol>(fl[0]->data).name != "fn") continue;

			bool trace = std::getenv("EDN_TRACE_IR") != nullptr;

			std::string fname;
			TypeId ret_ty = tctx_.get_base(BaseType::Void);
			std::vector<TypeId> param_types;
			std::vector<std::string> param_names;

			for (size_t j = 1; j < fl.size(); ++j) {
				if (!(fl[j] && std::holds_alternative<keyword>(fl[j]->data))) continue;
				std::string kw = std::get<keyword>(fl[j]->data).name;
				if (++j >= fl.size()) break;
				auto val = fl[j];
				if (kw == "name") {
					if (std::holds_alternative<std::string>(val->data)) fname = std::get<std::string>(val->data);
				} else if (kw == "ret") {
					ret_ty = tctx_.parse_type(val);
				} else if (kw == "params" && val && std::holds_alternative<vector_t>(val->data)) {
					for (auto& p : std::get<vector_t>(val->data).elems) {
						if (!p || !std::holds_alternative<list>(p->data)) continue;
						auto& pl = std::get<list>(p->data).elems;
						if (pl.size() != 3) continue;
						if (!std::holds_alternative<symbol>(pl[0]->data) || std::get<symbol>(pl[0]->data).name != "param") continue;
						TypeId pty = tctx_.parse_type(pl[1]);
						std::string pn;
						if (std::holds_alternative<symbol>(pl[2]->data)) pn = std::get<symbol>(pl[2]->data).name;
						if (!pn.empty() && pn[0] == '%') pn.erase(0, 1);
						param_types.push_back(pty);
						param_names.push_back(pn);
					}
				}
			}

			if (fname.empty()) fname = "anon" + std::to_string(i);
			TypeId fnty_id = tctx_.get_function(param_types, ret_ty, false);
			auto* fty = llvm::cast<llvm::FunctionType>(map_type(fnty_id));
			auto* F = function_map.count(fname) ? function_map[fname] : llvm::Function::Create(fty, llvm::Function::ExternalLinkage, fname, module_.get());
			function_map[fname] = F; // ensure map updated
			if (trace) std::cout << "[emit] created function '" << fname << "'\n";

			std::unordered_map<std::string, llvm::Value*> vmap;
			std::unordered_map<std::string, TypeId> vtypes;
			unsigned aidx = 0;
			for (auto& argRef : F->args()) {
				if (aidx < param_names.size() && !param_names[aidx].empty()) {
					argRef.setName(param_names[aidx]);
					vmap[param_names[aidx]] = &argRef;
					vtypes[param_names[aidx]] = param_types[aidx];
				}
				++aidx;
			}

			auto* entry = llvm::BasicBlock::Create(*llctx_, "entry", F);
			llvm::IRBuilder<> builder(entry);

			const vector_t* body_vec = nullptr;
			for (size_t j = 1; j < fl.size(); ++j) {
				if (!(fl[j] && std::holds_alternative<keyword>(fl[j]->data))) continue;
				std::string kw = std::get<keyword>(fl[j]->data).name;
				if (++j >= fl.size()) break;
				auto val = fl[j];
				if (kw == "body" && val && std::holds_alternative<vector_t>(val->data))
					body_vec = &std::get<vector_t>(val->data);
			}

			if (body_vec) {
				if (trace) std::cout << "[emit] lowering body size=" << body_vec->elems.size() << " fn='" << fname << "'\n";

				auto symName = [](const node_ptr& n) -> std::string {
					if (n && std::holds_alternative<symbol>(n->data)) return std::get<symbol>(n->data).name;
					return {};
				};
				auto trimPct = [](std::string s) {
					if (!s.empty() && s[0] == '%') s.erase(0, 1);
					return s;
				};

				std::vector<llvm::BasicBlock*> loopEndStack;
				int cfCounter = 0;

				std::function<bool(const std::vector<node_ptr>&)> emit_list;
				emit_list = [&](const std::vector<node_ptr>& insts) -> bool {
					for (auto& inst : insts) {
						if (!inst || !std::holds_alternative<list>(inst->data)) continue;
						auto& il = std::get<list>(inst->data).elems;
						if (il.empty()) continue;
						if (!std::holds_alternative<symbol>(il[0]->data)) continue;
						std::string op = std::get<symbol>(il[0]->data).name;

						if (op == "const" && il.size() == 4) {
							std::string dst = trimPct(symName(il[1]));
							if (dst.empty()) continue;
							TypeId ty = tctx_.parse_type(il[2]);
							llvm::Type* lty = map_type(ty);
							llvm::Value* cval = nullptr;
							if (std::holds_alternative<int64_t>(il[3]->data)) {
								auto v = (uint64_t)std::get<int64_t>(il[3]->data);
								cval = llvm::ConstantInt::get(lty, v, true);
							} else if (std::holds_alternative<double>(il[3]->data)) {
								double v = std::get<double>(il[3]->data);
								cval = llvm::ConstantFP::get(lty, v);
							}
							if (!cval) cval = llvm::UndefValue::get(lty);
							vmap[dst] = cval; vtypes[dst] = ty;
						}
						else if ((op == "add" || op == "sub" || op == "mul" || op == "sdiv") && il.size() == 5) {
							std::string dst = trimPct(symName(il[1]));
							TypeId ty = tctx_.parse_type(il[2]);
							std::string a = trimPct(symName(il[3]));
							std::string b = trimPct(symName(il[4]));
							if (a.empty() || b.empty() || dst.empty()) continue;
							auto* va = vmap.count(a) ? vmap[a] : nullptr;
							auto* vb = vmap.count(b) ? vmap[b] : nullptr;
							if (!va || !vb) continue;
							llvm::Value* res = nullptr;
							if (op == "add") res = builder.CreateAdd(va, vb, dst);
							else if (op == "sub") res = builder.CreateSub(va, vb, dst);
							else if (op == "mul") res = builder.CreateMul(va, vb, dst);
							else res = builder.CreateSDiv(va, vb, dst);
							vmap[dst] = res; vtypes[dst] = ty;
						}
						else if ((op == "eq" || op == "ne" || op == "lt" || op == "gt" || op == "le" || op == "ge") && il.size() == 5) {
							std::string dst = trimPct(symName(il[1]));
							TypeId opty = tctx_.parse_type(il[2]); (void)opty; // parsed for validation only
							std::string a = trimPct(symName(il[3]));
							std::string b = trimPct(symName(il[4]));
							if (a.empty() || b.empty() || dst.empty()) continue;
							auto* va = vmap.count(a) ? vmap[a] : nullptr;
							auto* vb = vmap.count(b) ? vmap[b] : nullptr;
							if (!va || !vb) continue;
							llvm::CmpInst::Predicate pred;
							if      (op == "eq") pred = llvm::CmpInst::ICMP_EQ;
							else if (op == "ne") pred = llvm::CmpInst::ICMP_NE;
							else if (op == "lt") pred = llvm::CmpInst::ICMP_SLT;
							else if (op == "gt") pred = llvm::CmpInst::ICMP_SGT;
							else if (op == "le") pred = llvm::CmpInst::ICMP_SLE;
							else                 pred = llvm::CmpInst::ICMP_SGE;
							auto* res = builder.CreateICmp(pred, va, vb, dst);
							vmap[dst] = res; vtypes[dst] = tctx_.get_base(BaseType::I1);
						}
						else if ((op == "and" || op == "or" || op == "xor" || op == "shl" || op == "lshr" || op == "ashr") && il.size() == 5) {
							std::string dst = trimPct(symName(il[1]));
							TypeId ty = tctx_.parse_type(il[2]);
							std::string a = trimPct(symName(il[3]));
							std::string b = trimPct(symName(il[4]));
							if (a.empty() || b.empty() || dst.empty()) continue;
							auto* va = vmap.count(a) ? vmap[a] : nullptr;
							auto* vb = vmap.count(b) ? vmap[b] : nullptr;
							if (!va || !vb) continue;
							llvm::Value* res = nullptr;
							if (op == "and") res = builder.CreateAnd(va, vb, dst);
							else if (op == "or") res = builder.CreateOr(va, vb, dst);
							else if (op == "xor") res = builder.CreateXor(va, vb, dst);
							else if (op == "shl") res = builder.CreateShl(va, vb, dst);
							else if (op == "lshr") res = builder.CreateLShr(va, vb, dst);
							else res = builder.CreateAShr(va, vb, dst);
							vmap[dst] = res; vtypes[dst] = ty;
						}
						else if (op == "assign" && il.size() == 3) {
							std::string dst = trimPct(symName(il[1]));
							std::string src = trimPct(symName(il[2]));
							if (dst.empty() || src.empty()) continue;
							if (auto it = vmap.find(src); it != vmap.end()) {
								vmap[dst] = it->second;
								if (vtypes.count(src)) vtypes[dst] = vtypes[src];
							}
						}
						else if (op == "alloca" && il.size() == 3) {
							std::string dst = trimPct(symName(il[1]));
							if (dst.empty()) continue;
							TypeId ty = tctx_.parse_type(il[2]);
							auto* av = builder.CreateAlloca(map_type(ty), nullptr, dst);
							vmap[dst] = av; vtypes[dst] = tctx_.get_pointer(ty);
						}
						else if (op == "store" && il.size() == 4) {
							(void)tctx_.parse_type(il[1]);
							std::string ptrn = trimPct(symName(il[2]));
							std::string valn = trimPct(symName(il[3]));
							if (ptrn.empty() || valn.empty()) continue;
							auto pit = vmap.find(ptrn);
							auto vit = vmap.find(valn);
							if (pit == vmap.end() || vit == vmap.end()) continue;
							builder.CreateStore(vit->second, pit->second);
						}
						else if (op == "gload" && il.size()==4) {
							std::string dst = trimPct(symName(il[1])); if(dst.empty()) continue; TypeId ty = tctx_.parse_type(il[2]); std::string gname = symName(il[3]); if(gname.empty()) continue; auto* gv = module_->getGlobalVariable(gname); if(!gv) continue; llvm::Type* lty = map_type(ty); if(!lty) continue; auto* lv = builder.CreateLoad(lty, gv, dst); if(lty->isIntegerTy()) { /* marker to use lty & ty */ } vmap[dst]=lv; vtypes[dst]=ty;
						}
						else if (op == "gstore" && il.size()==4) {
							(void)tctx_.parse_type(il[1]); std::string gname = symName(il[2]); std::string valn = trimPct(symName(il[3])); if(gname.empty()||valn.empty()) continue; auto* gv = module_->getGlobalVariable(gname); if(!gv) continue; auto vit = vmap.find(valn); if(vit==vmap.end()) continue; builder.CreateStore(vit->second, gv);
						}
						else if (op == "load" && il.size() == 4) {
							std::string dst = trimPct(symName(il[1]));
							if (dst.empty()) continue;
							TypeId ty = tctx_.parse_type(il[2]);
							std::string ptrn = trimPct(symName(il[3]));
							auto it = vmap.find(ptrn);
							if (it == vmap.end()) continue;
							if (!vtypes.count(ptrn)) continue;
							TypeId pty = vtypes[ptrn];
							const Type& PT = tctx_.at(pty);
							if (PT.kind != Type::Kind::Pointer || PT.pointee != ty) continue;
							auto* lv = builder.CreateLoad(map_type(ty), it->second, dst);
							vmap[dst] = lv; vtypes[dst] = ty;
						}
						else if (op == "index" && il.size() == 5) {
							std::string dst = trimPct(symName(il[1]));
							TypeId elemTy = tctx_.parse_type(il[2]);
							std::string base = trimPct(symName(il[3]));
							std::string idx  = trimPct(symName(il[4]));
							if (dst.empty() || base.empty() || idx.empty()) continue;
							auto bit = vmap.find(base);
							auto iit = vmap.find(idx);
							if (bit == vmap.end() || iit == vmap.end()) continue;
							if (!vtypes.count(base) || !vtypes.count(idx)) continue;
							TypeId baseTyId = vtypes[base];
							const Type* baseTy = &tctx_.at(baseTyId);
							if (baseTy->kind != Type::Kind::Pointer) continue;
							const Type* arrTy = &tctx_.at(baseTy->pointee);
							if (arrTy->kind != Type::Kind::Array || arrTy->elem != elemTy) continue;
							llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_), 0);
							auto* gep = builder.CreateInBoundsGEP(map_type(baseTy->pointee), bit->second, { zero, iit->second }, dst);
							vmap[dst] = gep; vtypes[dst] = tctx_.get_pointer(elemTy);
						}
						else if (op == "member" && il.size() == 5) {
							std::string dst   = trimPct(symName(il[1]));
							std::string sname = symName(il[2]);
							std::string base  = trimPct(symName(il[3]));
							std::string fname = symName(il[4]);
							if (dst.empty() || sname.empty() || base.empty() || fname.empty()) continue;
							auto bit = vmap.find(base);
							if (bit == vmap.end() || !vtypes.count(base)) continue;
							TypeId bty = vtypes[base];
							const Type& BT = tctx_.at(bty);
							TypeId structId = 0;
							bool baseIsPtr = false;
							if (BT.kind == Type::Kind::Pointer) {
								baseIsPtr = true;
								if (tctx_.at(BT.pointee).kind == Type::Kind::Struct) structId = BT.pointee;
							} else if (BT.kind == Type::Kind::Struct) {
								structId = bty;
							}
							if (structId == 0) continue;
							const Type& ST = tctx_.at(structId);
							if (ST.kind != Type::Kind::Struct || ST.struct_name != sname) continue;
							if (!baseIsPtr) continue; // only pointer base currently supported
							auto stIt = struct_types_.find(sname);
							if (stIt == struct_types_.end()) continue;
							auto idxMapIt = struct_field_index_.find(sname);
							if (idxMapIt == struct_field_index_.end()) continue;
							auto fIt = idxMapIt->second.find(fname);
							if (fIt == idxMapIt->second.end()) continue;
							size_t fidx = fIt->second;
							auto ftIt = struct_field_types_.find(sname);
							if (ftIt == struct_field_types_.end() || fidx >= ftIt->second.size()) continue;
							llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_), 0);
							llvm::Value* fieldIndex = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_), (uint32_t)fidx);
							auto* gep = builder.CreateInBoundsGEP(stIt->second, bit->second, { zero, fieldIndex }, dst + ".addr");
							llvm::Type* elemLTy = map_type(ftIt->second[fidx]);
							auto* lv = builder.CreateLoad(elemLTy, gep, dst);
							vmap[dst] = lv; vtypes[dst] = ftIt->second[fidx];
						}
						else if (op == "member-addr" && il.size() == 5) {
							// Produce pointer to struct field (no load)
							std::string dst   = trimPct(symName(il[1]));
							std::string sname = symName(il[2]);
							std::string base  = trimPct(symName(il[3]));
							std::string fname = symName(il[4]);
							if (dst.empty() || sname.empty() || base.empty() || fname.empty()) continue;
							auto bit = vmap.find(base);
							if (bit == vmap.end() || !vtypes.count(base)) continue;
							TypeId bty = vtypes[base];
							const Type& BT = tctx_.at(bty);
							TypeId structId = 0; bool baseIsPtr = false;
							if (BT.kind == Type::Kind::Pointer) { baseIsPtr = true; if (tctx_.at(BT.pointee).kind == Type::Kind::Struct) structId = BT.pointee; }
							else if (BT.kind == Type::Kind::Struct) { structId = bty; }
							if (structId == 0) continue;
							const Type& ST = tctx_.at(structId);
							if (ST.kind != Type::Kind::Struct || ST.struct_name != sname) continue;
							if (!baseIsPtr) continue;
							auto stIt = struct_types_.find(sname); if (stIt == struct_types_.end()) continue;
							auto idxMapIt = struct_field_index_.find(sname); if (idxMapIt == struct_field_index_.end()) continue;
							auto fIt = idxMapIt->second.find(fname); if (fIt == idxMapIt->second.end()) continue;
							size_t fidx = fIt->second; auto ftIt = struct_field_types_.find(sname);
							if (ftIt == struct_field_types_.end() || fidx >= ftIt->second.size()) continue;
							llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_), 0);
							llvm::Value* fieldIndex = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_), (uint32_t)fidx);
							auto* gep = builder.CreateInBoundsGEP(stIt->second, bit->second, { zero, fieldIndex }, dst);
							vmap[dst] = gep; vtypes[dst] = tctx_.get_pointer(ftIt->second[fidx]);
						}
						else if (op == "call" && il.size() >= 4) {
							// (call %dst <ret-type> callee %arg1 %arg2 ...)
							std::string dst = trimPct(symName(il[1]));
							if (dst.empty()) continue;
							TypeId retTy = tctx_.parse_type(il[2]);
							std::string callee = symName(il[3]);
							if (callee.empty()) continue;
							llvm::Function* CF = module_->getFunction(callee);
							if (!CF) {
								// create a declaration if missing (assume no params info; derive from operand vars)
								std::vector<TypeId> argTypeIds; std::vector<llvm::Type*> argTypes;
								for (size_t ai = 4; ai < il.size(); ++ai) {
									std::string av = trimPct(symName(il[ai]));
									if (av.empty() || !vtypes.count(av)) { argTypes.clear(); break; }
									argTypeIds.push_back(vtypes[av]); argTypes.push_back(map_type(vtypes[av]));
								}
								auto* fty = llvm::FunctionType::get(map_type(retTy), argTypes, false);
								CF = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, callee, module_.get());
							}
							std::vector<llvm::Value*> argsV;
							for (size_t ai = 4; ai < il.size(); ++ai) {
								std::string av = trimPct(symName(il[ai])); if (av.empty()) continue; auto vit = vmap.find(av); if (vit == vmap.end()) { argsV.clear(); break; } argsV.push_back(vit->second);
							}
							if (argsV.size() + 4 != il.size()) continue; // mismatch -> skip
							auto* callInst = builder.CreateCall(CF, argsV, CF->getReturnType()->isVoidTy() ? "" : dst);
							if (!CF->getReturnType()->isVoidTy()) { vmap[dst] = callInst; vtypes[dst] = retTy; }
						}
						else if (op == "if") {
							if (il.size() >= 3) {
								std::string condSym = trimPct(symName(il[1]));
								auto itc = vmap.find(condSym);
								if (itc == vmap.end()) continue;
								llvm::Value* condV = itc->second;
								auto* thenBB = llvm::BasicBlock::Create(*llctx_, "if.then." + std::to_string(cfCounter++), F);
								llvm::BasicBlock* elseBB = nullptr;
								auto* mergeBB = llvm::BasicBlock::Create(*llctx_, "if.end." + std::to_string(cfCounter++), F);
								bool hasElse = il.size() >= 4 && std::holds_alternative<vector_t>(il[3]->data);
								if (hasElse) elseBB = llvm::BasicBlock::Create(*llctx_, "if.else." + std::to_string(cfCounter++), F);
								if (!builder.GetInsertBlock()->getTerminator())
									builder.CreateCondBr(condV, thenBB, hasElse ? elseBB : mergeBB);
								builder.SetInsertPoint(thenBB);
								if (std::holds_alternative<vector_t>(il[2]->data)) emit_list(std::get<vector_t>(il[2]->data).elems);
								if (!thenBB->getTerminator()) builder.CreateBr(mergeBB);
								if (hasElse) {
									builder.SetInsertPoint(elseBB);
									emit_list(std::get<vector_t>(il[3]->data).elems);
									if (!elseBB->getTerminator()) builder.CreateBr(mergeBB);
								}
								builder.SetInsertPoint(mergeBB);
							}
						}
						else if (op == "while") {
							if (il.size() >= 3 && std::holds_alternative<vector_t>(il[2]->data)) {
								std::string condSym = trimPct(symName(il[1]));
								auto itc = vmap.find(condSym);
								if (itc == vmap.end()) continue;
								auto* condBB = llvm::BasicBlock::Create(*llctx_, "while.cond." + std::to_string(cfCounter++), F);
								auto* bodyBB = llvm::BasicBlock::Create(*llctx_, "while.body." + std::to_string(cfCounter++), F);
								auto* endBB  = llvm::BasicBlock::Create(*llctx_, "while.end."  + std::to_string(cfCounter++), F);
								if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(condBB);
								builder.SetInsertPoint(condBB);
								llvm::Value* condV = itc->second;
								builder.CreateCondBr(condV, bodyBB, endBB);
								builder.SetInsertPoint(bodyBB);
								loopEndStack.push_back(endBB);
								emit_list(std::get<vector_t>(il[2]->data).elems);
								loopEndStack.pop_back();
								if (!bodyBB->getTerminator()) builder.CreateBr(condBB);
								builder.SetInsertPoint(endBB);
							}
						}
						else if (op == "break") {
							if (!loopEndStack.empty()) {
								if (!builder.GetInsertBlock()->getTerminator())
									builder.CreateBr(loopEndStack.back());
							}
							return false; // terminate current list (loop body)
						}
						else if (op == "ret" && il.size() == 3) {
							std::string v = trimPct(symName(il[2]));
							if (!v.empty() && vmap.count(v)) builder.CreateRet(vmap[v]);
							else if (fty->getReturnType()->isVoidTy()) builder.CreateRetVoid();
							else builder.CreateRet(llvm::Constant::getNullValue(fty->getReturnType()));
							return false; // function terminated
						}
					}
					return true;
				};

				emit_list(body_vec->elems);
			}

			if (!entry->getTerminator()) {
				if (fty->getReturnType()->isVoidTy()) builder.CreateRetVoid();
				else builder.CreateRet(llvm::Constant::getNullValue(fty->getReturnType()));
			}
		}
	}

	if (std::getenv("EDN_TRACE_IR")) {
		std::cout << "[emit] final module functions: ";
		for (auto& F : module_->functions()) std::cout << F.getName().str() << ' ';
		std::cout << "\n";
	}
	return module_.get();
}

llvm::orc::ThreadSafeModule IREmitter::toThreadSafeModule() {
	return llvm::orc::ThreadSafeModule(std::move(module_), std::move(llctx_));
}

} // namespace edn
