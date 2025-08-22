// edn.cpp - Clean IR emitter implementation (fully rewritten after corruption)
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/generics.hpp"
#include "edn/traits.hpp"
#include "edn/diagnostics_json.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/Attributes.h>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <functional>
// IR context/env (centralized env parsing)
#include "edn/ir/context.hpp"
#include "edn/ir/exceptions.hpp"
// Centralized DataLayout helpers
#include "edn/ir/types.hpp"
#include "edn/ir/layout.hpp"
#include "edn/ir/debug.hpp"
#include "edn/ir/collect.hpp"
#include "edn/ir/core_ops.hpp"
#include "edn/ir/memory_ops.hpp"
#include "edn/ir/sum_ops.hpp"
#include "edn/ir/control_ops.hpp"
#include "edn/ir/closure_ops.hpp"

namespace edn
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
	static std::string trimPct(const std::string &s) { return (!s.empty() && s[0] == '%') ? s.substr(1) : s; }

	IREmitter::IREmitter(TypeContext &tctx) : tctx_(tctx) { llctx_ = std::make_unique<llvm::LLVMContext>(); }
	IREmitter::~IREmitter() = default;

	llvm::Module *IREmitter::emit(const node_ptr &module_ast, TypeCheckResult &tc_result)
	{
		// First, expand reader-macros that rewrite into core forms
		// Order: traits -> generics (traits produce plain structs/globals; generics may reference them)
		node_ptr rewritten = expand_traits(module_ast);
		rewritten = expand_generics(rewritten);
		TypeChecker checker(tctx_);
		tc_result = checker.check_module(rewritten);
		// Optional JSON diagnostics output (set EDN_DIAG_JSON=1)
		extern void maybe_print_json(const TypeCheckResult &); // forward (header-only impl)
		maybe_print_json(tc_result);
		if (!tc_result.success)
			return nullptr;
		module_ = std::make_unique<llvm::Module>("edn.module", *llctx_);
		// Optional: enable LLVM Debug Info (DWARF/CodeView agnostic in IR)
		bool enableDebugInfo = false;

		// Build Debug Manager
		auto debug_manager_ = std::make_shared<edn::ir::debug::DebugManager>(enableDebugInfo, module_, this);
		debug_manager_->initialize();

		// Centralized env
		const auto env = edn::detectEnv();
		edn::applyEnvToModule(*module_, env);
		if (!rewritten || !std::holds_alternative<list>(rewritten->data))
			return nullptr;
		auto &top = std::get<list>(rewritten->data).elems;
		if (top.empty())
			return nullptr;

		// Optional: configure an EH personality for all defined functions; model selection is
		// independent of whether EH emission (invokes/funclets) is enabled.
		llvm::Constant *selectedPersonality = edn::ir::exceptions::select_personality(*module_, env);
		bool enableEHItanium = env.enableEHItanium;
		bool enableEHSEH = env.enableEHSEH;
		bool panicUnwind = env.panicUnwind;
		bool enableCoro = env.enableCoro;

		// Collect Structs, Sums, Unions, Globals
		// TODO Fill in the call to run
		edn::ir::collect::run(top, this);

		for (size_t i = 1; i < top.size(); ++i)
		{
			auto fn = top[i];
			if (!fn || !std::holds_alternative<list>(fn->data))
				continue;
			auto &fl = std::get<list>(fn->data).elems;
			if (fl.empty())
				continue;
			if (!std::holds_alternative<symbol>(fl[0]->data) || std::get<symbol>(fl[0]->data).name != "fn")
				continue;
			std::string fname;
			TypeId retTy = tctx_.get_base(BaseType::Void);
			std::vector<std::pair<std::string, TypeId>> params;
			node_ptr body;
			bool isExternal = false;
			for (size_t j = 1; j < fl.size(); ++j)
			{
				if (!std::holds_alternative<keyword>(fl[j]->data))
					continue;
				std::string kw = std::get<keyword>(fl[j]->data).name;
				if (++j >= fl.size())
					break;
				auto val = fl[j];
				if (kw == "name")
					fname = symName(val);
				else if (kw == "ret")
					try
					{
						retTy = tctx_.parse_type(val);
					}
					catch (...)
					{
					}
				else if (kw == "params" && std::holds_alternative<vector_t>(val->data))
				{
					for (auto &p : std::get<vector_t>(val->data).elems)
					{
						if (!p || !std::holds_alternative<list>(p->data))
							continue;
						auto &pl = std::get<list>(p->data).elems;
						if (pl.size() != 3)
							continue;
						if (!std::holds_alternative<symbol>(pl[0]->data) || std::get<symbol>(pl[0]->data).name != "param")
							continue;
						try
						{
							TypeId pty = tctx_.parse_type(pl[1]);
							std::string pname = trimPct(symName(pl[2]));
							if (!pname.empty())
								params.emplace_back(pname, pty);
						}
						catch (...)
						{
						}
					}
				}
				else if (kw == "body" && std::holds_alternative<vector_t>(val->data))
					body = val;
				else if (kw == "external" && std::holds_alternative<bool>(val->data))
					isExternal = std::get<bool>(val->data);
			}
			if (fname.empty() || !body)
				continue;
			std::vector<TypeId> paramIds;
			for (auto &pr : params)
				paramIds.push_back(pr.second);
			// Determine variadic flag by re-parsing :vararg from function list (cheap scan)
			bool isVariadic = false;
			for (size_t j = 1; j < fl.size(); ++j)
			{
				if (fl[j] && std::holds_alternative<keyword>(fl[j]->data) && std::get<keyword>(fl[j]->data).name == "vararg")
				{
					if (j + 1 < fl.size() && std::holds_alternative<bool>(fl[j + 1]->data))
						isVariadic = std::get<bool>(fl[j + 1]->data);
					break;
				}
			}
			if (isExternal)
			{ // emit declaration only (skip body even if mistakenly present)
				std::vector<TypeId> paramIds;
				for (auto &pr : params)
					paramIds.push_back(pr.second);
				auto ftyId = tctx_.get_function(paramIds, retTy, isVariadic);
				auto *fty = llvm::cast<llvm::FunctionType>(map_type(ftyId));
				(void)llvm::Function::Create(fty, llvm::Function::ExternalLinkage, fname, module_.get());
				continue;
			}
			auto ftyId = tctx_.get_function(paramIds, retTy, isVariadic);
			auto *fty = llvm::cast<llvm::FunctionType>(map_type(ftyId));
			auto *F = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, fname, module_.get());
			// Attach a DISubprogram to the function if debug is enabled
			llvm::DISubprogram *DI_SP = nullptr;
			if (enableDebugInfo && debug_manager_->DIB && debug_manager_->DI_File)
			{
				// Build a real subroutine type for the function signature
				std::vector<llvm::Metadata *> sigTys;
				// Return type first
				sigTys.push_back(static_cast<llvm::Metadata *>(debug_manager_->diTypeOf(retTy)));
				// Param types
				for (auto &pr : params)
				{
					sigTys.push_back(static_cast<llvm::Metadata *>(debug_manager_->diTypeOf(pr.second)));
				}
				auto *subTy = debug_manager_->DIB->createSubroutineType(debug_manager_->DIB->getOrCreateTypeArray(sigTys));
				unsigned lineNo = edn::line(*fl[0]); // best-effort: use the "fn" token start line
				if (lineNo == 0)
					lineNo = 1;
				DI_SP = debug_manager_->DIB->createFunction(
					/*Scope=*/debug_manager_->DI_File,
					/*Name=*/fname,
					/*LinkageName=*/fname,
					/*File=*/debug_manager_->DI_File,
					/*LineNo=*/lineNo,
					/*Type=*/subTy,
					/*ScopeLine=*/lineNo,
					/*Flags=*/llvm::DINode::FlagZero,
					/*SPFlags=*/llvm::DISubprogram::SPFlagDefinition);
				F->setSubprogram(DI_SP);
			}
			// If coroutines are enabled, mark functions as pre-split coroutine to use the Switch-Resumed ABI
			if (enableCoro)
			{
				// Add both the well-known enum AttrKind and the string form (harmless redundancy); CoroEarly
				// checks the built-in kind via F.isPresplitCoroutine().
				F->addFnAttr(llvm::Attribute::AttrKind::PresplitCoroutine);
				F->addFnAttr("presplitcoroutine");
			}
			// Attach selected personality if configured
			if (selectedPersonality)
				F->setPersonalityFn(selectedPersonality);
			// If EH is enabled for either model, add uwtable to ease unwinder table emission
			if (selectedPersonality && (enableEHItanium || enableEHSEH))
			{
				F->addFnAttr("uwtable");
			}
			size_t ai = 0;
			for (auto &arg : F->args())
				arg.setName(params[ai++].first);
			auto *entry = llvm::BasicBlock::Create(*llctx_, "entry", F);
			llvm::IRBuilder<> builder(entry);
			std::unordered_map<std::string, llvm::Value *> vmap;
			std::unordered_map<std::string, TypeId> vtypes;
			for (auto &pr : params)
			{
				vtypes[pr.first] = pr.second;
			}
			for (auto &arg : F->args())
			{
				vmap[std::string(arg.getName())] = &arg;
			}
			// Stack slots for variables that are assigned/address-taken
			std::unordered_map<std::string, llvm::AllocaInst *> varSlots;
			// Map initializer SSA names to their owning mutable variable (from `(as %dst <ty> %init)`)
			std::unordered_map<std::string, std::string> initAlias;
			auto ensureSlot = [&](const std::string &name, TypeId ty, bool initFromCurrent) -> llvm::AllocaInst *
			{
				auto it = varSlots.find(name);
				if (it != varSlots.end())
					return it->second;
				// Place allocas in the entry block for canonical form
				llvm::IRBuilder<> eb(&*entry->getFirstInsertionPt());
				auto *slot = eb.CreateAlloca(map_type(ty), nullptr, name + ".slot");
				varSlots[name] = slot;
				// Record the value type of the slot's contents
				vtypes[name] = ty;
				// Optionally initialize the slot with any existing SSA value for this name
				if (initFromCurrent)
				{
					auto itv = vmap.find(name);
					if (itv != vmap.end() && itv->second)
					{
						builder.CreateStore(itv->second, slot);
					}
				}
				return slot;
			};
			if (enableDebugInfo && F->getSubprogram())
			{
				builder.SetCurrentDebugLocation(llvm::DILocation::get(*llctx_, F->getSubprogram()->getLine(), /*Col*/ 1, F->getSubprogram()));
				// Emit parameter debug info at entry using dbg.value
				unsigned argIndex = 1;
				for (auto &arg : F->args())
				{
					std::string an = std::string(arg.getName());
					llvm::DIType *diTy = nullptr;
					if (auto it = vtypes.find(an); it != vtypes.end())
						diTy = debug_manager_->diTypeOf(it->second);
					auto *pvar = debug_manager_->DIB->createParameterVariable(F->getSubprogram(), an, argIndex, debug_manager_->DI_File, F->getSubprogram()->getLine(), diTy, true);
					auto *expr = debug_manager_->DIB->createExpression();
					(void)debug_manager_->DIB->insertDbgValueIntrinsic(&arg, pvar, expr, builder.getCurrentDebugLocation(), entry);
					++argIndex;
				}
			}
			llvm::Value *lastCoroIdTok = nullptr;			   // holds last coro.id token for ops that need it
			std::vector<llvm::BasicBlock *> loopEndStack;	   // for break targets (end blocks)
			std::vector<llvm::BasicBlock *> loopContinueStack; // for continue targets (condition re-check or step blocks)
			size_t cfCounter = 0;
			bool functionDone = false;
			// Track defining EDN nodes for values (used to recompute conditions each iteration)
			std::unordered_map<std::string, node_ptr> defNode;
			// Reusable Windows SEH cleanup funclet block (created on first use)
			llvm::BasicBlock *sehCleanupBB = nullptr;
			// Stack of active SEH exception targets (catch dispatch blocks)
			std::vector<llvm::BasicBlock *> sehExceptTargetStack;
			// Stack of active Itanium exception targets (landingpad dispatch blocks)
			std::vector<llvm::BasicBlock *> itnExceptTargetStack;
			// Defer phi creation until predecessors exist: collect specs in current block then create at end of block emission phase.
			struct PendingPhi
			{
				std::string dst;
				TypeId ty;
				std::vector<std::pair<std::string, std::string>> incomings;
				llvm::BasicBlock *insertBlock;
			};
			std::vector<PendingPhi> pendingPhis;
			auto emit_list = [&](const std::vector<node_ptr> &insts, auto &&emit_ref) -> void
			{
				if (functionDone)
					return;
				for (auto &inst : insts)
				{
					if (functionDone)
						break;
					if (!inst || !std::holds_alternative<list>(inst->data))
						continue;
					auto &il = std::get<list>(inst->data).elems;
					if (il.empty())
						continue;
					if (!std::holds_alternative<symbol>(il[0]->data))
						continue;
					std::string op = std::get<symbol>(il[0]->data).name;
					auto getVal = [&](const node_ptr &n) -> llvm::Value *
					{
						std::string nm = trimPct(symName(n));
						if (nm.empty())
							return nullptr;
						// If this name was an initializer to an `as`-declared variable, redirect to that variable
						auto loadFromAliasedVar = [&](const std::string &key) -> llvm::Value *
						{
							auto aIt = initAlias.find(key);
							if (aIt == initAlias.end())
								return (llvm::Value *)nullptr;
							const std::string &var = aIt->second;
							// Prefer loading from the variable's slot
							if (auto s2 = varSlots.find(var); s2 != varSlots.end())
							{
								auto t2 = vtypes.find(var);
								if (t2 == vtypes.end())
									return (llvm::Value *)nullptr;
								return builder.CreateLoad(map_type(t2->second), s2->second, var);
							}
							// Fallback to any SSA value registered for the variable
							if (auto vv = vmap.find(var); vv != vmap.end())
								return vv->second;
							return (llvm::Value *)nullptr;
						};
						if (auto aliased = loadFromAliasedVar(nm))
							return aliased;
						// Try common synthesized suffixes used for constants/temps
						auto normalized = nm;
						auto stripSuffix = [&](const char *suf)
						{ size_t L = strlen(suf); if(normalized.size()>=L && normalized.rfind(suf)==normalized.size()-L) normalized.resize(normalized.size()-L); };
						stripSuffix(".cst.load");
						stripSuffix(".load");
						stripSuffix(".cst.tmp");
						stripSuffix(".tmp");
						if (normalized != nm)
						{
							if (auto aliased2 = loadFromAliasedVar(normalized))
								return aliased2;
						}
						// If this name has a stack slot, always load from it
						auto sIt = varSlots.find(nm);
						if (sIt != varSlots.end())
						{
							auto tIt = vtypes.find(nm);
							if (tIt == vtypes.end())
								return nullptr;
							return builder.CreateLoad(map_type(tIt->second), sIt->second, nm);
						}
						auto it = vmap.find(nm);
						return (it != vmap.end()) ? it->second : nullptr;
					};
					// Attempt to recompute value from its defining EDN node (limited set: eq/ne/lt/gt/le/ge, and/or/xor)
					auto evalDefined = [&](const std::string &name) -> llvm::Value *
					{
						auto it = defNode.find(name);
						if (it == defNode.end())
							return nullptr;
						auto dn = it->second;
						if (!dn || !std::holds_alternative<list>(dn->data))
							return nullptr;
						auto &dl = std::get<list>(dn->data).elems;
						if (dl.empty() || !std::holds_alternative<symbol>(dl[0]->data))
							return nullptr;
						std::string dop = std::get<symbol>(dl[0]->data).name;
						auto valOf = [&](size_t idx) -> llvm::Value *
						{ if(idx>=dl.size()) return nullptr; return getVal(dl[idx]); };
						if ((dop == "eq" || dop == "ne" || dop == "lt" || dop == "gt" || dop == "le" || dop == "ge") && dl.size() == 5)
						{
							llvm::Value *va = valOf(3);
							llvm::Value *vb = valOf(4);
							if (!va || !vb)
								return nullptr;
							llvm::CmpInst::Predicate P = llvm::CmpInst::ICMP_EQ;
							if (dop == "eq")
								P = llvm::CmpInst::ICMP_EQ;
							else if (dop == "ne")
								P = llvm::CmpInst::ICMP_NE;
							else if (dop == "lt")
								P = llvm::CmpInst::ICMP_SLT;
							else if (dop == "gt")
								P = llvm::CmpInst::ICMP_SGT;
							else if (dop == "le")
								P = llvm::CmpInst::ICMP_SLE;
							else
								P = llvm::CmpInst::ICMP_SGE;
							return builder.CreateICmp(P, va, vb, name + ".re");
						}
						if ((dop == "and" || dop == "or" || dop == "xor") && dl.size() == 5)
						{
							llvm::Value *va = valOf(3);
							llvm::Value *vb = valOf(4);
							if (!va || !vb)
								return nullptr;
							if (dop == "and")
								return builder.CreateAnd(va, vb, name + ".re");
							if (dop == "or")
								return builder.CreateOr(va, vb, name + ".re");
							return builder.CreateXor(va, vb, name + ".re");
						}
						return nullptr;
					};

					// --- Dispatch extracted core ops (integer/float arith, bitwise, shifts, ptr math) ---
					{
						edn::ir::builder::State S{builder, *llctx_, *module_, tctx_,
												  [&](TypeId id)
												  { return map_type(id); }, vmap, vtypes, varSlots, initAlias, defNode, debug_manager_};
						if (edn::ir::core_ops::handle_integer_arith(S, il) ||
							edn::ir::core_ops::handle_float_arith(S, il) ||
							edn::ir::core_ops::handle_bitwise_shift(S, il, inst) ||
							edn::ir::core_ops::handle_ptr_add_sub(S, il) ||
							edn::ir::core_ops::handle_ptr_diff(S, il))
						{
							continue; // handled by modular core_ops
						}
						// --- Dispatch extracted memory ops (assign/alloca/load/store/gload/gstore) ---
						if (edn::ir::memory_ops::handle_assign(S, il) ||
							edn::ir::memory_ops::handle_alloca(S, il) ||
							edn::ir::memory_ops::handle_store(S, il) ||
							edn::ir::memory_ops::handle_gload(S, il) ||
							edn::ir::memory_ops::handle_gstore(S, il) ||
							edn::ir::memory_ops::handle_load(S, il) ||
							edn::ir::memory_ops::handle_index(S, il) ||
							edn::ir::memory_ops::handle_array_lit(S, il) ||
							edn::ir::memory_ops::handle_struct_lit(S, il, struct_field_index_, struct_field_types_) ||
							edn::ir::memory_ops::handle_member(S, il, struct_types_, struct_field_index_, struct_field_types_) ||
							edn::ir::memory_ops::handle_member_addr(S, il, struct_types_, struct_field_index_, struct_field_types_) ||
							edn::ir::memory_ops::handle_union_member(S, il, struct_types_, union_field_types_) ||
							edn::ir::closure_ops::handle_closure(S, il, top, cfCounter) ||
							edn::ir::closure_ops::handle_make_closure(S, il, top) ||
							edn::ir::closure_ops::handle_call_closure(S, il) ||
							edn::ir::sum_ops::handle_sum_new(S, il, sum_variant_tag_, sum_variant_field_types_) ||
							edn::ir::sum_ops::handle_sum_is(S, il, sum_variant_tag_) ||
							edn::ir::sum_ops::handle_sum_get(S, il, sum_variant_tag_, sum_variant_field_types_))
						{
							continue; // handled by modular memory_ops/sum_ops
						}
						// Control-flow ops (if/while/for/switch/match/break/continue)
						{
							edn::ir::control_ops::Context CF{
								S,
								cfCounter,
								F,
								[&](const std::vector<edn::node_ptr> &nodes)
								{ emit_ref(nodes, emit_ref); },
								[&](const std::string &nm)
								{ return evalDefined(nm); },
								[&](const edn::node_ptr &n)
								{ return getVal(n); },
								loopEndStack,
								loopContinueStack,
								sum_variant_tag_,
								sum_variant_field_types_};
							if (edn::ir::control_ops::handle(CF, il))
								continue;
						}
					}
					if (op == "block")
					{ // (block :locals [ ... ] :body [ ... ])
						node_ptr bodyNode = nullptr;
						for (size_t i = 1; i < il.size(); ++i)
						{
							if (!il[i] || !std::holds_alternative<keyword>(il[i]->data))
								break;
							std::string kw = std::get<keyword>(il[i]->data).name;
							if (++i >= il.size())
								break;
							auto val = il[i];
							if (kw == "body")
							{
								if (val && std::holds_alternative<vector_t>(val->data))
									bodyNode = val;
							}
						}
						if (bodyNode && std::holds_alternative<vector_t>(bodyNode->data))
						{
							emit_ref(std::get<vector_t>(bodyNode->data).elems, emit_ref);
						}
					}
					if (op == "const" && il.size() == 4)
					{
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue;
						TypeId ty;
						try
						{
							ty = tctx_.parse_type(il[2]);
						}
						catch (...)
						{
							continue;
						}
						llvm::Type *lty = map_type(ty);
						llvm::Value *cv = nullptr;
						if (std::holds_alternative<int64_t>(il[3]->data))
							cv = llvm::ConstantInt::get(lty, (uint64_t)std::get<int64_t>(il[3]->data), true);
						else if (std::holds_alternative<double>(il[3]->data))
							cv = llvm::ConstantFP::get(lty, std::get<double>(il[3]->data));
						if (!cv)
							cv = llvm::UndefValue::get(lty);
						vmap[dst] = cv;
						vtypes[dst] = ty;
					}
					else if ((op == "add" || op == "sub" || op == "mul" || op == "sdiv" || op == "udiv" || op == "srem" || op == "urem") && il.size() == 5)
					{
						// Legacy integer arithmetic path removed (handled earlier by ir::core_ops)
					}
					else if (op == "addr" && il.size() == 4)
					{ // (addr %dst (ptr <T>) %src)
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue;
						TypeId annot;
						try
						{
							annot = tctx_.parse_type(il[2]);
						}
						catch (...)
						{
							continue;
						}
						std::string srcName = trimPct(symName(il[3]));
						if (srcName.empty())
							continue;
						const Type &AT = tctx_.at(annot);
						if (AT.kind != Type::Kind::Pointer)
							continue;
						// Ensure a slot for the source variable and initialize from its current value (if any)
						TypeId valTy = AT.pointee;
						// If we already have a tracked type for the source, prefer it
						if (vtypes.count(srcName))
							valTy = vtypes[srcName];
						auto *slot = ensureSlot(srcName, valTy, /*initFromCurrent=*/true);
						vmap[dst] = slot;
						vtypes[dst] = annot;
					}
					else if (op == "deref" && il.size() == 4)
					{ // (deref %dst <T> %ptr)
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue;
						TypeId ty;
						try
						{
							ty = tctx_.parse_type(il[2]);
						}
						catch (...)
						{
							continue;
						}
						std::string ptrName = trimPct(symName(il[3]));
						if (ptrName.empty())
							continue;
						if (!vtypes.count(ptrName))
							continue;
						TypeId pty = vtypes[ptrName];
						const Type &PT = tctx_.at(pty);
						if (PT.kind != Type::Kind::Pointer || PT.pointee != ty)
							continue;
						auto *pv = vmap[ptrName];
						if (!pv)
							continue;
						auto *lv = builder.CreateLoad(map_type(ty), pv, dst);
						vmap[dst] = lv;
						vtypes[dst] = ty;
					}
					else if (op == "fnptr" && il.size() == 4)
					{ // (fnptr %dst (ptr (fn-type ...)) Name)
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue;
						TypeId pty;
						try
						{
							pty = tctx_.parse_type(il[2]);
						}
						catch (...)
						{
							continue;
						}
						std::string fname = symName(il[3]);
						if (fname.empty())
							continue;
						auto *F = module_->getFunction(fname);
						if (!F)
						{ // create decl with matching signature if we can
							const Type &PT = tctx_.at(pty);
							if (PT.kind != Type::Kind::Pointer)
								continue;
							const Type &FT = tctx_.at(PT.pointee);
							if (FT.kind != Type::Kind::Function)
								continue;
							std::vector<llvm::Type *> ps;
							for (auto pid : FT.params)
								ps.push_back(map_type(pid));
							auto *ftyDecl = llvm::FunctionType::get(map_type(FT.ret), ps, FT.variadic);
							F = llvm::Function::Create(ftyDecl, llvm::Function::ExternalLinkage, fname, module_.get());
						}
						if (!F)
							continue; // take address of function is just function pointer value in LLVM IR
						vmap[dst] = F;
						vtypes[dst] = pty;
					}
					else if (op == "call-indirect" && il.size() >= 4)
					{ // (call-indirect %dst <ret> %fptr %args...)
						std::string dst = trimPct(symName(il[1]));
						TypeId retTy;
						try
						{
							retTy = tctx_.parse_type(il[2]);
						}
						catch (...)
						{
							continue;
						}
						std::string fptrName = trimPct(symName(il[3]));
						if (fptrName.empty())
							continue;
						if (!vtypes.count(fptrName))
							continue;
						TypeId fpty = vtypes[fptrName];
						const Type &FPT = tctx_.at(fpty);
						if (FPT.kind != Type::Kind::Pointer)
							continue;
						const Type &FT = tctx_.at(FPT.pointee);
						if (FT.kind != Type::Kind::Function)
							continue;
						auto *calleeV = vmap[fptrName];
						if (!calleeV)
							continue;
						std::vector<llvm::Value *> args;
						bool bad = false;
						for (size_t ai = 4; ai < il.size(); ++ai)
						{
							std::string an = trimPct(symName(il[ai]));
							if (an.empty() || !vmap.count(an))
							{
								bad = true;
								break;
							}
							args.push_back(vmap[an]);
						}
						if (bad)
							continue;
						llvm::FunctionType *fty = llvm::cast<llvm::FunctionType>(map_type(FPT.pointee));
						auto *ci = builder.CreateCall(fty, calleeV, args, fty->getReturnType()->isVoidTy() ? "" : dst);
						if (!fty->getReturnType()->isVoidTy())
						{
							vmap[dst] = ci;
							vtypes[dst] = retTy;
						}
					}
					
					else if ((op == "fadd" || op == "fsub" || op == "fmul" || op == "fdiv") && il.size() == 5)
					{
						std::string dst = trimPct(symName(il[1]));
						TypeId ty;
						try
						{
							ty = tctx_.parse_type(il[2]);
						}
						catch (...)
						{
							continue;
						}
						auto *va = getVal(il[3]);
						auto *vb = getVal(il[4]);
						if (!va || !vb || dst.empty())
							continue;
						llvm::Value *r = nullptr;
						if (op == "fadd")
							r = builder.CreateFAdd(va, vb, dst);
						else if (op == "fsub")
							r = builder.CreateFSub(va, vb, dst);
						else if (op == "fmul")
							r = builder.CreateFMul(va, vb, dst);
						else
							r = builder.CreateFDiv(va, vb, dst);
						vmap[dst] = r;
						vtypes[dst] = ty;
					}
					else if (op == "fcmp" && il.size() == 7)
					{
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue;
						if (!std::holds_alternative<keyword>(il[3]->data))
							continue;
						std::string pred = symName(il[4]);
						auto *va = getVal(il[5]);
						auto *vb = getVal(il[6]);
						if (!va || !vb)
							continue;
						llvm::CmpInst::Predicate P = llvm::CmpInst::FCMP_OEQ;
						if (pred == "oeq")
							P = llvm::CmpInst::FCMP_OEQ;
						else if (pred == "one")
							P = llvm::CmpInst::FCMP_ONE;
						else if (pred == "olt")
							P = llvm::CmpInst::FCMP_OLT;
						else if (pred == "ogt")
							P = llvm::CmpInst::FCMP_OGT;
						else if (pred == "ole")
							P = llvm::CmpInst::FCMP_OLE;
						else if (pred == "oge")
							P = llvm::CmpInst::FCMP_OGE;
						else if (pred == "ord")
							P = llvm::CmpInst::FCMP_ORD;
						else if (pred == "uno")
							P = llvm::CmpInst::FCMP_UNO;
						else if (pred == "ueq")
							P = llvm::CmpInst::FCMP_UEQ;
						else if (pred == "une")
							P = llvm::CmpInst::FCMP_UNE;
						else if (pred == "ult")
							P = llvm::CmpInst::FCMP_ULT;
						else if (pred == "ugt")
							P = llvm::CmpInst::FCMP_UGT;
						else if (pred == "ule")
							P = llvm::CmpInst::FCMP_ULE;
						else if (pred == "uge")
							P = llvm::CmpInst::FCMP_UGE;
						else
							continue;
						auto *res = builder.CreateFCmp(P, va, vb, dst);
						vmap[dst] = res;
						vtypes[dst] = tctx_.get_base(BaseType::I1);
					}
					else if ((op == "eq" || op == "ne" || op == "lt" || op == "gt" || op == "le" || op == "ge") && il.size() == 5)
					{
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue;
						auto *va = getVal(il[3]);
						auto *vb = getVal(il[4]);
						if (!va || !vb)
							continue;
						llvm::CmpInst::Predicate P = llvm::CmpInst::ICMP_EQ;
						if (op == "eq")
							P = llvm::CmpInst::ICMP_EQ;
						else if (op == "ne")
							P = llvm::CmpInst::ICMP_NE;
						else if (op == "lt")
							P = llvm::CmpInst::ICMP_SLT;
						else if (op == "gt")
							P = llvm::CmpInst::ICMP_SGT;
						else if (op == "le")
							P = llvm::CmpInst::ICMP_SLE;
						else
							P = llvm::CmpInst::ICMP_SGE;
						auto *res = builder.CreateICmp(P, va, vb, dst);
						vmap[dst] = res;
						vtypes[dst] = tctx_.get_base(BaseType::I1);
						defNode[dst] = inst;
					}
					else if (op == "icmp" && il.size() == 7)
					{
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue;
						if (!std::holds_alternative<keyword>(il[3]->data))
							continue;
						std::string pred = symName(il[4]);
						auto *va = getVal(il[5]);
						auto *vb = getVal(il[6]);
						if (!va || !vb)
							continue;
						llvm::CmpInst::Predicate P = llvm::CmpInst::ICMP_EQ;
						if (pred == "eq")
							P = llvm::CmpInst::ICMP_EQ;
						else if (pred == "ne")
							P = llvm::CmpInst::ICMP_NE;
						else if (pred == "slt")
							P = llvm::CmpInst::ICMP_SLT;
						else if (pred == "sgt")
							P = llvm::CmpInst::ICMP_SGT;
						else if (pred == "sle")
							P = llvm::CmpInst::ICMP_SLE;
						else if (pred == "sge")
							P = llvm::CmpInst::ICMP_SGE;
						else if (pred == "ult")
							P = llvm::CmpInst::ICMP_ULT;
						else if (pred == "ugt")
							P = llvm::CmpInst::ICMP_UGT;
						else if (pred == "ule")
							P = llvm::CmpInst::ICMP_ULE;
						else if (pred == "uge")
							P = llvm::CmpInst::ICMP_UGE;
						else
							continue;
						auto *res = builder.CreateICmp(P, va, vb, dst);
						vmap[dst] = res;
						vtypes[dst] = tctx_.get_base(BaseType::I1);
					}
					else if ((op == "and" || op == "or" || op == "xor" || op == "shl" || op == "lshr" || op == "ashr") && il.size() == 5)
					{
						// Legacy bitwise/shift path removed (handled earlier by ir::core_ops)
					}
					else if ((op == "zext" || op == "sext" || op == "trunc" || op == "bitcast" || op == "sitofp" || op == "uitofp" || op == "fptosi" || op == "fptoui" || op == "ptrtoint" || op == "inttoptr") && il.size() == 4)
					{
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue;
						TypeId toTy;
						try
						{
							toTy = tctx_.parse_type(il[2]);
						}
						catch (...)
						{
							continue;
						}
						auto *srcV = getVal(il[3]);
						if (!srcV)
							continue;
						llvm::Value *castV = nullptr;
						llvm::Type *llvmTo = map_type(toTy);
						if (llvm::isa<llvm::Constant>(srcV))
						{
							auto *tmpAlloca = builder.CreateAlloca(srcV->getType(), nullptr, trimPct(symName(il[3])) + ".cst.tmp");
							builder.CreateStore(srcV, tmpAlloca);
							srcV = builder.CreateLoad(srcV->getType(), tmpAlloca, trimPct(symName(il[3])) + ".cst.load");
						}
						if (op == "zext")
							castV = builder.CreateZExt(srcV, llvmTo, dst);
						else if (op == "sext")
							castV = builder.CreateSExt(srcV, llvmTo, dst);
						else if (op == "trunc")
							castV = builder.CreateTrunc(srcV, llvmTo, dst);
						else if (op == "bitcast")
							castV = builder.CreateBitCast(srcV, llvmTo, dst);
						else if (op == "sitofp")
							castV = builder.CreateSIToFP(srcV, llvmTo, dst);
						else if (op == "uitofp")
							castV = builder.CreateUIToFP(srcV, llvmTo, dst);
						else if (op == "fptosi")
							castV = builder.CreateFPToSI(srcV, llvmTo, dst);
						else if (op == "fptoui")
							castV = builder.CreateFPToUI(srcV, llvmTo, dst);
						else if (op == "ptrtoint")
							castV = builder.CreatePtrToInt(srcV, llvmTo, dst);
						else if (op == "inttoptr")
							castV = builder.CreateIntToPtr(srcV, llvmTo, dst);
						if (castV)
						{
							vmap[dst] = castV;
							vtypes[dst] = toTy;
						}
					}
					else if (op == "as" && il.size() == 4)
					{ // (as %dst <type> %initOrLiteral)
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue;
						TypeId ty;
						try
						{
							ty = tctx_.parse_type(il[2]);
						}
						catch (...)
						{
							continue;
						}
						llvm::Value *initV = nullptr;
						llvm::Type *lty = map_type(ty);
						// initializer can be a symbol or a literal
						if (std::holds_alternative<symbol>(il[3]->data))
						{
							// Resolve directly from current SSA map to avoid alias recursion during initialization
							std::string initName = trimPct(symName(il[3]));
							if (!initName.empty())
							{
								if (auto itInit = vmap.find(initName); itInit != vmap.end())
								{
									initV = itInit->second;
								}
							}
						}
						else if (std::holds_alternative<int64_t>(il[3]->data))
						{
							initV = llvm::ConstantInt::get(lty, (uint64_t)std::get<int64_t>(il[3]->data), true);
						}
						else if (std::holds_alternative<double>(il[3]->data))
						{
							initV = llvm::ConstantFP::get(lty, std::get<double>(il[3]->data));
						}
						if (!initV)
						{
							// Fallback to undef/null of the declared type if initializer isn't resolvable
							initV = llvm::UndefValue::get(lty);
						}
						auto *slot = ensureSlot(dst, ty, /*initFromCurrent=*/false);
						builder.CreateStore(initV, slot);
						vtypes[dst] = ty;
						// Also expose a loaded SSA value for %dst so paths that consult vmap directly see the current value.
						// getVal will prefer the slot, but a few ops read vmap directly (e.g., some legacy paths).
						auto *loaded = builder.CreateLoad(map_type(ty), slot, dst);
						vmap[dst] = loaded;
						// Now register alias so future reads of the initializer symbol go through this variable's slot
						if (std::holds_alternative<symbol>(il[3]->data))
						{
							std::string initName = trimPct(symName(il[3]));
							if (!initName.empty())
							{
								initAlias[initName] = dst;
								// Also redirect the initializer symbol's SSA to the current loaded value
								vmap[initName] = loaded;
								vtypes[initName] = ty;
							}
						}
						// Optionally emit debug info for the local variable
						if (enableDebugInfo && F->getSubprogram())
						{
							auto *lv = debug_manager_->DIB->createAutoVariable(F->getSubprogram(), dst, debug_manager_->DI_File, builder.getCurrentDebugLocation() ? builder.getCurrentDebugLocation()->getLine() : F->getSubprogram()->getLine(), debug_manager_->diTypeOf(ty));
							auto *expr = debug_manager_->DIB->createExpression();
							(void)debug_manager_->DIB->insertDeclare(slot, lv, expr, builder.getCurrentDebugLocation(), builder.GetInsertBlock());
						}
					}
					
					else if (op == "phi" && il.size() == 4)
					{
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue;
						TypeId ty;
						try
						{
							ty = tctx_.parse_type(il[2]);
						}
						catch (...)
						{
							continue;
						}
						if (!std::holds_alternative<vector_t>(il[3]->data))
							continue;
						std::vector<std::pair<std::string, std::string>> incomings;
						for (auto &inc : std::get<vector_t>(il[3]->data).elems)
						{
							if (!inc || !std::holds_alternative<list>(inc->data))
								continue;
							auto &pl = std::get<list>(inc->data).elems;
							if (pl.size() != 2)
								continue;
							std::string val = trimPct(symName(pl[0]));
							std::string label = symName(pl[1]);
							if (!val.empty() && !label.empty())
								incomings.emplace_back(val, label);
						}
						pendingPhis.push_back(PendingPhi{dst, ty, std::move(incomings), builder.GetInsertBlock()});
					}

					else if (op == "panic" && il.size() == 1)
					{ // (panic) -> abort or unwind depending on EDN_PANIC
						if (panicUnwind && enableEHItanium && selectedPersonality)
						{
							// Itanium: emit invoke to __cxa_throw so exceptional edge routes to active landingpad
							auto *i8Ty = llvm::Type::getInt8Ty(*llctx_);
							auto *i8Ptr = llvm::PointerType::getUnqual(i8Ty);
							auto *throwFTy = llvm::FunctionType::get(llvm::Type::getVoidTy(*llctx_), {i8Ptr, i8Ptr, i8Ptr}, /*isVarArg*/ false);
							auto throwCallee = module_->getOrInsertFunction("__cxa_throw", throwFTy);
							llvm::Value *nullp = llvm::ConstantPointerNull::get(i8Ptr);
							// Determine exceptional target
							llvm::BasicBlock *exTarget = nullptr;
							if (!itnExceptTargetStack.empty())
							{
								exTarget = itnExceptTargetStack.back();
							}
							else
							{
								exTarget = edn::ir::exceptions::create_panic_cleanup_landingpad(F, builder);
							}
							// Normal dest (never taken) just contains unreachable
							auto *unwCont = llvm::BasicBlock::Create(*llctx_, "panic.cont", F);
							builder.CreateInvoke(throwFTy, throwCallee.getCallee(), unwCont, exTarget, {nullp, nullp, nullp});
							llvm::IRBuilder<> nb(unwCont);
							if (enableDebugInfo && F->getSubprogram())
								nb.SetCurrentDebugLocation(builder.getCurrentDebugLocation());
							nb.CreateUnreachable();
							// Current block is terminated; subsequent emission will continue in other blocks (e.g., handler/cont)
						}
						else if (panicUnwind && enableEHSEH && selectedPersonality)
						{
							// SEH: emit invoke to RaiseException so unwind flows into catchswitch/catchpad if inside try
							auto *i32 = llvm::Type::getInt32Ty(*llctx_);
							auto *i8 = llvm::Type::getInt8Ty(*llctx_);
							auto *i8ptr = llvm::PointerType::getUnqual(i8);
							auto *raiseFTy = llvm::FunctionType::get(llvm::Type::getVoidTy(*llctx_), {i32, i32, i32, i8ptr}, false);
							auto raiseCallee = module_->getOrInsertFunction("RaiseException", raiseFTy);
							auto *code = llvm::ConstantInt::get(i32, 0xE0ED0001);
							auto *flags = llvm::ConstantInt::get(i32, 1);
							auto *nargs = llvm::ConstantInt::get(i32, 0);
							llvm::Value *argsPtr = llvm::ConstantPointerNull::get(i8ptr);
							llvm::BasicBlock *exTarget = sehExceptTargetStack.empty() ? nullptr : sehExceptTargetStack.back();
							if (!exTarget)
							{
								// Fallback to shared cleanup funclet
								sehCleanupBB = edn::ir::exceptions::ensure_seh_cleanup(F, builder, sehCleanupBB);
								exTarget = sehCleanupBB;
							}
							auto *unwCont = llvm::BasicBlock::Create(*llctx_, "panic.cont", F);
							builder.CreateInvoke(raiseFTy, raiseCallee.getCallee(), unwCont, exTarget, {code, flags, nargs, argsPtr});
							llvm::IRBuilder<> nb(unwCont);
							if (enableDebugInfo && F->getSubprogram())
								nb.SetCurrentDebugLocation(builder.getCurrentDebugLocation());
							nb.CreateUnreachable();
						}
						else
						{
							// Default: abort via llvm.trap for deterministic tests
							auto callee = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::trap);
							builder.CreateCall(callee);
							builder.CreateUnreachable();
						}
						continue;
					}
					// --- M4.6 Coroutines (minimal) ---
					else if (op == "coro-begin" && il.size() == 2)
					{ // (coro-begin %hdl) -> ptr handle
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue;
						auto *i8 = llvm::Type::getInt8Ty(*llctx_);
						auto *i8p = llvm::PointerType::getUnqual(i8);
						llvm::Value *hdl = nullptr;
						if (enableCoro)
						{
							// Proper minimal sequence: id + begin
							auto idDecl = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::coro_id);
							auto beginDecl = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::coro_begin);
							auto *i32 = llvm::Type::getInt32Ty(*llctx_);
							llvm::Value *alignV = llvm::ConstantInt::get(i32, 0); // default alignment semantics
							llvm::Value *nullp = llvm::ConstantPointerNull::get(i8p);
							// token %id = llvm.coro.id(i32 0, ptr null, ptr null, ptr null)
							auto *idTok = builder.CreateCall(idDecl, {alignV, nullp, nullp, nullp}, "coro.id");
							lastCoroIdTok = idTok;
							// ptr %hdl = llvm.coro.begin(token %id, ptr null)
							hdl = builder.CreateCall(beginDecl, {idTok, nullp}, dst);
						}
						else
						{
							hdl = llvm::ConstantPointerNull::get(i8p);
						}
						vmap[dst] = hdl;
						vtypes[dst] = tctx_.get_pointer(tctx_.get_base(BaseType::I8));
					}
					else if (op == "coro-suspend" && il.size() == 3)
					{ // (coro-suspend %st %hdlOrTok) -> i8 status
						std::string dst = trimPct(symName(il[1]));
						std::string h = trimPct(symName(il[2]));
						if (dst.empty() || h.empty() || !vmap.count(h))
							continue;
						llvm::Value *stV = nullptr;
						if (enableCoro)
						{
							auto suspendDecl = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::coro_suspend);
							// If operand is a token, pass it; otherwise use token none.
							auto *i1 = llvm::Type::getInt1Ty(*llctx_);
							auto *tokArg = vmap[h]->getType()->isTokenTy() ? vmap[h] : (llvm::Value *)llvm::ConstantTokenNone::get(*llctx_);
							llvm::Value *isFinal = llvm::ConstantInt::getFalse(i1);
							stV = builder.CreateCall(suspendDecl, {tokArg, isFinal}, dst);
						}
						else
						{
							stV = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*llctx_), 0);
						}
						vmap[dst] = stV;
						vtypes[dst] = tctx_.get_base(BaseType::I8);
					}
					else if (op == "coro-final-suspend" && il.size() == 3)
					{ // (coro-final-suspend %st %hdlOrTok) -> i8 status
						std::string dst = trimPct(symName(il[1]));
						std::string h = trimPct(symName(il[2]));
						if (dst.empty() || h.empty() || !vmap.count(h))
							continue;
						llvm::Value *stV = nullptr;
						if (enableCoro)
						{
							auto suspendDecl = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::coro_suspend);
							auto *i1 = llvm::Type::getInt1Ty(*llctx_);
							auto *tokArg = vmap[h]->getType()->isTokenTy() ? vmap[h] : (llvm::Value *)llvm::ConstantTokenNone::get(*llctx_);
							llvm::Value *isFinal = llvm::ConstantInt::getTrue(i1);
							stV = builder.CreateCall(suspendDecl, {tokArg, isFinal}, dst);
						}
						else
						{
							stV = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*llctx_), 0);
						}
						vmap[dst] = stV;
						vtypes[dst] = tctx_.get_base(BaseType::I8);
					}
					else if (op == "coro-save" && il.size() == 3)
					{ // (coro-save %sv %hdl) -> token
						std::string dst = trimPct(symName(il[1]));
						std::string h = trimPct(symName(il[2]));
						if (dst.empty() || h.empty() || !vmap.count(h))
							continue;
						llvm::Value *tokV = nullptr;
						if (enableCoro)
						{
							auto saveDecl = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::coro_save);
							tokV = builder.CreateCall(saveDecl, {vmap[h]}, dst);
						}
						else
						{
							// Represent as token none when disabled
							tokV = llvm::ConstantTokenNone::get(*llctx_);
						}
						vmap[dst] = tokV; // type map uses a placeholder; TC tracks it as i8
					}
					else if (op == "coro-id" && il.size() == 2)
					{ // (coro-id %cid) -> token (bound for later ops)
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue;
						if (lastCoroIdTok)
						{
							vmap[dst] = lastCoroIdTok;
						}
					}
					else if (op == "coro-size" && il.size() == 2)
					{ // (coro-size %sz) -> i64
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue;
						llvm::Value *szV = nullptr;
						if (enableCoro)
						{
							auto *i64 = llvm::Type::getInt64Ty(*llctx_);
							auto sizeDecl = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::coro_size, {i64});
							szV = builder.CreateCall(sizeDecl, {}, dst);
						}
						else
						{
							szV = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llctx_), 0);
						}
						vmap[dst] = szV;
						vtypes[dst] = tctx_.get_base(BaseType::I64);
					}
					else if (op == "coro-alloc" && il.size() == 3)
					{ // (coro-alloc %need %cid) -> i1
						std::string dst = trimPct(symName(il[1]));
						std::string cid = trimPct(symName(il[2]));
						if (dst.empty() || cid.empty() || !vmap.count(cid))
							continue;
						llvm::Value *needV = nullptr;
						if (enableCoro)
						{
							auto allocDecl = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::coro_alloc);
							needV = builder.CreateCall(allocDecl, {vmap[cid]}, dst);
						}
						else
						{
							needV = llvm::ConstantInt::getFalse(llvm::Type::getInt1Ty(*llctx_));
						}
						vmap[dst] = needV;
						vtypes[dst] = tctx_.get_base(BaseType::I1);
					}
					else if (op == "coro-free" && il.size() == 4)
					{ // (coro-free %mem %cid %hdl) -> ptr
						std::string dst = trimPct(symName(il[1]));
						std::string cid = trimPct(symName(il[2]));
						std::string h = trimPct(symName(il[3]));
						if (dst.empty() || cid.empty() || h.empty() || !vmap.count(cid) || !vmap.count(h))
							continue;
						llvm::Value *memV = nullptr;
						if (enableCoro)
						{
							auto freeDecl = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::coro_free);
							auto *i8 = llvm::Type::getInt8Ty(*llctx_);
							auto *i8p = llvm::PointerType::getUnqual(i8);
							auto *hdlCast = builder.CreateBitCast(vmap[h], i8p);
							memV = builder.CreateCall(freeDecl, {vmap[cid], hdlCast}, dst);
						}
						else
						{
							memV = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llctx_)));
						}
						vmap[dst] = memV;
						vtypes[dst] = tctx_.get_pointer(tctx_.get_base(BaseType::I8));
					}
					else if (op == "coro-promise" && il.size() == 3)
					{ // (coro-promise %p %hdl) -> ptr
						std::string dst = trimPct(symName(il[1]));
						std::string h = trimPct(symName(il[2]));
						if (dst.empty() || h.empty() || !vmap.count(h))
							continue;
						llvm::Value *pV = nullptr;
						if (enableCoro)
						{
							auto promDecl = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::coro_promise);
							auto *i32 = llvm::Type::getInt32Ty(*llctx_);
							auto *i1 = llvm::Type::getInt1Ty(*llctx_);
							auto *i8 = llvm::Type::getInt8Ty(*llctx_);
							auto *i8p = llvm::PointerType::getUnqual(i8);
							llvm::Value *alignZero = llvm::ConstantInt::get(i32, 0);
							llvm::Value *fromPromise = llvm::ConstantInt::getFalse(i1);
							// coro.promise(ptr handle, i32 align, i1 fromPromise) -> ptr
							auto *hdlCast = builder.CreateBitCast(vmap[h], i8p);
							pV = builder.CreateCall(promDecl, {hdlCast, alignZero, fromPromise}, dst);
						}
						else
						{
							pV = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llctx_)));
						}
						vmap[dst] = pV;
						vtypes[dst] = tctx_.get_pointer(tctx_.get_base(BaseType::I8));
					}
					else if (op == "coro-resume" && il.size() == 2)
					{ // (coro-resume %hdl)
						std::string h = trimPct(symName(il[1]));
						if (h.empty() || !vmap.count(h))
							continue;
						if (enableCoro)
						{
							auto resumeDecl = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::coro_resume);
							auto *i8 = llvm::Type::getInt8Ty(*llctx_);
							auto *i8p = llvm::PointerType::getUnqual(i8);
							auto *hdlCast = builder.CreateBitCast(vmap[h], i8p);
							builder.CreateCall(resumeDecl, {hdlCast});
						}
					}
					else if (op == "coro-destroy" && il.size() == 2)
					{ // (coro-destroy %hdl)
						std::string h = trimPct(symName(il[1]));
						if (h.empty() || !vmap.count(h))
							continue;
						if (enableCoro)
						{
							auto destroyDecl = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::coro_destroy);
							auto *i8 = llvm::Type::getInt8Ty(*llctx_);
							auto *i8p = llvm::PointerType::getUnqual(i8);
							auto *hdlCast = builder.CreateBitCast(vmap[h], i8p);
							builder.CreateCall(destroyDecl, {hdlCast});
						}
					}
					else if (op == "coro-done" && il.size() == 3)
					{ // (coro-done %d %hdl) -> i1
						std::string dst = trimPct(symName(il[1]));
						std::string h = trimPct(symName(il[2]));
						if (dst.empty() || h.empty() || !vmap.count(h))
							continue;
						llvm::Value *dV = nullptr;
						if (enableCoro)
						{
							auto doneDecl = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::coro_done);
							auto *i8 = llvm::Type::getInt8Ty(*llctx_);
							auto *i8p = llvm::PointerType::getUnqual(i8);
							auto *hdlCast = builder.CreateBitCast(vmap[h], i8p);
							dV = builder.CreateCall(doneDecl, {hdlCast}, dst);
						}
						else
						{
							dV = llvm::ConstantInt::getFalse(llvm::Type::getInt1Ty(*llctx_));
						}
						vmap[dst] = dV;
						vtypes[dst] = tctx_.get_base(BaseType::I1);
					}
					else if (op == "coro-end" && il.size() == 2)
					{ // (coro-end %hdl)
						std::string h = trimPct(symName(il[1]));
						if (h.empty() || !vmap.count(h))
							continue;
						if (enableCoro)
						{
							auto endDecl = llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::coro_end);
							auto *i1 = llvm::Type::getInt1Ty(*llctx_);
							llvm::Value *u0 = llvm::ConstantInt::getFalse(i1);
							auto *tokNone = llvm::ConstantTokenNone::get(*llctx_);
							(void)builder.CreateCall(endDecl, {vmap[h], u0, tokNone}, "coro.end");
						}
					}
					else if (op == "call" && il.size() >= 4)
					{
						std::string dst = trimPct(symName(il[1]));
						TypeId retTy;
						try
						{
							retTy = tctx_.parse_type(il[2]);
						}
						catch (...)
						{
							continue;
						}
						std::string callee = symName(il[3]);
						if (callee.empty())
							continue;
						llvm::Function *CF = module_->getFunction(callee);
						if (!CF)
						{ // create forward decl; attempt to detect variadic via type cache by matching existing function type if any
							// Scan original module AST for function header to recover signature & variadic flag
							bool foundHeader = false;
							std::vector<llvm::Type *> headerParamLL;
							bool headerVariadic = false; // fallback param list from current call if header parse fails
							for (size_t ti = 1; ti < top.size() && !foundHeader; ++ti)
							{
								auto &fnNode = top[ti];
								if (!fnNode || !std::holds_alternative<list>(fnNode->data))
									continue;
								auto &fl2 = std::get<list>(fnNode->data).elems;
								if (fl2.empty() || !std::holds_alternative<symbol>(fl2[0]->data) || std::get<symbol>(fl2[0]->data).name != "fn")
									continue;
								std::string fname2;
								TypeId retHeader = tctx_.get_base(BaseType::Void);
								std::vector<TypeId> paramTypeIds;
								bool varargFlag = false;
								for (size_t j = 1; j < fl2.size(); ++j)
								{
									if (!fl2[j] || !std::holds_alternative<keyword>(fl2[j]->data))
										break;
									std::string kw = std::get<keyword>(fl2[j]->data).name;
									if (++j >= fl2.size())
										break;
									auto val = fl2[j];
									if (kw == "name")
									{
										fname2 = symName(val);
									}
									else if (kw == "ret")
									{
										try
										{
											retHeader = tctx_.parse_type(val);
										}
										catch (...)
										{
											retHeader = tctx_.get_base(BaseType::Void);
										}
									}
									else if (kw == "params" && val && std::holds_alternative<vector_t>(val->data))
									{
										for (auto &p : std::get<vector_t>(val->data).elems)
										{
											if (!p || !std::holds_alternative<list>(p->data))
												continue;
											auto &pl = std::get<list>(p->data).elems;
											if (pl.size() == 3 && std::holds_alternative<symbol>(pl[0]->data) && std::get<symbol>(pl[0]->data).name == "param")
											{
												try
												{
													TypeId pty = tctx_.parse_type(pl[1]);
													paramTypeIds.push_back(pty);
												}
												catch (...)
												{
												}
											}
										}
									}
									else if (kw == "vararg")
									{
										if (val && std::holds_alternative<bool>(val->data))
											varargFlag = std::get<bool>(val->data);
									}
								}
								if (fname2 == callee)
								{ // build FunctionType using header info
									for (auto pid : paramTypeIds)
										headerParamLL.push_back(map_type(pid));
									headerVariadic = varargFlag; // override return type from header (compiler may mismatch annotation; rely on checker)
									CF = llvm::Function::Create(llvm::FunctionType::get(map_type(retHeader), headerParamLL, headerVariadic), llvm::Function::ExternalLinkage, callee, module_.get());
									foundHeader = true;
									break;
								}
							}
							if (!foundHeader)
							{ // fallback: use current arg variable types (best effort, assume non-variadic)
								std::vector<llvm::Type *> argLTys;
								argLTys.reserve(il.size() - 4);
								for (size_t ai = 4; ai < il.size(); ++ai)
								{
									std::string av = trimPct(symName(il[ai]));
									if (av.empty() || !vtypes.count(av))
									{
										argLTys.clear();
										break;
									}
									argLTys.push_back(map_type(vtypes[av]));
								}
								CF = llvm::Function::Create(llvm::FunctionType::get(map_type(retTy), argLTys, false), llvm::Function::ExternalLinkage, callee, module_.get());
							}
						}
						std::vector<llvm::Value *> args;
						for (size_t ai = 4; ai < il.size(); ++ai)
						{
							auto *v = getVal(il[ai]);
							if (!v)
							{
								args.clear();
								break;
							}
							args.push_back(v);
						}
						if (args.size() + 4 != il.size())
							continue;
						if (enableEHItanium && selectedPersonality)
						{
							// Emit invoke with landingpad on exceptional edge; use shared try-target if present.
							auto *contBB = llvm::BasicBlock::Create(*llctx_, "invoke.cont." + std::to_string(cfCounter++), F);
							llvm::BasicBlock *exTarget = nullptr;
							if (!itnExceptTargetStack.empty())
							{
								exTarget = itnExceptTargetStack.back();
							}
							else
							{
								exTarget = edn::ir::exceptions::create_panic_cleanup_landingpad(F, builder);
							}
							llvm::InvokeInst *inv = builder.CreateInvoke(CF, contBB, exTarget, args, CF->getReturnType()->isVoidTy() ? "" : dst);
							// Normal continuation
							builder.SetInsertPoint(contBB);
							if (!CF->getReturnType()->isVoidTy())
							{
								vmap[dst] = inv;
								vtypes[dst] = retTy;
							}
						}
						else if (enableEHSEH && selectedPersonality)
						{
							// Windows SEH: emit invoke with a single shared cleanup funclet per function.
							auto *contBB = llvm::BasicBlock::Create(*llctx_, "invoke.cont." + std::to_string(cfCounter++), F);
							sehCleanupBB = edn::ir::exceptions::ensure_seh_cleanup(F, builder, sehCleanupBB);
							llvm::BasicBlock *exTarget = sehExceptTargetStack.empty() ? sehCleanupBB : sehExceptTargetStack.back();
							llvm::InvokeInst *inv = builder.CreateInvoke(CF, contBB, exTarget, args, CF->getReturnType()->isVoidTy() ? "" : dst);
							// Normal continuation
							builder.SetInsertPoint(contBB);
							if (!CF->getReturnType()->isVoidTy())
							{
								vmap[dst] = inv;
								vtypes[dst] = retTy;
							}
						}
						else
						{
							auto *callInst = builder.CreateCall(CF, args, CF->getReturnType()->isVoidTy() ? "" : dst);
							if (!CF->getReturnType()->isVoidTy())
							{
								vmap[dst] = callInst;
								vtypes[dst] = retTy;
							}
						}
					}
					else if (op == "va-start" && il.size() == 2)
					{ // allocate simple i8* zero for va_list handle
						std::string ap = trimPct(symName(il[1]));
						if (ap.empty())
							continue; // represent va_list as i8* pointer to first extra arg placeholder (not implemented)
						llvm::Value *nullp = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*llctx_)));
						vmap[ap] = nullp; // already attached type by checker
					}
					else if (op == "va-arg" && il.size() == 4)
					{
						std::string dst = trimPct(symName(il[1]));
						if (dst.empty())
							continue; // produce undef of requested type (checker ensured types)
						TypeId ty;
						try
						{
							ty = tctx_.parse_type(il[2]);
						}
						catch (...)
						{
							continue;
						}
						llvm::Type *lty = map_type(ty);
						llvm::Value *uv = llvm::UndefValue::get(lty);
						vmap[dst] = uv;
						vtypes[dst] = ty;
					}
					else if (op == "va-end" && il.size() == 2)
					{ /* no-op */
					}
					else if (op == "try")
					{ // (try :body [ ... ] :catch [ ... ])  -- Itanium & SEH (catch-all minimal)
						// Parse sections
						node_ptr bodyNode = nullptr, catchNode = nullptr;
						for (size_t i = 1; i < il.size(); ++i)
						{
							if (!il[i] || !std::holds_alternative<keyword>(il[i]->data))
								break;
							std::string kw = std::get<keyword>(il[i]->data).name;
							if (++i >= il.size())
								break;
							auto val = il[i];
							if (kw == "body" && val && std::holds_alternative<vector_t>(val->data))
								bodyNode = val;
							else if (kw == "catch" && val && std::holds_alternative<vector_t>(val->data))
								catchNode = val;
						}
						if (!bodyNode || !catchNode)
						{
							continue;
						}
						// Create blocks
						auto *bodyBB = llvm::BasicBlock::Create(*llctx_, "try.body." + std::to_string(cfCounter++), F);
						auto *contBB = llvm::BasicBlock::Create(*llctx_, "try.end." + std::to_string(cfCounter++), F);
						if (enableEHSEH && selectedPersonality)
						{
							// SEH path: catchswitch/catchpad/catchret to handler
							auto *catchHandlerBB = llvm::BasicBlock::Create(*llctx_, "try.handler." + std::to_string(cfCounter++), F);
							if (!builder.GetInsertBlock()->getTerminator())
								builder.CreateBr(bodyBB);
							auto scf = edn::ir::exceptions::create_seh_catch_scaffold(F, builder, contBB);
							// After creating the catchpad, branch to handler
							builder.SetInsertPoint(scf.catchPadBB);
							builder.CreateCatchRet(scf.catchPad, catchHandlerBB);
							// Body with SEH exception target
							builder.SetInsertPoint(bodyBB);
							sehExceptTargetStack.push_back(scf.dispatchBB);
							emit_ref(std::get<vector_t>(bodyNode->data).elems, emit_ref);
							sehExceptTargetStack.pop_back();
							if (!builder.GetInsertBlock()->getTerminator())
								builder.CreateBr(contBB);
							// Handler body
							builder.SetInsertPoint(catchHandlerBB);
							emit_ref(std::get<vector_t>(catchNode->data).elems, emit_ref);
							if (!builder.GetInsertBlock()->getTerminator())
								builder.CreateBr(contBB);
							builder.SetInsertPoint(contBB);
						}
						else if (enableEHItanium && selectedPersonality)
						{
							// Itanium path: landingpad catch-all -> handler
							auto *handlerBB = llvm::BasicBlock::Create(*llctx_, "try.handler." + std::to_string(cfCounter++), F);
							if (!builder.GetInsertBlock()->getTerminator())
								builder.CreateBr(bodyBB);
							// Landingpad with catch-all
							auto *lpadBB = edn::ir::exceptions::create_catch_all_landingpad(F, builder, handlerBB);
							// Body with Itanium exception target
							builder.SetInsertPoint(bodyBB);
							itnExceptTargetStack.push_back(lpadBB);
							emit_ref(std::get<vector_t>(bodyNode->data).elems, emit_ref);
							itnExceptTargetStack.pop_back();
							if (!builder.GetInsertBlock()->getTerminator())
								builder.CreateBr(contBB);
							// Handler body
							builder.SetInsertPoint(handlerBB);
							emit_ref(std::get<vector_t>(catchNode->data).elems, emit_ref);
							if (!builder.GetInsertBlock()->getTerminator())
								builder.CreateBr(contBB);
							builder.SetInsertPoint(contBB);
						}
						else
						{
							// No EH enabled: just emit body then catch (no-op structural)
							if (!builder.GetInsertBlock()->getTerminator())
								builder.CreateBr(bodyBB);
							builder.SetInsertPoint(bodyBB);
							emit_ref(std::get<vector_t>(bodyNode->data).elems, emit_ref);
							if (!builder.GetInsertBlock()->getTerminator())
								builder.CreateBr(contBB);
							builder.SetInsertPoint(contBB);
						}
					}
					
					else if (op == "ret" && il.size() == 3)
					{
						std::string rv = trimPct(symName(il[2]));
						if (!rv.empty())
						{
							// If value is slot-backed, load from slot for return
							auto sIt = varSlots.find(rv);
							if (sIt != varSlots.end())
							{
								auto tIt = vtypes.find(rv);
								if (tIt != vtypes.end())
								{
									auto *lv = builder.CreateLoad(map_type(tIt->second), sIt->second, rv + ".ret");
									builder.CreateRet(lv);
									functionDone = true;
									return;
								}
							}
							if (vmap.count(rv))
							{
								builder.CreateRet(vmap[rv]);
								functionDone = true;
								return;
							}
						}
						if (fty->getReturnType()->isVoidTy())
							builder.CreateRetVoid();
						else
							builder.CreateRet(llvm::Constant::getNullValue(fty->getReturnType()));
						functionDone = true;
						return;
					}
				}
			};
			emit_list(std::get<vector_t>(body->data).elems, emit_list);
			// Realize pending phi nodes now that all basic blocks exist.
			for (auto &pp : pendingPhis)
			{
				llvm::BasicBlock *insertBB = pp.insertBlock;
				if (!insertBB)
					continue;
				llvm::IRBuilder<> tmpBuilder(insertBB, insertBB->begin());
				llvm::PHINode *phi = tmpBuilder.CreatePHI(map_type(pp.ty), (unsigned)pp.incomings.size(), pp.dst);
				vmap[pp.dst] = phi;
				vtypes[pp.dst] = pp.ty;
				for (auto &inc : pp.incomings)
				{
					auto itv = vmap.find(inc.first);
					if (itv == vmap.end())
						continue; // find basic block by exact name
					llvm::BasicBlock *pred = nullptr;
					for (auto &bb : *F)
					{
						if (bb.getName() == inc.second)
						{
							pred = &bb;
							break;
						}
					}
					if (pred)
						phi->addIncoming(itv->second, pred);
				}
			}
			if (!entry->getTerminator())
			{
				if (fty->getReturnType()->isVoidTy())
					builder.CreateRetVoid();
				else
					builder.CreateRet(llvm::Constant::getNullValue(fty->getReturnType()));
			}
		}
		// Finalize DI after all functions are emitted
		if (enableDebugInfo && debug_manager_->DIB)
		{
			debug_manager_->DIB->finalize();
		}
		// Optional: run an optimization pipeline if enabled. EDN_PASS_PIPELINE overrides presets.
		if (const char *enable = std::getenv("EDN_ENABLE_PASSES"); enable && std::string(enable) == "1")
		{
			// Setup PassBuilder and analysis managers once
			llvm::PassBuilder PB;
			llvm::LoopAnalysisManager LAM;
			llvm::FunctionAnalysisManager FAM;
			llvm::CGSCCAnalysisManager CGAM;
			llvm::ModuleAnalysisManager MAM;
			PB.registerModuleAnalyses(MAM);
			PB.registerCGSCCAnalyses(CGAM);
			PB.registerFunctionAnalyses(FAM);
			PB.registerLoopAnalyses(LAM);
			PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

			// If a textual pipeline is provided, try to parse and run it.
			if (const char *pipeline = std::getenv("EDN_PASS_PIPELINE"); pipeline && *pipeline)
			{
				llvm::ModulePassManager MPM;
				// parsePassPipeline returns llvm::Error in recent LLVM; consume on failure and fall back.
				if (auto Err = PB.parsePassPipeline(MPM, pipeline))
				{
					llvm::consumeError(std::move(Err));
				}
				else
				{
					// Optional IR verification before running pipeline (for debugging)
					if (const char *v = std::getenv("EDN_VERIFY_IR"); v && std::string(v) == "1")
					{
						if (llvm::verifyModule(*module_, &llvm::errs()))
						{
							llvm::errs() << "[edn] IR verify failed before custom pipeline\n";
						}
					}
					MPM.run(*module_, MAM);
					if (const char *v2 = std::getenv("EDN_VERIFY_IR"); v2 && std::string(v2) == "1")
					{
						if (llvm::verifyModule(*module_, &llvm::errs()))
						{
							llvm::errs() << "[edn] IR verify failed after custom pipeline\n";
						}
					}
					return module_.get();
				}
			}

			// No custom pipeline or parse failed: use presets via EDN_OPT_LEVEL (0/1/2/3)
			llvm::OptimizationLevel optLevel = llvm::OptimizationLevel::O1; // default
			if (const char *lvl = std::getenv("EDN_OPT_LEVEL"); lvl && *lvl)
			{
				std::string s = lvl;
				for (char &c : s)
					c = (char)tolower((unsigned char)c);
				if (s == "0" || s == "o0")
					optLevel = llvm::OptimizationLevel::O0;
				else if (s == "2" || s == "o2")
					optLevel = llvm::OptimizationLevel::O2;
				else if (s == "3" || s == "o3")
					optLevel = llvm::OptimizationLevel::O3;
				else
					optLevel = llvm::OptimizationLevel::O1;
			}
			if (optLevel != llvm::OptimizationLevel::O0)
			{
				llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(optLevel);
				if (const char *v = std::getenv("EDN_VERIFY_IR"); v && std::string(v) == "1")
				{
					if (llvm::verifyModule(*module_, &llvm::errs()))
					{
						llvm::errs() << "[edn] IR verify failed before preset pipeline\n";
					}
				}
				MPM.run(*module_, MAM);
				if (const char *v2 = std::getenv("EDN_VERIFY_IR"); v2 && std::string(v2) == "1")
				{
					if (llvm::verifyModule(*module_, &llvm::errs()))
					{
						llvm::errs() << "[edn] IR verify failed after preset pipeline\n";
					}
				}
			}
			// For O0, do nothing (preserve IR for debugging)
		}
		return module_.get();
	}

	llvm::orc::ThreadSafeModule IREmitter::toThreadSafeModule() { return llvm::orc::ThreadSafeModule(std::move(module_), std::move(llctx_)); }

} // namespace edn
