# Phase 4 Plan – Platform Infrastructure for Modern Language Support

Status: In progress (2025-08-15)

Goal: Equip EDN with cross-cutting capabilities (types, ABI, runtime hooks, codegen QoL) needed to host front‑ends for modern languages (Rust, TypeScript, Python, Zig). Focus on reusable primitives, tight diagnostics, and testable, incremental delivery.

Links: see `design/modern_language_support_roadmap.md` for the long-term map.

## Scope and Non‑Goals
- In scope: new IR forms, type system extensions, ABI attributes, EH/coroutines/closure lowering helpers, GC/statepoints scaffolding, opt‑in optimization pipeline, debug info, verification/lints.
- Out of scope: full language front‑ends; complex runtime libraries beyond minimal shims.

## Milestones (M4.x)
1. M4.1 Sum Types & Match Helpers
   - Tagged unions (ADTs) and `option`/`result` conventions.
   - Minimal pattern match helper or select-based lowering.
   - Error codes: E1400–E1423 used (sum-new/is/get; match, binds, exhaustiveness, result-as-value).
2. M4.2 Generics – Monomorphization
    - Approach: reader‑macro style expander prior to type check.
       - `(gfn :name "f" :generics [ T U ... ] ...)` declares a generic template.
       - `(gcall %dst <ret-type> f :types [ <args>* ] %args...)` requests an instantiation; the expander:
          - Rewrites it to `(call %dst <ret-type> f@mangled %args...)` where `mangled = name@arg1$arg2...` and args are sanitized `to_string` of types.
          - Generates a concrete `(fn ...)` by cloning the `gfn`, dropping `:generics`, substituting type params with the provided types, and setting `:name` to the mangled name.
       - Emission placement: specializations are emitted at the original `gfn` position (the `gfn` is omitted), ensuring deterministic ordering and avoiding linker renaming like `.1` suffix.
       - Header preservation: module `:kw value` pairs are preserved verbatim.
       - Deduplication: repeated identical instantiations are emitted once per module.
   - Integration: `IREmitter::emit` runs `expand_traits` then `expand_generics` before type checking.
    - Tests: `phase4_generics_macro_test.cpp` (basic), `phase4_generics_more_tests.cpp` (two type params, dedup).
    - Error codes: None added yet (expansion happens pre‑check). If future validation errors are needed, use reserved range E1470–E1479.
3. M4.3 Traits/Interfaces & VTables
    - Dictionary passing; trait object layout (ptr + vtable).
    - Implemented as reader-macro expander that synthesizes two structs per trait:
       - `<Trait>VT` (vtable with one function-pointer field per method; field names are symbols)
       - `<Trait>Obj` (object wrapper with fields `data: (ptr i8)` and `vtable: (ptr <Trait>VT)`)
    - Lowerings:
       - `(make-trait-obj %o Trait %dataPtr %vtPtr)` → `(bitcast %data.i8 (ptr i8) %dataPtr)` + `(struct-lit %o TraitObj [ data %data.i8 vtable %vtPtr ])`
       - `(trait-call %dst <ret> Trait %obj method %args...)` → `member-addr/load` of method fnptr from `%obj.vtable`, retrieve ctx from `%obj.data`, then `call-indirect` with `%args...` (ctx first if present)
    - Constraints enforced by existing checker rules:
       - Struct field names are symbols; function names are strings
       - Explicit types required on ret/store/load/call-indirect
       - `member-addr` base must be pointer to the struct; avoid redundant addr-of on allocas
    - Error codes: no new codes; relies on existing E08xx (struct/member) and E13xx (fnptr/call-indirect). If needed later, reserve E1480–E1489 for trait-specific validation.
4. M4.4 Closures & Captures
   - Env struct capture; escaping vs non‑escaping; callable thunks.
   - Error codes: E1430–E1439.
5. M4.5 Exceptions & Panics
   - LLVM EH personality wiring (Itanium/SEH); landingpads; panic=abort|unwind toggle.
   - Error codes: E1440–E1449. Env flags: `EDN_ENABLE_EH=1`, `EDN_PANIC=abort|unwind`.
6. M4.6 Coroutines & Async
   - Integrate LLVM `coro.*` intrinsics; helper lowering utilities.
   - Error codes: E1450–E1459. Env flag: `EDN_ENABLE_CORO=1`.
7. M4.7 GC/Statepoints
   - Statepoint/stackmap emission hooks; allocation API shim; RC/ARC initial profile.
   - Error codes: E1460–E1469. Env flag: `EDN_ENABLE_GC=rc|arc|statepoint`.
8. M4.8 Opt‑In Optimization Pipeline
   - Pass manager scaffolding; `mem2reg`, `instcombine`, `simplifycfg`, `dce`, `sroa`.
   - Env flag: `EDN_ENABLE_PASSES=1`.
9. M4.9 Debug Info & Diagnostics Spans
   - DIBuilder for functions/locals; line/col on IR; extend JSON diagnostics with spans.
   - No new error codes; doc and tests.
