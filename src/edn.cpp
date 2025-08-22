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
#include "edn/ir/compare_ops.hpp"
#include "edn/ir/cast_ops.hpp"
#include "edn/ir/pointer_func_ops.hpp"
#include "edn/ir/const_ops.hpp"
#include "edn/ir/variable_ops.hpp"
#include "edn/ir/phi_ops.hpp"
#include "edn/ir/coro_ops.hpp"
#include "edn/ir/exception_ops.hpp"
#include "edn/ir/debug_pipeline.hpp"
#include "edn/ir/di.hpp"

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
			// Function-level debug info (skeleton via di module)
			if (enableDebugInfo)
			{
				unsigned fnLine = edn::line(*fl[0]);
				edn::ir::di::attach_function_debug(*debug_manager_, *F, fname, retTy, params, fnLine);
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
			if (enableDebugInfo)
			{
				edn::ir::di::emit_parameter_debug(*debug_manager_, *F, builder, vtypes);
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
			// Collected phi specifications for deferred realization
			std::vector<edn::ir::phi_ops::PendingPhi> pendingPhis;
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
						// --- Dispatch extracted compare ops (int simple, icmp, fcmp) ---
						if (edn::ir::compare_ops::handle_int_simple(S, il, inst) ||
							edn::ir::compare_ops::handle_icmp(S, il) ||
							edn::ir::compare_ops::handle_fcmp(S, il))
						{
							continue; // handled by modular compare_ops
						}
						// --- Dispatch extracted cast ops ---
						if (edn::ir::cast_ops::handle(S, il))
						{
							continue; // handled by modular cast_ops
						}
						// --- Dispatch extracted pointer/function ops (addr, deref, fnptr, call-indirect) ---
						if (edn::ir::pointer_func_ops::handle_addr(S, il, ensureSlot) ||
							edn::ir::pointer_func_ops::handle_deref(S, il) ||
							edn::ir::pointer_func_ops::handle_fnptr(S, il) ||
							edn::ir::pointer_func_ops::handle_call_indirect(S, il))
						{
							continue; // handled by modular pointer_func_ops
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
							edn::ir::sum_ops::handle_sum_get(S, il, sum_variant_tag_, sum_variant_field_types_) ||
							edn::ir::variable_ops::handle_as(S, il, ensureSlot, initAlias, enableDebugInfo, F, debug_manager_))
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
					// Lightweight state wrapper for standalone simple ops (const)
					edn::ir::builder::State S_local_for_dispatch{builder, *llctx_, *module_, tctx_,
																 [&](TypeId id)
																 { return map_type(id); }, vmap, vtypes, varSlots, initAlias, defNode, debug_manager_};
					if (edn::ir::const_ops::handle(S_local_for_dispatch, il))
					{
						continue;
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
						// Legacy fcmp path removed (handled by ir::compare_ops)
					}
					else if ((op == "eq" || op == "ne" || op == "lt" || op == "gt" || op == "le" || op == "ge") && il.size() == 5)
					{
						// Legacy simple integer compare path removed (handled by ir::compare_ops)
					}
					else if (op == "icmp" && il.size() == 7)
					{
						// Legacy icmp path removed (handled by ir::compare_ops)
					}
					else if ((op == "and" || op == "or" || op == "xor" || op == "shl" || op == "lshr" || op == "ashr") && il.size() == 5)
					{
						// Legacy bitwise/shift path removed (handled earlier by ir::core_ops)
					}
					else if ((op == "zext" || op == "sext" || op == "trunc" || op == "bitcast" || op == "sitofp" || op == "uitofp" || op == "fptosi" || op == "fptoui" || op == "ptrtoint" || op == "inttoptr") && il.size() == 4)
					{
						// Legacy cast path removed (handled by ir::cast_ops)
					}
					else if (op == "as" && il.size() == 4)
					{ // (as %dst <type> %initOrLiteral)
					  // Legacy 'as' variable init path removed (handled by ir::variable_ops)
					}

					else if (edn::ir::phi_ops::collect(S_local_for_dispatch, il, pendingPhis))
					{
						continue; // handled by phi_ops (collection phase)
					}

					else if (op == "panic" && il.size() == 1)
					{
						edn::ir::builder::State S_exc{builder, *llctx_, *module_, tctx_, [&](TypeId id)
													  { return map_type(id); }, vmap, vtypes, varSlots, initAlias, defNode, debug_manager_};
						edn::ir::exception_ops::Context EC{S_exc, builder, *llctx_, *module_, F, enableDebugInfo, panicUnwind, enableEHItanium, enableEHSEH, selectedPersonality, cfCounter, sehExceptTargetStack, itnExceptTargetStack, sehCleanupBB, [&](const std::vector<edn::node_ptr> &nodes) { /* unused here */ }};
						if (edn::ir::exception_ops::handle_panic(EC, il))
							continue;
					}
					// --- M4.6 Coroutines (minimal) ---
					else if (edn::ir::coro_ops::handle(S_local_for_dispatch, il, enableCoro, lastCoroIdTok))
					{
						continue; // handled by coro_ops
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
					{
						edn::ir::builder::State S_exc{builder, *llctx_, *module_, tctx_, [&](TypeId id)
													  { return map_type(id); }, vmap, vtypes, varSlots, initAlias, defNode, debug_manager_};
						edn::ir::exception_ops::Context EC{S_exc, builder, *llctx_, *module_, F, enableDebugInfo, panicUnwind, enableEHItanium, enableEHSEH, selectedPersonality, cfCounter, sehExceptTargetStack, itnExceptTargetStack, sehCleanupBB, [&](const std::vector<edn::node_ptr> &nodes)
														   { emit_ref(nodes, emit_ref); }};
						if (edn::ir::exception_ops::handle_try(EC, il))
							continue;
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
			edn::ir::builder::State S_finalize{builder, *llctx_, *module_, tctx_, [&](TypeId id)
											   { return map_type(id); }, vmap, vtypes, varSlots, initAlias, defNode, debug_manager_};
			edn::ir::phi_ops::finalize(S_finalize, pendingPhis, F, [&](TypeId id)
									   { return map_type(id); });
			if (!entry->getTerminator())
			{
				if (fty->getReturnType()->isVoidTy())
					builder.CreateRetVoid();
				else
					builder.CreateRet(llvm::Constant::getNullValue(fty->getReturnType()));
			}
		}
		// Finalize debug info & optional pass pipeline via helper module
		edn::ir::debug_pipeline::finalize_debug(debug_manager_, enableDebugInfo);
		edn::ir::debug_pipeline::run_pass_pipeline(*module_);
		return module_.get();
	}

	llvm::orc::ThreadSafeModule IREmitter::toThreadSafeModule() { return llvm::orc::ThreadSafeModule(std::move(module_), std::move(llctx_)); }

} // namespace edn
