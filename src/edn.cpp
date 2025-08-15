// edn.cpp - Clean IR emitter implementation (fully rewritten after corruption)
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/diagnostics_json.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <cstdlib>

namespace edn {

static std::string symName(const node_ptr& n){ if(!n) return {}; if(std::holds_alternative<symbol>(n->data)) return std::get<symbol>(n->data).name; if(std::holds_alternative<std::string>(n->data)) return std::get<std::string>(n->data); return {}; }
static std::string trimPct(const std::string& s){ return (!s.empty() && s[0]=='%') ? s.substr(1) : s; }

IREmitter::IREmitter(TypeContext& tctx): tctx_(tctx){ llctx_ = std::make_unique<llvm::LLVMContext>(); }
IREmitter::~IREmitter() = default;

llvm::Type* IREmitter::map_type(TypeId id){ const Type& T=tctx_.at(id); switch(T.kind){
	case Type::Kind::Base: switch(T.base){
		case BaseType::I1: return llvm::Type::getInt1Ty(*llctx_);
		case BaseType::I8: return llvm::Type::getInt8Ty(*llctx_);
		case BaseType::I16: return llvm::Type::getInt16Ty(*llctx_);
		case BaseType::I32: return llvm::Type::getInt32Ty(*llctx_);
		case BaseType::I64: return llvm::Type::getInt64Ty(*llctx_);
		case BaseType::U8: return llvm::Type::getInt8Ty(*llctx_);
		case BaseType::U16: return llvm::Type::getInt16Ty(*llctx_);
		case BaseType::U32: return llvm::Type::getInt32Ty(*llctx_);
		case BaseType::U64: return llvm::Type::getInt64Ty(*llctx_);
		case BaseType::F32: return llvm::Type::getFloatTy(*llctx_);
		case BaseType::F64: return llvm::Type::getDoubleTy(*llctx_);
		case BaseType::Void: return llvm::Type::getVoidTy(*llctx_); }
	case Type::Kind::Pointer: return llvm::PointerType::getUnqual(map_type(T.pointee));
	case Type::Kind::Struct: { if(auto existing=llvm::StructType::getTypeByName(*llctx_,"struct."+T.struct_name)) return existing; return llvm::StructType::create(*llctx_,"struct."+T.struct_name); }
	case Type::Kind::Function: { std::vector<llvm::Type*> ps; ps.reserve(T.params.size()); for(auto p:T.params) ps.push_back(map_type(p)); return llvm::FunctionType::get(map_type(T.ret), ps, T.variadic); }
	case Type::Kind::Array: return llvm::ArrayType::get(map_type(T.elem),(uint64_t)T.array_size);
 }
 return llvm::Type::getVoidTy(*llctx_);
}

llvm::StructType* IREmitter::get_or_create_struct(const std::string& name,const std::vector<TypeId>& field_types){ if(auto it=struct_types_.find(name); it!=struct_types_.end()) return it->second; auto* ST=llvm::StructType::getTypeByName(*llctx_,"struct."+name); if(!ST) ST=llvm::StructType::create(*llctx_,"struct."+name); std::vector<llvm::Type*> elems; elems.reserve(field_types.size()); for(auto ft:field_types) elems.push_back(map_type(ft)); if(ST->isOpaque()) ST->setBody(elems,false); struct_types_[name]=ST; return ST; }

llvm::Module* IREmitter::emit(const node_ptr& module_ast, TypeCheckResult& tc_result){
	TypeChecker checker(tctx_); tc_result = checker.check_module(module_ast);
	// Optional JSON diagnostics output (set EDN_DIAG_JSON=1)
	extern void maybe_print_json(const TypeCheckResult&); // forward (header-only impl)
	maybe_print_json(tc_result);
	if(!tc_result.success) return nullptr;
	module_ = std::make_unique<llvm::Module>("edn.module", *llctx_);
	if(!module_ast || !std::holds_alternative<list>(module_ast->data)) return nullptr; auto &top = std::get<list>(module_ast->data).elems; if(top.empty()) return nullptr;

	auto collect_structs = [&](const std::vector<node_ptr>& elems){ for(auto &n: elems){ if(!n||!std::holds_alternative<list>(n->data)) continue; auto &l=std::get<list>(n->data).elems; if(l.empty()) continue; if(!std::holds_alternative<symbol>(l[0]->data)|| std::get<symbol>(l[0]->data).name!="struct") continue; std::string sname; std::vector<TypeId> ftypes; std::vector<std::string> fnames; for(size_t i=1;i<l.size();++i){ if(!std::holds_alternative<keyword>(l[i]->data)) continue; std::string kw=std::get<keyword>(l[i]->data).name; if(++i>=l.size()) break; auto val=l[i]; if(kw=="name") sname=symName(val); else if(kw=="fields" && std::holds_alternative<vector_t>(val->data)){ for(auto &f: std::get<vector_t>(val->data).elems){ if(!f||!std::holds_alternative<list>(f->data)) continue; auto &fl=std::get<list>(f->data).elems; std::string fname; TypeId fty=0; for(size_t k=0;k<fl.size(); ++k){ if(!std::holds_alternative<keyword>(fl[k]->data)) continue; std::string fkw=std::get<keyword>(fl[k]->data).name; if(++k>=fl.size()) break; auto v=fl[k]; if(fkw=="name") fname=symName(v); else if(fkw=="type") try{ fty=tctx_.parse_type(v);}catch(...){} } if(!fname.empty()&&fty){ fnames.push_back(fname); ftypes.push_back(fty);} } } } if(!sname.empty()&& !ftypes.empty()){ get_or_create_struct(sname,ftypes); struct_field_types_[sname]=ftypes; auto &m=struct_field_index_[sname]; for(size_t ix=0; ix<fnames.size(); ++ix) m[fnames[ix]]=ix; } } };
	// Represent unions as single-field struct of byte array big enough to hold largest field (simplified); for loads we bitcast
	auto collect_unions = [&](const std::vector<node_ptr>& elems){ for(auto &n: elems){ if(!n||!std::holds_alternative<list>(n->data)) continue; auto &l=std::get<list>(n->data).elems; if(l.empty()) continue; if(!std::holds_alternative<symbol>(l[0]->data) || std::get<symbol>(l[0]->data).name!="union") continue; std::string uname; std::vector<std::pair<std::string,TypeId>> fields; for(size_t i=1;i<l.size(); ++i){ if(!std::holds_alternative<keyword>(l[i]->data)) continue; std::string kw=std::get<keyword>(l[i]->data).name; if(++i>=l.size()) break; auto val=l[i]; if(kw=="name") uname=symName(val); else if(kw=="fields" && std::holds_alternative<vector_t>(val->data)){ for(auto &f: std::get<vector_t>(val->data).elems){ if(!f||!std::holds_alternative<list>(f->data)) continue; auto &fl=std::get<list>(f->data).elems; if(fl.empty()||!std::holds_alternative<symbol>(fl[0]->data) || std::get<symbol>(fl[0]->data).name!="ufield") continue; std::string fname; TypeId fty=0; for(size_t k=1;k<fl.size(); ++k){ if(!std::holds_alternative<keyword>(fl[k]->data)) break; std::string fkw=std::get<keyword>(fl[k]->data).name; if(++k>=fl.size()) break; auto v=fl[k]; if(fkw=="name") fname=symName(v); else if(fkw=="type") try{ fty=tctx_.parse_type(v);}catch(...){} } if(!fname.empty()&&fty) fields.emplace_back(fname,fty); } }
		}
		if(uname.empty()||fields.empty()) continue; // size calc
		uint64_t maxSize=0; std::unordered_map<std::string,TypeId> ftypes; for(auto &p: fields){ llvm::Type* llT=map_type(p.second); uint64_t sz = module_->getDataLayout().getTypeAllocSize(llT); if(sz>maxSize) maxSize=sz; ftypes[p.first]=p.second; }
		// create [maxSize x i8] array wrapper struct: { [N x i8] }
		llvm::ArrayType* storageArr = llvm::ArrayType::get(llvm::Type::getInt8Ty(*llctx_), maxSize?maxSize:1);
		auto *ST=llvm::StructType::create(*llctx_, {storageArr}, "struct."+uname); (void)ST; // suppress unused warning (layout captured via name)
		struct_field_types_[uname] = { tctx_.get_array(tctx_.get_base(BaseType::I8), maxSize?maxSize:1) };
		struct_field_index_[uname]["__storage"] = 0; // pseudo field
		union_field_types_[uname] = ftypes; // remember logical fields
	} };
	auto emit_globals = [&](const std::vector<node_ptr>& elems){ for(auto &n: elems){ if(!n||!std::holds_alternative<list>(n->data)) continue; auto &l=std::get<list>(n->data).elems; if(l.empty()) continue; if(!std::holds_alternative<symbol>(l[0]->data)||std::get<symbol>(l[0]->data).name!="global") continue; std::string gname; TypeId gty=0; node_ptr init; bool isConst=false; for(size_t i=1;i<l.size(); ++i){ if(!std::holds_alternative<keyword>(l[i]->data)) continue; std::string kw=std::get<keyword>(l[i]->data).name; if(++i>=l.size()) break; auto v=l[i]; if(kw=="name") gname=symName(v); else if(kw=="type") try{ gty=tctx_.parse_type(v);}catch(...){} else if(kw=="init") init=v; else if(kw=="const" && std::holds_alternative<bool>(v->data)) isConst=std::get<bool>(v->data); }
		if(gname.empty()||!gty) continue; llvm::Type* lty=map_type(gty); llvm::Constant* c=nullptr; if(init){
			// scalar init
			if(std::holds_alternative<int64_t>(init->data)) c=llvm::ConstantInt::get(lty,(uint64_t)std::get<int64_t>(init->data),true);
			else if(std::holds_alternative<double>(init->data)) c=llvm::ConstantFP::get(lty,std::get<double>(init->data));
			// aggregate vector init for arrays or structs
			else if(std::holds_alternative<vector_t>(init->data)){
				const Type& T = tctx_.at(gty);
				if(T.kind==Type::Kind::Array){ auto &elemsV = std::get<vector_t>(init->data).elems; if(elemsV.size()==T.array_size){ std::vector<llvm::Constant*> elemsC; elemsC.reserve(elemsV.size()); llvm::Type* eltTy = map_type(T.elem); bool ok=true; for(auto &e: elemsV){ if(!e){ ok=false; break;} if(std::holds_alternative<int64_t>(e->data)) elemsC.push_back(llvm::ConstantInt::get(eltTy,(uint64_t)std::get<int64_t>(e->data),true)); else if(std::holds_alternative<double>(e->data)) elemsC.push_back(llvm::ConstantFP::get(eltTy,std::get<double>(e->data))); else { ok=false; break;} } if(ok) c=llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(lty), elemsC); }
				} else if(T.kind==Type::Kind::Struct){ auto &elemsV = std::get<vector_t>(init->data).elems; // expect one literal per base field in declared order
					// Reconstruct struct field types from previously registered struct_types_ info if possible
					// We rely on struct_field_types_ captured during earlier pass (collect_structs).
					auto ftIt = struct_field_types_.find(T.struct_name);
					if(ftIt!=struct_field_types_.end() && elemsV.size()==ftIt->second.size()){
						std::vector<llvm::Constant*> fieldConsts; fieldConsts.reserve(elemsV.size()); bool ok=true; for(size_t fi=0; fi<elemsV.size(); ++fi){ auto &e=elemsV[fi]; if(!e){ ok=false; break; } const Type& FT=tctx_.at(ftIt->second[fi]); if(FT.kind!=Type::Kind::Base){ ok=false; break; } llvm::Type* flty=map_type(ftIt->second[fi]); if(std::holds_alternative<int64_t>(e->data) && is_integer_base(FT.base)) fieldConsts.push_back(llvm::ConstantInt::get(flty,(uint64_t)std::get<int64_t>(e->data),true)); else if(std::holds_alternative<double>(e->data) && is_float_base(FT.base)) fieldConsts.push_back(llvm::ConstantFP::get(flty,std::get<double>(e->data))); else if(std::holds_alternative<int64_t>(e->data) && is_float_base(FT.base)) fieldConsts.push_back(llvm::ConstantFP::get(flty,(double)std::get<int64_t>(e->data))); else { ok=false; break; } }
						if(ok){ auto *ST = llvm::StructType::getTypeByName(*llctx_, "struct."+T.struct_name); if(!ST) { std::vector<llvm::Type*> lt; for(auto tid: ftIt->second) lt.push_back(map_type(tid)); ST=llvm::StructType::create(*llctx_, lt, "struct."+T.struct_name); } c=llvm::ConstantStruct::get(ST, fieldConsts); }
					}
				}
			}
		}
		if(!c) c=llvm::Constant::getNullValue(lty); auto *gv = new llvm::GlobalVariable(*module_, lty, isConst, llvm::GlobalValue::ExternalLinkage, c, gname); (void)gv;
	} };
	collect_structs(top); collect_unions(top); emit_globals(top);