10. M4.10 Lints/Verifier & Golden IR Tests
   - EDN-level lints (unreachable blocks, missing terminators, unused vars/globals).
   - Golden IR snapshot/matcher test harness.

Ordering can adjust; aim to land self-contained slices with tests.

## Deliverables per Milestone
- Spec: brief design notes in this file (forms, invariants, error codes).
- Implementation: type check + emission helpers; env flags as applicable.
- Tests: positive and negative; JSON diagnostic assertions for new error codes.
- Docs: README additions; `design/error_codes.md` updated; CHANGELOG entry.

## Proposed IR Additions (Sketch)
- ADT: `(sum :name T :variants [ (variant :name A :fields [ <types>* ]) ... ])` and value constructors.
- Match: `(match %dst <to-type> %value :arms [ (arm :variant A :binds [ ... ] :body [ ... ]) ... ])` (or lower-level select helpers).
- Trait Object: `(vtable :name VT :methods [ ... ])` (metadata) and runtime layout conventions.
- Closure: `(closure %dst (ptr <fn-type>) %fn [%captures...])` → produces thunk + env.
- EH: `(try [ ... ] :catch [ ... ])` or front-end lowered; EDN provides landingpad helpers.
- Coroutine: `(coro-begin ...) (coro-suspend ...) (coro-end ...)` helpers or front-end lowering; EDN wires LLVM intrinsics.

## Environment Flags (Phase 4)
- `EDN_ENABLE_PASSES=1`
- `EDN_ENABLE_EH=1`; `EDN_PANIC=abort|unwind`
- `EDN_ENABLE_CORO=1`
- `EDN_ENABLE_GC=rc|arc|statepoint`

## Testing Strategy
- Extend existing test harness with feature groups (phase4_* files).
- Add golden IR tests for ADTs/closures/coroutines/EH minimal cases.
- Add linter tests for verifier warnings.

## Initial Task Seeds
- [M4.8] Wire pass pipeline behind `EDN_ENABLE_PASSES`; add a couple of golden IR tests to prove effect.
- [M4.1] Define sum type metadata and minimal constructor/check ops; add type checker validation and an emitter placeholder.
- [M4.4] Implement closure capture to env struct + thunk emission for a simple add capture.
- [M4.5] Add EH personality/landingpad scaffolding behind `EDN_ENABLE_EH` and a panic=abort path.
- [M4.9] Add DIBuilder hooks for function and basic block line info; prove with a debug build sample.

## Risks & Mitigations
- Complexity creep: ship narrow slices; guard experimental features with flags.
- ABI/regression risk: keep golden IR tests stable; use verifier and lints.
- Platform differences (Windows SEH vs Itanium): abstract via small helpers; test on both.

## Completion Criteria
- All M4.x slices landed with tests and docs.
- Feature flags stable; default build remains functional without them.
- Roadmap prototypes (Phase 5) unblocked by Phase 4 primitives.

## Progress summary (as of 2025-08-15)
- M4.1 Sum Types & Match: DONE
   - Implemented sums (constructors, tag test, field get), match helper with binds, exhaustiveness checks, and result-as-value form with PHI merge.
   - Error codes E1400–E1423 added; structured diagnostics with JSON option.
   - Tests: positive/negative suites and golden IR snapshots for sums/match.
- M4.8 Opt‑In Optimization Pipeline: DONE
   - Pass pipeline behind `EDN_ENABLE_PASSES`; disabled during golden tests for determinism.
- M4.10 Lints/Verifier & Golden IR Tests: PARTIAL
   - Golden IR harness in place for ADTs/match. Lints (unreachable, missing terminators, unused) pending.
- M4.2 Generics – Monomorphization: DONE
   - Reader‑macro expander implemented (`include/edn/generics.hpp`), integrated pre‑type‑check.
   - Rewrites `(gcall ...)` → `(call ...)`, generates specializations, preserves header, dedups instances.
   - Tests added and passing: basic gfn/gcall, two type params, instantiation dedup.
- M4.3 Traits/VTables: DONE
    - Expander implemented (`include/edn/traits.hpp`), integrated prior to generics in `IREmitter::emit`.
    - Generates `<Trait>VT` and `<Trait>Obj`, lowers `make-trait-obj` and `trait-call` to core IR.
    - Tests passing; example target `edn_traits_example` verifies IR.
    - Docs: `docs/TRAITS.md` (shape, lowering, constraints).
- Remaining milestones (M4.4 Closures, M4.5 Exceptions, M4.6 Coroutines, M4.7 GC/Statepoints, M4.9 Debug Info): NOT STARTED

### Pointers (docs & examples)
- Docs:
   - `docs/TRAITS.md`, `docs/GENERICS.md`, `docs/SUMS.md`
- Examples (CMake targets):
   - `edn_traits_example`, `edn_generics_example`, `edn_sum_example`
   - Built in `build/examples/Release/` and verified via LLVM IR verifier

Error code allocation note: M4.1 consumed E1400–E1423 (including result-as-value). Adjust later milestone ranges to avoid collisions (e.g., use E147x+ for Generics).