	for(size_t i=1;i<top.size(); ++i){ auto fn=top[i]; if(!fn||!std::holds_alternative<list>(fn->data)) continue; auto &fl=std::get<list>(fn->data).elems; if(fl.empty()) continue; if(!std::holds_alternative<symbol>(fl[0]->data)|| std::get<symbol>(fl[0]->data).name!="fn") continue; std::string fname; TypeId retTy=tctx_.get_base(BaseType::Void); std::vector<std::pair<std::string,TypeId>> params; node_ptr body; for(size_t j=1;j<fl.size(); ++j){ if(!std::holds_alternative<keyword>(fl[j]->data)) continue; std::string kw=std::get<keyword>(fl[j]->data).name; if(++j>=fl.size()) break; auto val=fl[j]; if(kw=="name") fname=symName(val); else if(kw=="ret") try{ retTy=tctx_.parse_type(val);}catch(...){} else if(kw=="params" && std::holds_alternative<vector_t>(val->data)){ for(auto &p: std::get<vector_t>(val->data).elems){ if(!p||!std::holds_alternative<list>(p->data)) continue; auto &pl=std::get<list>(p->data).elems; if(pl.size()!=3) continue; if(!std::holds_alternative<symbol>(pl[0]->data)|| std::get<symbol>(pl[0]->data).name!="param") continue; try{ TypeId pty=tctx_.parse_type(pl[1]); std::string pname=trimPct(symName(pl[2])); if(!pname.empty()) params.emplace_back(pname,pty);}catch(...){} } } else if(kw=="body" && std::holds_alternative<vector_t>(val->data)) body=val; }
		if(fname.empty()||!body) continue; std::vector<TypeId> paramIds; for(auto &pr: params) paramIds.push_back(pr.second);
		// Determine variadic flag by re-parsing :vararg from function list (cheap scan)
		bool isVariadic=false; for(size_t j=1;j<fl.size(); ++j){ if(fl[j] && std::holds_alternative<keyword>(fl[j]->data) && std::get<keyword>(fl[j]->data).name=="vararg"){ if(j+1<fl.size() && std::holds_alternative<bool>(fl[j+1]->data)) isVariadic=std::get<bool>(fl[j+1]->data); break; } }
		auto ftyId=tctx_.get_function(paramIds, retTy, isVariadic); auto *fty=llvm::cast<llvm::FunctionType>(map_type(ftyId)); auto *F=llvm::Function::Create(fty, llvm::Function::ExternalLinkage, fname, module_.get()); size_t ai=0; for(auto &arg: F->args()) arg.setName(params[ai++].first); auto *entry=llvm::BasicBlock::Create(*llctx_,"entry",F); llvm::IRBuilder<> builder(entry); std::unordered_map<std::string,llvm::Value*> vmap; std::unordered_map<std::string,TypeId> vtypes; for(auto &pr: params){ vtypes[pr.first]=pr.second; } for(auto &arg: F->args()){ vmap[std::string(arg.getName())]=&arg; }
		std::vector<llvm::BasicBlock*> loopEndStack; // for break targets (end blocks)
		std::vector<llvm::BasicBlock*> loopContinueStack; // for continue targets (condition re-check or step blocks)
		int cfCounter=0; bool functionDone=false;
		// Defer phi creation until predecessors exist: collect specs in current block then create at end of block emission phase.
		struct PendingPhi { std::string dst; TypeId ty; std::vector<std::pair<std::string,std::string>> incomings; llvm::BasicBlock* insertBlock; };
		std::vector<PendingPhi> pendingPhis;
		auto emit_list = [&](const std::vector<node_ptr>& insts, auto&& emit_ref) -> void {
			if(functionDone) return; for(auto &inst: insts){ if(functionDone) break; if(!inst||!std::holds_alternative<list>(inst->data)) continue; auto &il=std::get<list>(inst->data).elems; if(il.empty()) continue; if(!std::holds_alternative<symbol>(il[0]->data)) continue; std::string op=std::get<symbol>(il[0]->data).name;
				auto getVal=[&](const node_ptr& n)->llvm::Value*{ std::string nm=trimPct(symName(n)); if(nm.empty()) return nullptr; auto it=vmap.find(nm); return (it!=vmap.end())?it->second:nullptr; };
				if(op=="const" && il.size()==4){ std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId ty; try{ ty=tctx_.parse_type(il[2]); }catch(...){ continue; } llvm::Type* lty=map_type(ty); llvm::Value* cv=nullptr; if(std::holds_alternative<int64_t>(il[3]->data)) cv=llvm::ConstantInt::get(lty,(uint64_t)std::get<int64_t>(il[3]->data),true); else if(std::holds_alternative<double>(il[3]->data)) cv=llvm::ConstantFP::get(lty,std::get<double>(il[3]->data)); if(!cv) cv=llvm::UndefValue::get(lty); vmap[dst]=cv; vtypes[dst]=ty; }
				else if((op=="add"||op=="sub"||op=="mul"||op=="sdiv"||op=="udiv"||op=="srem"||op=="urem") && il.size()==5){ std::string dst=trimPct(symName(il[1])); TypeId ty; try{ ty=tctx_.parse_type(il[2]); }catch(...){ continue; } auto *va=getVal(il[3]); auto *vb=getVal(il[4]); if(!va||!vb||dst.empty()) continue; llvm::Value* r=nullptr; if(op=="add") r=builder.CreateAdd(va,vb,dst); else if(op=="sub") r=builder.CreateSub(va,vb,dst); else if(op=="mul") r=builder.CreateMul(va,vb,dst); else if(op=="sdiv") r=builder.CreateSDiv(va,vb,dst); else if(op=="udiv") r=builder.CreateUDiv(va,vb,dst); else if(op=="srem") r=builder.CreateSRem(va,vb,dst); else r=builder.CreateURem(va,vb,dst); vmap[dst]=r; vtypes[dst]=ty; }
				else if((op=="ptr-add"||op=="ptr-sub") && il.size()==5){ // (ptr-add %dst (ptr <T>) %base %offset)
					std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId annot; try{ annot=tctx_.parse_type(il[2]); }catch(...){ continue; }
					std::string baseName=trimPct(symName(il[3])); std::string offName=trimPct(symName(il[4])); if(baseName.empty()||offName.empty()) continue; auto bit=vmap.find(baseName); auto oit=vmap.find(offName); if(bit==vmap.end()||oit==vmap.end()) continue; if(!vtypes.count(baseName)||!vtypes.count(offName)) continue; TypeId bty=vtypes[baseName]; const Type& BT=tctx_.at(bty); const Type& AT=tctx_.at(annot); if(BT.kind!=Type::Kind::Pointer||AT.kind!=Type::Kind::Pointer||BT.pointee!=AT.pointee) continue; TypeId oty=vtypes[offName]; const Type& OT=tctx_.at(oty); if(!(OT.kind==Type::Kind::Base && is_integer_base(OT.base))) continue; llvm::Value* offsetVal=oit->second; if(op=="ptr-sub"){ // subtract integer -> negate offset
						offsetVal = builder.CreateNeg(offsetVal, offName+".neg"); }
					// Use GEP with element count (LLVM scales automatically by element size)
					llvm::Value* gep = builder.CreateGEP(map_type(AT.pointee), bit->second, offsetVal, dst);
					vmap[dst]=gep; vtypes[dst]=annot; }
				else if(op=="ptr-diff" && il.size()==5){ // (ptr-diff %dst <int-type> %a %b)  result = (#elements difference)
					std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId rty; try{ rty=tctx_.parse_type(il[2]); }catch(...){ continue; }
					std::string aName=trimPct(symName(il[3])); std::string bName=trimPct(symName(il[4])); if(aName.empty()||bName.empty()) continue; if(!vtypes.count(aName)||!vtypes.count(bName)) continue; const Type& AT=tctx_.at(vtypes[aName]); const Type& BT2=tctx_.at(vtypes[bName]); if(AT.kind!=Type::Kind::Pointer||BT2.kind!=Type::Kind::Pointer||AT.pointee!=BT2.pointee) continue; auto *aV=vmap[aName]; auto *bV=vmap[bName]; // cast to int, subtract, divide by sizeof(elem)
					llvm::Type* intPtrTy = llvm::Type::getInt64Ty(*llctx_); // assume 64-bit for diff scaling (simplification)
					auto *aInt = builder.CreatePtrToInt(aV, intPtrTy, aName+".pi");
					auto *bInt = builder.CreatePtrToInt(bV, intPtrTy, bName+".pi");
					auto *rawDiff = builder.CreateSub(aInt,bInt,dst+".raw");
					// size of element
					llvm::Type* elemLL = map_type(AT.pointee);
					uint64_t elemSize = module_->getDataLayout().getTypeAllocSize(elemLL);
					llvm::Value* scale = llvm::ConstantInt::get(intPtrTy, elemSize);
					llvm::Value* elemCount = builder.CreateSDiv(rawDiff, scale, dst+".elts");
					// cast to requested integer type if sizes differ
					llvm::Type* destTy = map_type(rty); llvm::Value* finalV = elemCount;
					if(destTy!=elemCount->getType()){
						unsigned fromBits = elemCount->getType()->getIntegerBitWidth(); unsigned toBits = destTy->getIntegerBitWidth();
						if(toBits>fromBits) finalV = builder.CreateSExt(elemCount,destTy,dst+".ext"); else if(toBits<fromBits) finalV = builder.CreateTrunc(elemCount,destTy,dst+".trunc");
					}
					vmap[dst]=finalV; vtypes[dst]=rty; }
				else if(op=="addr" && il.size()==4){ // (addr %dst (ptr <T>) %src)
					std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId annot; try{ annot=tctx_.parse_type(il[2]); }catch(...){ continue; }
					std::string srcName=trimPct(symName(il[3])); if(srcName.empty()) continue; if(!vtypes.count(srcName)) continue; auto *valV=vmap[srcName]; if(!valV) continue; const Type& AT=tctx_.at(annot); if(AT.kind!=Type::Kind::Pointer) continue; // allocate slot if first time taking address
					// Heuristic: create (or reuse) a shadow alloca for the source by naming convention
					std::string slotName = srcName+".addr.slot"; llvm::AllocaInst* slot=nullptr; // search existing mapping
					if(vmap.count(slotName)) slot=llvm::dyn_cast<llvm::AllocaInst>(vmap[slotName]);
					if(!slot){ slot=builder.CreateAlloca(map_type(AT.pointee), nullptr, slotName); vmap[slotName]=slot; vtypes[slotName]=tctx_.get_pointer(AT.pointee); builder.CreateStore(valV, slot); }
					vmap[dst]=slot; vtypes[dst]=annot; }
				else if(op=="deref" && il.size()==4){ // (deref %dst <T> %ptr)
					std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId ty; try{ ty=tctx_.parse_type(il[2]); }catch(...){ continue; }
					std::string ptrName=trimPct(symName(il[3])); if(ptrName.empty()) continue; if(!vtypes.count(ptrName)) continue; TypeId pty=vtypes[ptrName]; const Type& PT=tctx_.at(pty); if(PT.kind!=Type::Kind::Pointer||PT.pointee!=ty) continue; auto *pv=vmap[ptrName]; if(!pv) continue; auto *lv=builder.CreateLoad(map_type(ty), pv, dst); vmap[dst]=lv; vtypes[dst]=ty; }
				else if(op=="fnptr" && il.size()==4){ // (fnptr %dst (ptr (fn-type ...)) Name)
					std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId pty; try{ pty=tctx_.parse_type(il[2]); }catch(...){ continue; } std::string fname=symName(il[3]); if(fname.empty()) continue; auto *F=module_->getFunction(fname); if(!F){ // create decl with matching signature if we can
						const Type& PT=tctx_.at(pty); if(PT.kind!=Type::Kind::Pointer) continue; const Type& FT=tctx_.at(PT.pointee); if(FT.kind!=Type::Kind::Function) continue; std::vector<llvm::Type*> ps; for(auto pid: FT.params) ps.push_back(map_type(pid)); auto *ftyDecl=llvm::FunctionType::get(map_type(FT.ret), ps, FT.variadic); F=llvm::Function::Create(ftyDecl, llvm::Function::ExternalLinkage, fname, module_.get()); }
					if(!F) continue; // take address of function is just function pointer value in LLVM IR
					vmap[dst]=F; vtypes[dst]=pty; }
				else if(op=="call-indirect" && il.size()>=4){ // (call-indirect %dst <ret> %fptr %args...)
					std::string dst=trimPct(symName(il[1])); TypeId retTy; try{ retTy=tctx_.parse_type(il[2]); }catch(...){ continue; } std::string fptrName=trimPct(symName(il[3])); if(fptrName.empty()) continue; if(!vtypes.count(fptrName)) continue; TypeId fpty=vtypes[fptrName]; const Type& FPT=tctx_.at(fpty); if(FPT.kind!=Type::Kind::Pointer) continue; const Type& FT=tctx_.at(FPT.pointee); if(FT.kind!=Type::Kind::Function) continue; auto *calleeV=vmap[fptrName]; if(!calleeV) continue; std::vector<llvm::Value*> args; bool bad=false; for(size_t ai=4; ai<il.size(); ++ai){ std::string an=trimPct(symName(il[ai])); if(an.empty()||!vmap.count(an)){ bad=true; break;} args.push_back(vmap[an]); }
					if(bad) continue; llvm::FunctionType* fty = llvm::cast<llvm::FunctionType>(map_type(FPT.pointee)); auto *ci=builder.CreateCall(fty, calleeV, args, fty->getReturnType()->isVoidTy()?"":dst); if(!fty->getReturnType()->isVoidTy()){ vmap[dst]=ci; vtypes[dst]=retTy; }
				}
				else if((op=="fadd"||op=="fsub"||op=="fmul"||op=="fdiv") && il.size()==5){ std::string dst=trimPct(symName(il[1])); TypeId ty; try{ ty=tctx_.parse_type(il[2]); }catch(...){ continue; } auto *va=getVal(il[3]); auto *vb=getVal(il[4]); if(!va||!vb||dst.empty()) continue; llvm::Value* r=nullptr; if(op=="fadd") r=builder.CreateFAdd(va,vb,dst); else if(op=="fsub") r=builder.CreateFSub(va,vb,dst); else if(op=="fmul") r=builder.CreateFMul(va,vb,dst); else r=builder.CreateFDiv(va,vb,dst); vmap[dst]=r; vtypes[dst]=ty; }
				else if(op=="fcmp" && il.size()==7){
					std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; if(!std::holds_alternative<keyword>(il[3]->data)) continue; std::string pred=symName(il[4]); auto *va=getVal(il[5]); auto *vb=getVal(il[6]); if(!va||!vb) continue; llvm::CmpInst::Predicate P=llvm::CmpInst::FCMP_OEQ;
					if(pred=="oeq") P=llvm::CmpInst::FCMP_OEQ; else if(pred=="one") P=llvm::CmpInst::FCMP_ONE; else if(pred=="olt") P=llvm::CmpInst::FCMP_OLT; else if(pred=="ogt") P=llvm::CmpInst::FCMP_OGT; else if(pred=="ole") P=llvm::CmpInst::FCMP_OLE; else if(pred=="oge") P=llvm::CmpInst::FCMP_OGE; else if(pred=="ord") P=llvm::CmpInst::FCMP_ORD; else if(pred=="uno") P=llvm::CmpInst::FCMP_UNO; else if(pred=="ueq") P=llvm::CmpInst::FCMP_UEQ; else if(pred=="une") P=llvm::CmpInst::FCMP_UNE; else if(pred=="ult") P=llvm::CmpInst::FCMP_ULT; else if(pred=="ugt") P=llvm::CmpInst::FCMP_UGT; else if(pred=="ule") P=llvm::CmpInst::FCMP_ULE; else if(pred=="uge") P=llvm::CmpInst::FCMP_UGE; else continue;
					auto* res=builder.CreateFCmp(P,va,vb,dst); vmap[dst]=res; vtypes[dst]=tctx_.get_base(BaseType::I1);
				}
				else if((op=="eq"||op=="ne"||op=="lt"||op=="gt"||op=="le"||op=="ge") && il.size()==5){ std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; auto *va=getVal(il[3]); auto *vb=getVal(il[4]); if(!va||!vb) continue; llvm::CmpInst::Predicate P=llvm::CmpInst::ICMP_EQ; if(op=="eq") P=llvm::CmpInst::ICMP_EQ; else if(op=="ne") P=llvm::CmpInst::ICMP_NE; else if(op=="lt") P=llvm::CmpInst::ICMP_SLT; else if(op=="gt") P=llvm::CmpInst::ICMP_SGT; else if(op=="le") P=llvm::CmpInst::ICMP_SLE; else P=llvm::CmpInst::ICMP_SGE; auto* res=builder.CreateICmp(P,va,vb,dst); vmap[dst]=res; vtypes[dst]=tctx_.get_base(BaseType::I1); }
				else if(op=="icmp" && il.size()==7){ std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; if(!std::holds_alternative<keyword>(il[3]->data)) continue; std::string pred=symName(il[4]); auto *va=getVal(il[5]); auto *vb=getVal(il[6]); if(!va||!vb) continue; llvm::CmpInst::Predicate P=llvm::CmpInst::ICMP_EQ; if(pred=="eq") P=llvm::CmpInst::ICMP_EQ; else if(pred=="ne") P=llvm::CmpInst::ICMP_NE; else if(pred=="slt") P=llvm::CmpInst::ICMP_SLT; else if(pred=="sgt") P=llvm::CmpInst::ICMP_SGT; else if(pred=="sle") P=llvm::CmpInst::ICMP_SLE; else if(pred=="sge") P=llvm::CmpInst::ICMP_SGE; else if(pred=="ult") P=llvm::CmpInst::ICMP_ULT; else if(pred=="ugt") P=llvm::CmpInst::ICMP_UGT; else if(pred=="ule") P=llvm::CmpInst::ICMP_ULE; else if(pred=="uge") P=llvm::CmpInst::ICMP_UGE; else continue; auto* res=builder.CreateICmp(P,va,vb,dst); vmap[dst]=res; vtypes[dst]=tctx_.get_base(BaseType::I1); }
				else if((op=="and"||op=="or"||op=="xor"||op=="shl"||op=="lshr"||op=="ashr") && il.size()==5){ std::string dst=trimPct(symName(il[1])); TypeId ty; try{ ty=tctx_.parse_type(il[2]); }catch(...){ continue; } auto *va=getVal(il[3]); auto *vb=getVal(il[4]); if(!va||!vb||dst.empty()) continue; llvm::Value* r=nullptr; if(op=="and") r=builder.CreateAnd(va,vb,dst); else if(op=="or") r=builder.CreateOr(va,vb,dst); else if(op=="xor") r=builder.CreateXor(va,vb,dst); else if(op=="shl") r=builder.CreateShl(va,vb,dst); else if(op=="lshr") r=builder.CreateLShr(va,vb,dst); else r=builder.CreateAShr(va,vb,dst); vmap[dst]=r; vtypes[dst]=ty; }
				else if(op=="as" && il.size()==4){ // (as %dst <to-type> %src)
					std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId toTy; try{ toTy=tctx_.parse_type(il[2]); }catch(...){ continue; }
					std::string src=trimPct(symName(il[3])); if(src.empty()||!vmap.count(src)||!vtypes.count(src)) continue; TypeId fromTy=vtypes[src]; const Type& FROM=tctx_.at(fromTy); const Type& TO=tctx_.at(toTy);
					auto isInt=[&](const Type&t){ return t.kind==Type::Kind::Base && is_integer_base(t.base); };
					auto intWidth=[&](const Type&t){ switch(t.base){ case BaseType::I1: return 1; case BaseType::I8: case BaseType::U8: return 8; case BaseType::I16: case BaseType::U16: return 16; case BaseType::I32: case BaseType::U32: return 32; case BaseType::I64: case BaseType::U64: return 64; default: return 0; } };
					std::string chosen; if(isInt(FROM) && isInt(TO)){ int fw=intWidth(FROM), tw=intWidth(TO); if(fw==tw) chosen="bitcast"; else if(fw<tw) chosen=is_signed_base(FROM.base)?"sext":"zext"; else chosen="trunc"; } else if(isInt(FROM) && TO.kind==Type::Kind::Base && (TO.base==BaseType::F32||TO.base==BaseType::F64)) chosen=is_signed_base(FROM.base)?"sitofp":"uitofp"; else if(FROM.kind==Type::Kind::Base && (FROM.base==BaseType::F32||FROM.base==BaseType::F64) && isInt(TO)) chosen=is_signed_base(TO.base)?"fptosi":"fptoui"; else if(FROM.kind==Type::Kind::Pointer && isInt(TO)) chosen="ptrtoint"; else if(isInt(FROM) && TO.kind==Type::Kind::Pointer) chosen="inttoptr"; else if(FROM.kind==Type::Kind::Pointer && TO.kind==Type::Kind::Pointer) chosen="bitcast"; else if(FROM.kind==Type::Kind::Base && (FROM.base==BaseType::F32||FROM.base==BaseType::F64) && TO.kind==Type::Kind::Base && (TO.base==BaseType::F32||TO.base==BaseType::F64) && FROM.base==TO.base) chosen="bitcast";
					if(chosen.empty()) continue; llvm::Value* srcV=vmap[src]; llvm::Value* result=nullptr; llvm::Type* llvmTo=map_type(toTy);
					if(chosen=="zext") result=builder.CreateZExt(srcV, llvmTo, dst); else if(chosen=="sext") result=builder.CreateSExt(srcV, llvmTo, dst); else if(chosen=="trunc") result=builder.CreateTrunc(srcV, llvmTo, dst); else if(chosen=="bitcast") result=builder.CreateBitCast(srcV, llvmTo, dst); else if(chosen=="sitofp") result=builder.CreateSIToFP(srcV, llvmTo, dst); else if(chosen=="uitofp") result=builder.CreateUIToFP(srcV, llvmTo, dst); else if(chosen=="fptosi") result=builder.CreateFPToSI(srcV, llvmTo, dst); else if(chosen=="fptoui") result=builder.CreateFPToUI(srcV, llvmTo, dst); else if(chosen=="ptrtoint") result=builder.CreatePtrToInt(srcV, llvmTo, dst); else if(chosen=="inttoptr") result=builder.CreateIntToPtr(srcV, llvmTo, dst); if(result){ vmap[dst]=result; vtypes[dst]=toTy; }
				}
				else if((op=="zext"||op=="sext"||op=="trunc"||op=="bitcast"||op=="sitofp"||op=="uitofp"||op=="fptosi"||op=="fptoui"||op=="ptrtoint"||op=="inttoptr") && il.size()==4){
					std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId toTy; try{ toTy=tctx_.parse_type(il[2]); }catch(...){ continue; } auto *srcV=getVal(il[3]); if(!srcV) continue; llvm::Value* castV=nullptr; llvm::Type* llvmTo=map_type(toTy);
					if(op=="zext") castV=builder.CreateZExt(srcV, llvmTo, dst);
					else if(op=="sext") castV=builder.CreateSExt(srcV, llvmTo, dst);
					else if(op=="trunc") castV=builder.CreateTrunc(srcV, llvmTo, dst);
					else if(op=="bitcast") castV=builder.CreateBitCast(srcV, llvmTo, dst);
					else if(op=="sitofp") castV=builder.CreateSIToFP(srcV, llvmTo, dst);
					else if(op=="uitofp") castV=builder.CreateUIToFP(srcV, llvmTo, dst);
					else if(op=="fptosi") castV=builder.CreateFPToSI(srcV, llvmTo, dst);
					else if(op=="fptoui") castV=builder.CreateFPToUI(srcV, llvmTo, dst);
					else if(op=="ptrtoint") castV=builder.CreatePtrToInt(srcV, llvmTo, dst);
					else if(op=="inttoptr") castV=builder.CreateIntToPtr(srcV, llvmTo, dst);
					if(castV){ vmap[dst]=castV; vtypes[dst]=toTy; }
				}
				else if(op=="assign" && il.size()==3){ std::string dst=trimPct(symName(il[1])); std::string src=trimPct(symName(il[2])); if(dst.empty()||src.empty()) continue; auto it=vmap.find(src); if(it!=vmap.end()){ vmap[dst]=it->second; if(vtypes.count(src)) vtypes[dst]=vtypes[src]; } }
				else if(op=="alloca" && il.size()==3){ std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId ty; try{ ty=tctx_.parse_type(il[2]); }catch(...){ continue; } auto *av=builder.CreateAlloca(map_type(ty), nullptr, dst); vmap[dst]=av; vtypes[dst]=tctx_.get_pointer(ty); }
				else if(op=="struct-lit" && il.size()==4){ // (struct-lit %dst StructName [ field1 %v1 ... ]) produce pointer to struct alloca
					std::string dst=trimPct(symName(il[1])); std::string sname=symName(il[2]); if(dst.empty()||sname.empty()) continue; if(!std::holds_alternative<vector_t>(il[3]->data)) continue; auto idxIt=struct_field_index_.find(sname); auto ftIt=struct_field_types_.find(sname); if(idxIt==struct_field_index_.end()||ftIt==struct_field_types_.end()) continue; auto *ST = llvm::StructType::getTypeByName(*llctx_, "struct."+sname); if(!ST) { // create body if missed
						std::vector<llvm::Type*> ftys; for(auto tid: ftIt->second) ftys.push_back(map_type(tid)); ST=llvm::StructType::create(*llctx_, ftys, "struct."+sname); }
					auto *allocaPtr = builder.CreateAlloca(ST, nullptr, dst);
					auto &vec= std::get<vector_t>(il[3]->data).elems; // expect name/value pairs in order
					for(size_t i=0,fi=0;i+1<vec.size(); i+=2,++fi){ if(!std::holds_alternative<symbol>(vec[i]->data) || !std::holds_alternative<symbol>(vec[i+1]->data)) continue; std::string fname=symName(vec[i]); std::string val=trimPct(symName(vec[i+1])); if(val.empty()) continue; auto vit=vmap.find(val); if(vit==vmap.end()) continue; auto idxMapIt=idxIt->second.find(fname); if(idxMapIt==idxIt->second.end()) continue; uint32_t fidx=idxMapIt->second; llvm::Value* zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_),0); llvm::Value* fIndex=llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_),fidx); auto *gep=builder.CreateInBoundsGEP(ST, allocaPtr, {zero,fIndex}, dst+"."+fname+".addr"); builder.CreateStore(vit->second, gep); }
					vmap[dst]=allocaPtr; vtypes[dst]=tctx_.get_pointer(tctx_.get_struct(sname));
				}
				else if(op=="array-lit" && il.size()==5){ // (array-lit %dst <elem-type> <size> [ %e0 ... ])
					std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId elemTy; try{ elemTy=tctx_.parse_type(il[2]); }catch(...){ continue; } if(!std::holds_alternative<int64_t>(il[3]->data)) continue; uint64_t asz=(uint64_t)std::get<int64_t>(il[3]->data); if(asz==0) continue; if(!std::holds_alternative<vector_t>(il[4]->data)) continue; auto &elems=std::get<vector_t>(il[4]->data).elems; if(elems.size()!=asz) continue; TypeId arrTy = tctx_.get_array(elemTy, asz); auto *AT = llvm::cast<llvm::ArrayType>(map_type(arrTy)); auto *allocaPtr = builder.CreateAlloca(AT, nullptr, dst); for(size_t i=0;i<elems.size(); ++i){ if(!std::holds_alternative<symbol>(elems[i]->data)) continue; std::string val=trimPct(symName(elems[i])); if(val.empty()) continue; auto vit=vmap.find(val); if(vit==vmap.end()) continue; llvm::Value* zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_),0); llvm::Value* idx=llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_),(uint32_t)i); auto *gep=builder.CreateInBoundsGEP(AT, allocaPtr, {zero,idx}, dst+".elem"+std::to_string(i)+".addr"); builder.CreateStore(vit->second, gep); }
					vmap[dst]=allocaPtr; vtypes[dst]=tctx_.get_pointer(arrTy);
				}
				else if(op=="phi" && il.size()==4){ std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId ty; try{ ty=tctx_.parse_type(il[2]); }catch(...){ continue; } if(!std::holds_alternative<vector_t>(il[3]->data)) continue; std::vector<std::pair<std::string,std::string>> incomings; for(auto &inc: std::get<vector_t>(il[3]->data).elems){ if(!inc||!std::holds_alternative<list>(inc->data)) continue; auto &pl=std::get<list>(inc->data).elems; if(pl.size()!=2) continue; std::string val=trimPct(symName(pl[0])); std::string label=symName(pl[1]); if(!val.empty() && !label.empty()) incomings.emplace_back(val,label); } pendingPhis.push_back(PendingPhi{dst,ty,std::move(incomings),builder.GetInsertBlock()}); }
				else if(op=="store" && il.size()==4){ std::string ptrn=trimPct(symName(il[2])); std::string valn=trimPct(symName(il[3])); if(ptrn.empty()||valn.empty()) continue; auto pit=vmap.find(ptrn); auto vit=vmap.find(valn); if(pit==vmap.end()||vit==vmap.end()) continue; builder.CreateStore(vit->second,pit->second); }
				else if(op=="gload" && il.size()==4){ std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId ty; try{ ty=tctx_.parse_type(il[2]); }catch(...){ continue; } std::string gname=symName(il[3]); if(gname.empty()) continue; auto *gv=module_->getGlobalVariable(gname); if(!gv) continue; auto *lv=builder.CreateLoad(map_type(ty), gv, dst); vmap[dst]=lv; vtypes[dst]=ty; }
				else if(op=="gstore" && il.size()==4){ std::string gname=symName(il[2]); std::string valn=trimPct(symName(il[3])); if(gname.empty()||valn.empty()) continue; auto *gv=module_->getGlobalVariable(gname); if(!gv) continue; auto vit=vmap.find(valn); if(vit==vmap.end()) continue; builder.CreateStore(vit->second, gv); }
				else if(op=="load" && il.size()==4){ std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId ty; try{ ty=tctx_.parse_type(il[2]); }catch(...){ continue; } std::string ptrn=trimPct(symName(il[3])); auto it=vmap.find(ptrn); if(it==vmap.end()||!vtypes.count(ptrn)) continue; TypeId pty=vtypes[ptrn]; const Type& PT=tctx_.at(pty); if(PT.kind!=Type::Kind::Pointer||PT.pointee!=ty) continue; auto *lv=builder.CreateLoad(map_type(ty), it->second, dst); vmap[dst]=lv; vtypes[dst]=ty; }
				else if(op=="index" && il.size()==5){ std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; TypeId elemTy; try{ elemTy=tctx_.parse_type(il[2]); }catch(...){ continue; } auto *baseV=getVal(il[3]); auto *idxV=getVal(il[4]); if(!baseV||!idxV) continue; std::string baseName=trimPct(symName(il[3])); if(!vtypes.count(baseName)) continue; TypeId baseTyId=vtypes[baseName]; const Type* baseTy=&tctx_.at(baseTyId); if(baseTy->kind!=Type::Kind::Pointer) continue; const Type* arrTy=&tctx_.at(baseTy->pointee); if(arrTy->kind!=Type::Kind::Array||arrTy->elem!=elemTy) continue; llvm::Value* zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_),0); auto *gep=builder.CreateInBoundsGEP(map_type(baseTy->pointee), baseV, {zero,idxV}, dst); vmap[dst]=gep; vtypes[dst]=tctx_.get_pointer(elemTy); }
				else if(op=="member" && il.size()==5){ std::string dst=trimPct(symName(il[1])); std::string sname=symName(il[2]); std::string base=trimPct(symName(il[3])); std::string fname=symName(il[4]); if(dst.empty()||sname.empty()||base.empty()||fname.empty()) continue; auto bit=vmap.find(base); if(bit==vmap.end()||!vtypes.count(base)) continue; TypeId bty=vtypes[base]; const Type& BT=tctx_.at(bty); TypeId structId=0; bool baseIsPtr=false; if(BT.kind==Type::Kind::Pointer){ baseIsPtr=true; if(tctx_.at(BT.pointee).kind==Type::Kind::Struct) structId=BT.pointee; } else if(BT.kind==Type::Kind::Struct) structId=bty; if(structId==0||!baseIsPtr) continue; const Type& ST=tctx_.at(structId); if(ST.kind!=Type::Kind::Struct || ST.struct_name!=sname) continue; auto stIt=struct_types_.find(sname); if(stIt==struct_types_.end()) continue; auto idxIt=struct_field_index_.find(sname); if(idxIt==struct_field_index_.end()) continue; auto fIt=idxIt->second.find(fname); if(fIt==idxIt->second.end()) continue; size_t fidx=fIt->second; auto ftIt=struct_field_types_.find(sname); if(ftIt==struct_field_types_.end()||fidx>=ftIt->second.size()) continue; llvm::Value* zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_),0); llvm::Value* fieldIndex=llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_),(uint32_t)fidx); auto *gep=builder.CreateInBoundsGEP(stIt->second, bit->second, {zero,fieldIndex}, dst+".addr"); auto *lv=builder.CreateLoad(map_type(ftIt->second[fidx]), gep, dst); vmap[dst]=lv; vtypes[dst]=ftIt->second[fidx]; }
				else if(op=="union-member" && il.size()==5){ // (union-member %dst Union %ptr field)
					std::string dst=trimPct(symName(il[1])); std::string uname=symName(il[2]); std::string base=trimPct(symName(il[3])); std::string fname=symName(il[4]); if(dst.empty()||uname.empty()||base.empty()||fname.empty()) continue; auto bit=vmap.find(base); if(bit==vmap.end()||!vtypes.count(base)) continue; TypeId bty=vtypes[base]; const Type& BT=tctx_.at(bty); if(BT.kind!=Type::Kind::Pointer) continue; TypeId pointee=BT.pointee; const Type& PT=tctx_.at(pointee); if(PT.kind!=Type::Kind::Struct || PT.struct_name!=uname) continue; auto stIt=struct_types_.find(uname); if(stIt==struct_types_.end()) continue; auto uftIt = union_field_types_.find(uname); if(uftIt==union_field_types_.end()) continue; auto fTyIt=uftIt->second.find(fname); if(fTyIt==uftIt->second.end()) continue; TypeId fieldTy = fTyIt->second; // compute pointer to storage start
					llvm::Value* zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_),0); llvm::Value* storageIndex=llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_),0); auto *storagePtr=builder.CreateInBoundsGEP(stIt->second, bit->second, {zero,storageIndex}, dst+".ustorage.addr"); // [N x i8]*
					// Bitcast storage to pointer to field type and load
					llvm::Type* fieldLL = map_type(fieldTy);
					llvm::PointerType* fieldPtrTy = llvm::PointerType::getUnqual(fieldLL);
					auto *rawPtr = builder.CreateBitCast(storagePtr, fieldPtrTy, dst+".cast");
					auto *lv = builder.CreateLoad(fieldLL, rawPtr, dst);
					vmap[dst]=lv; vtypes[dst]=fieldTy; }
				else if(op=="member-addr" && il.size()==5){ std::string dst=trimPct(symName(il[1])); std::string sname=symName(il[2]); std::string base=trimPct(symName(il[3])); std::string fname=symName(il[4]); if(dst.empty()||sname.empty()||base.empty()||fname.empty()) continue; auto bit=vmap.find(base); if(bit==vmap.end()||!vtypes.count(base)) continue; TypeId bty=vtypes[base]; const Type& BT=tctx_.at(bty); TypeId structId=0; bool baseIsPtr=false; if(BT.kind==Type::Kind::Pointer){ baseIsPtr=true; if(tctx_.at(BT.pointee).kind==Type::Kind::Struct) structId=BT.pointee; } else if(BT.kind==Type::Kind::Struct) structId=bty; if(structId==0||!baseIsPtr) continue; const Type& ST=tctx_.at(structId); if(ST.kind!=Type::Kind::Struct || ST.struct_name!=sname) continue; auto stIt=struct_types_.find(sname); if(stIt==struct_types_.end()) continue; auto idxIt=struct_field_index_.find(sname); if(idxIt==struct_field_index_.end()) continue; auto fIt=idxIt->second.find(fname); if(fIt==idxIt->second.end()) continue; size_t fidx=fIt->second; auto ftIt=struct_field_types_.find(sname); if(ftIt==struct_field_types_.end()||fidx>=ftIt->second.size()) continue; llvm::Value* zero=llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_),0); llvm::Value* fieldIndex=llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llctx_),(uint32_t)fidx); auto *gep=builder.CreateInBoundsGEP(stIt->second, bit->second, {zero,fieldIndex}, dst); vmap[dst]=gep; vtypes[dst]=tctx_.get_pointer(ftIt->second[fidx]); }
				else if(op=="call" && il.size()>=4){ std::string dst=trimPct(symName(il[1])); TypeId retTy; try{ retTy=tctx_.parse_type(il[2]); }catch(...){ continue; } std::string callee=symName(il[3]); if(callee.empty()) continue; llvm::Function* CF=module_->getFunction(callee); if(!CF){ // create forward decl; attempt to detect variadic via type cache by matching existing function type if any
					// Scan original module AST for function header to recover signature & variadic flag
					bool foundHeader=false; std::vector<llvm::Type*> headerParamLL; bool headerVariadic=false; // fallback param list from current call if header parse fails
					for(size_t ti=1; ti<top.size() && !foundHeader; ++ti){ auto &fnNode = top[ti]; if(!fnNode||!std::holds_alternative<list>(fnNode->data)) continue; auto &fl2 = std::get<list>(fnNode->data).elems; if(fl2.empty()||!std::holds_alternative<symbol>(fl2[0]->data) || std::get<symbol>(fl2[0]->data).name!="fn") continue; std::string fname2; TypeId retHeader=tctx_.get_base(BaseType::Void); std::vector<TypeId> paramTypeIds; bool varargFlag=false; for(size_t j=1;j<fl2.size(); ++j){ if(!fl2[j]||!std::holds_alternative<keyword>(fl2[j]->data)) break; std::string kw=std::get<keyword>(fl2[j]->data).name; if(++j>=fl2.size()) break; auto val=fl2[j]; if(kw=="name"){ fname2=symName(val); } else if(kw=="ret"){ try{ retHeader=tctx_.parse_type(val);}catch(...){ retHeader=tctx_.get_base(BaseType::Void);} } else if(kw=="params" && val && std::holds_alternative<vector_t>(val->data)){ for(auto &p: std::get<vector_t>(val->data).elems){ if(!p||!std::holds_alternative<list>(p->data)) continue; auto &pl=std::get<list>(p->data).elems; if(pl.size()==3 && std::holds_alternative<symbol>(pl[0]->data) && std::get<symbol>(pl[0]->data).name=="param"){ try{ TypeId pty=tctx_.parse_type(pl[1]); paramTypeIds.push_back(pty);}catch(...){ } } } } else if(kw=="vararg"){ if(val && std::holds_alternative<bool>(val->data)) varargFlag=std::get<bool>(val->data); } }
						if(fname2==callee){ // build FunctionType using header info
							for(auto pid: paramTypeIds) headerParamLL.push_back(map_type(pid));
							headerVariadic=varargFlag; // override return type from header (compiler may mismatch annotation; rely on checker)
							CF = llvm::Function::Create(llvm::FunctionType::get(map_type(retHeader), headerParamLL, headerVariadic), llvm::Function::ExternalLinkage, callee, module_.get());
							foundHeader=true; break; }
					}
					if(!foundHeader){ // fallback: use current arg variable types (best effort, assume non-variadic)
						std::vector<llvm::Type*> argLTys; argLTys.reserve(il.size()-4); for(size_t ai=4; ai<il.size(); ++ai){ std::string av=trimPct(symName(il[ai])); if(av.empty()||!vtypes.count(av)){ argLTys.clear(); break;} argLTys.push_back(map_type(vtypes[av])); }
						CF=llvm::Function::Create(llvm::FunctionType::get(map_type(retTy), argLTys, false), llvm::Function::ExternalLinkage, callee, module_.get()); }
				}
				std::vector<llvm::Value*> args; for(size_t ai=4; ai<il.size(); ++ai){ auto *v=getVal(il[ai]); if(!v){ args.clear(); break;} args.push_back(v);} if(args.size()+4!=il.size()) continue; auto *callInst=builder.CreateCall(CF,args, CF->getReturnType()->isVoidTy()?"":dst); if(!CF->getReturnType()->isVoidTy()){ vmap[dst]=callInst; vtypes[dst]=retTy; } }
				else if(op=="va-start" && il.size()==2){ // allocate simple i8* zero for va_list handle
					std::string ap=trimPct(symName(il[1])); if(ap.empty()) continue; // represent va_list as i8* pointer to first extra arg placeholder (not implemented)
					llvm::Value* nullp = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llctx_)));
					vmap[ap]=nullp; // already attached type by checker
				}
				else if(op=="va-arg" && il.size()==4){ std::string dst=trimPct(symName(il[1])); if(dst.empty()) continue; // produce undef of requested type (checker ensured types)
					TypeId ty; try{ ty=tctx_.parse_type(il[2]); }catch(...){ continue; }
					llvm::Type* lty=map_type(ty); llvm::Value* uv=llvm::UndefValue::get(lty); vmap[dst]=uv; vtypes[dst]=ty; }
				else if(op=="va-end" && il.size()==2){ /* no-op */ }
				else if(op=="if"){ if(il.size()>=3){ std::string cond=trimPct(symName(il[1])); auto itc=vmap.find(cond); if(itc==vmap.end()) continue; auto *thenBB=llvm::BasicBlock::Create(*llctx_,"if.then."+std::to_string(cfCounter++),F); llvm::BasicBlock* elseBB=nullptr; auto *mergeBB=llvm::BasicBlock::Create(*llctx_,"if.end."+std::to_string(cfCounter++),F); bool hasElse=il.size()>=4 && std::holds_alternative<vector_t>(il[3]->data); if(hasElse) elseBB=llvm::BasicBlock::Create(*llctx_,"if.else."+std::to_string(cfCounter++),F); if(!builder.GetInsertBlock()->getTerminator()) builder.CreateCondBr(itc->second, thenBB, hasElse?elseBB:mergeBB); builder.SetInsertPoint(thenBB); if(std::holds_alternative<vector_t>(il[2]->data)) emit_ref(std::get<vector_t>(il[2]->data).elems, emit_ref); if(!thenBB->getTerminator()) builder.CreateBr(mergeBB); if(hasElse){ builder.SetInsertPoint(elseBB); emit_ref(std::get<vector_t>(il[3]->data).elems, emit_ref); if(!elseBB->getTerminator()) builder.CreateBr(mergeBB);} builder.SetInsertPoint(mergeBB);} }
				else if(op=="while"){ if(il.size()>=3 && std::holds_alternative<vector_t>(il[2]->data)){ std::string cond=trimPct(symName(il[1])); auto itc=vmap.find(cond); if(itc==vmap.end()) continue; auto *condBB=llvm::BasicBlock::Create(*llctx_,"while.cond."+std::to_string(cfCounter++),F); auto *bodyBB=llvm::BasicBlock::Create(*llctx_,"while.body."+std::to_string(cfCounter++),F); auto *endBB=llvm::BasicBlock::Create(*llctx_,"while.end."+std::to_string(cfCounter++),F); if(!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(condBB); builder.SetInsertPoint(condBB); builder.CreateCondBr(itc->second, bodyBB, endBB); builder.SetInsertPoint(bodyBB); loopEndStack.push_back(endBB); loopContinueStack.push_back(condBB); emit_ref(std::get<vector_t>(il[2]->data).elems, emit_ref); loopContinueStack.pop_back(); loopEndStack.pop_back(); if(!bodyBB->getTerminator()) builder.CreateBr(condBB); builder.SetInsertPoint(endBB);} }
				else if(op=="for"){ // (for :init [..] :cond %c :step [..] :body [..])
					// Parse keyword sections
					std::vector<node_ptr> initVec, condSymNode, stepVec, bodyVec; std::string condVar;
					for(size_t i=1;i<il.size(); ++i){ if(!il[i]||!std::holds_alternative<keyword>(il[i]->data)) break; std::string kw=std::get<keyword>(il[i]->data).name; if(++i>=il.size()) break; auto val=il[i]; if(kw=="init" && val && std::holds_alternative<vector_t>(val->data)) initVec=std::get<vector_t>(val->data).elems; else if(kw=="cond" && val && std::holds_alternative<symbol>(val->data)) condVar=symName(val); else if(kw=="step" && val && std::holds_alternative<vector_t>(val->data)) stepVec=std::get<vector_t>(val->data).elems; else if(kw=="body" && val && std::holds_alternative<vector_t>(val->data)) bodyVec=std::get<vector_t>(val->data).elems; }
					// emit init
					if(!initVec.empty()) emit_ref(initVec, emit_ref);
					// blocks
					auto *condBB=llvm::BasicBlock::Create(*llctx_,"for.cond."+std::to_string(cfCounter++),F);
					auto *bodyBB=llvm::BasicBlock::Create(*llctx_,"for.body."+std::to_string(cfCounter++),F);
					auto *stepBB=llvm::BasicBlock::Create(*llctx_,"for.step."+std::to_string(cfCounter++),F);
					auto *endBB=llvm::BasicBlock::Create(*llctx_,"for.end."+std::to_string(cfCounter++),F);
					if(!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(condBB);
					builder.SetInsertPoint(condBB);
					auto itc=vmap.find(trimPct(condVar)); if(itc==vmap.end()) { builder.CreateBr(endBB); builder.SetInsertPoint(endBB); }
					else { builder.CreateCondBr(itc->second, bodyBB, endBB); builder.SetInsertPoint(bodyBB); loopEndStack.push_back(endBB); loopContinueStack.push_back(stepBB); if(!bodyVec.empty()) emit_ref(bodyVec, emit_ref); loopContinueStack.pop_back(); if(!bodyBB->getTerminator()) builder.CreateBr(stepBB); builder.SetInsertPoint(stepBB); if(!stepVec.empty()) emit_ref(stepVec, emit_ref); loopEndStack.pop_back(); if(!stepBB->getTerminator()) builder.CreateBr(condBB); builder.SetInsertPoint(endBB); }
				}
				else if(op=="switch"){ // (switch %expr :cases [ (case <int> [ ... ])* ] :default [ ... ])
					if(il.size()<2) continue; std::string expr=trimPct(symName(il[1])); if(expr.empty()||!vmap.count(expr)) continue; llvm::Value* exprV=vmap[expr];
					// Parse sections
					std::vector<std::pair<int64_t,std::vector<node_ptr>>> cases; std::vector<node_ptr> defaultBody; bool haveDefault=false; node_ptr casesNode=nullptr, defaultNode=nullptr;
					for(size_t i=2;i<il.size(); ++i){ if(!il[i]||!std::holds_alternative<keyword>(il[i]->data)) break; std::string kw=std::get<keyword>(il[i]->data).name; if(++i>=il.size()) break; auto val=il[i]; if(kw=="cases" && val && std::holds_alternative<vector_t>(val->data)) casesNode=val; else if(kw=="default" && val && std::holds_alternative<vector_t>(val->data)) { defaultNode=val; haveDefault=true; } }
					if(casesNode){ for(auto &cv: std::get<vector_t>(casesNode->data).elems){ if(!cv||!std::holds_alternative<list>(cv->data)) continue; auto &cl=std::get<list>(cv->data).elems; if(cl.size()<3) continue; if(!std::holds_alternative<symbol>(cl[0]->data) || std::get<symbol>(cl[0]->data).name!="case") continue; if(!std::holds_alternative<int64_t>(cl[1]->data)) continue; if(!std::holds_alternative<vector_t>(cl[2]->data)) continue; int64_t cval=std::get<int64_t>(cl[1]->data); cases.emplace_back(cval, std::get<vector_t>(cl[2]->data).elems); } }
					if(haveDefault && defaultNode) defaultBody= std::get<vector_t>(defaultNode->data).elems;
					// Build blocks: chain compares; create merge block
					auto *mergeBB=llvm::BasicBlock::Create(*llctx_,"switch.end."+std::to_string(cfCounter++),F);
					std::vector<llvm::BasicBlock*> caseBlocks; caseBlocks.reserve(cases.size()); for(size_t ci=0; ci<cases.size(); ++ci) caseBlocks.push_back(llvm::BasicBlock::Create(*llctx_,"switch.case."+std::to_string(cases[ci].first)+"."+std::to_string(cfCounter++),F));
					llvm::BasicBlock* defaultBB = haveDefault ? llvm::BasicBlock::Create(*llctx_,"switch.default."+std::to_string(cfCounter++),F) : mergeBB;
					// Initial jump: we'll linearize comparisons
					auto *curBB=builder.GetInsertBlock(); if(!curBB->getTerminator()) builder.CreateBr(caseBlocks.empty()?defaultBB:caseBlocks[0]);
					for(size_t ci=0; ci<cases.size(); ++ci){ builder.SetInsertPoint(caseBlocks[ci]); int64_t cval=cases[ci].first; llvm::Value* constVal=nullptr; if(exprV->getType()->isIntegerTy()) constVal=llvm::ConstantInt::get(exprV->getType(), (uint64_t)cval, true); else { // fallback treat as i64 compare then cast
						// unsupported type (non-integer) already rejected by type checker; just branch to default
						builder.CreateBr(defaultBB); continue; }
						llvm::Value* cmp=builder.CreateICmpEQ(exprV,constVal,"swcmp");
						// Body block for matched case
						auto *bodyBB=llvm::BasicBlock::Create(*llctx_,"switch.case.body."+std::to_string(cval)+"."+std::to_string(cfCounter++),F);
						auto *nextCmpBB = (ci+1<cases.size())? caseBlocks[ci+1] : defaultBB;
						builder.CreateCondBr(cmp, bodyBB, nextCmpBB);
						builder.SetInsertPoint(bodyBB);
						emit_ref(cases[ci].second, emit_ref);
						if(!bodyBB->getTerminator()) builder.CreateBr(mergeBB);
					}
					if(haveDefault){ builder.SetInsertPoint(defaultBB); emit_ref(defaultBody, emit_ref); if(!defaultBB->getTerminator()) builder.CreateBr(mergeBB); }
					builder.SetInsertPoint(mergeBB);
				}
				else if(op=="break"){ if(!loopEndStack.empty() && !builder.GetInsertBlock()->getTerminator()) builder.CreateBr(loopEndStack.back()); return; }
				else if(op=="continue"){ if(!loopContinueStack.empty() && !builder.GetInsertBlock()->getTerminator()){ builder.CreateBr(loopContinueStack.back()); return; } }
				else if(op=="ret" && il.size()==3){ std::string rv=trimPct(symName(il[2])); if(!rv.empty() && vmap.count(rv)) builder.CreateRet(vmap[rv]); else if(fty->getReturnType()->isVoidTy()) builder.CreateRetVoid(); else builder.CreateRet(llvm::Constant::getNullValue(fty->getReturnType())); functionDone=true; return; }
			}
		};
		emit_list(std::get<vector_t>(body->data).elems, emit_list);
		// Realize pending phi nodes now that all basic blocks exist.
		for(auto &pp: pendingPhis){ llvm::BasicBlock* insertBB = pp.insertBlock; if(!insertBB) continue; llvm::IRBuilder<> tmpBuilder(insertBB, insertBB->begin());
			llvm::PHINode* phi = tmpBuilder.CreatePHI(map_type(pp.ty), (unsigned)pp.incomings.size(), pp.dst);
			vmap[pp.dst]=phi; vtypes[pp.dst]=pp.ty;
			for(auto &inc : pp.incomings){ auto itv=vmap.find(inc.first); if(itv==vmap.end()) continue; // find basic block by exact name
				llvm::BasicBlock* pred=nullptr; for(auto &bb : *F){ if(bb.getName()==inc.second){ pred=&bb; break; } }
				if(pred) phi->addIncoming(itv->second, pred); }
		}
		if(!entry->getTerminator()){ if(fty->getReturnType()->isVoidTy()) builder.CreateRetVoid(); else builder.CreateRet(llvm::Constant::getNullValue(fty->getReturnType())); }
	}
	return module_.get();
}

llvm::orc::ThreadSafeModule IREmitter::toThreadSafeModule(){ return llvm::orc::ThreadSafeModule(std::move(module_), std::move(llctx_)); }

} // namespace edn
