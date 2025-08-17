# Phase 4 Plan – Platform Infrastructure for Modern Language Support

Status: In progress (2025-08-17)

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
    - Implemented: personality selection via `EDN_EH_MODEL=itanium|seh`; invoke-based EH on both models behind `EDN_ENABLE_EH=1`; try/catch lowering for:
       - Itanium: `invoke` → `landingpad` with catch-all clause (`catch i8* null`) → handler; cleanup-only landingpads outside try regions resume.
       - SEH: `catchswitch`/`catchpad`/`catchret` funclets routing invokes from try bodies to the handler; shared `cleanuppad` for cleanup-only edges.
       - Panic=unwind: Itanium lowers `(panic)` to `__cxa_throw(...)` + `unreachable`; SEH lowers to `RaiseException(...)` + `unreachable`.
   - Error codes: E1440–E1449. Env flags: `EDN_EH_MODEL`, `EDN_ENABLE_EH=1`, `EDN_PANIC=abort|unwind`.
6. M4.6 Coroutines & Async
   - Integrate LLVM `coro.*` intrinsics; helper lowering utilities.
   - Implemented (v1): begin/id/save/suspend/final-suspend/end + promise/resume/destroy/done + size/alloc/free.
   - Error codes: E1460–E1475 allocated for coroutine op validation.
   - Env flag: `EDN_ENABLE_CORO=1`.
7. M4.7 GC/Statepoints
   - Statepoint/stackmap emission hooks; allocation API shim; RC/ARC initial profile.
   - Error codes: E1460–E1469. Env flag: `EDN_ENABLE_GC=rc|arc|statepoint`.
8. M4.8 Opt‑In Optimization Pipeline
   - Pass manager scaffolding; `mem2reg`, `instcombine`, `simplifycfg`, `dce`, `sroa`.
   - Env flag: `EDN_ENABLE_PASSES=1`.
   - Status: Implemented; tests verify effects: DCE (`phase4_passes_dce_test.cpp`) and mem2reg (`phase4_passes_mem2reg_test.cpp`).
9. M4.9 Debug Info & Diagnostics Spans
   - DIBuilder for functions/locals; line/col on IR; extend JSON diagnostics with spans.
   - Status: STARTED. Minimal DI wired: compile unit+file, DISubprogram per function, DILocation per instruction from node spans; gated via EDN_ENABLE_DEBUG=1. Smoke test added.
   - Next: local variable dbg.declare/addr, richer type mapping, and JSON diagnostic span enrichments.
10. M4.10 Lints/Verifier & Golden IR Tests
   - EDN-level lints (unreachable blocks, missing terminators, unused vars/params/globals) — DONE, incl. W1405 unused global.
   - Golden IR snapshot/matcher test harness — DONE for sums/match and coroutines; extended as needed.

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
 - `EDN_ENABLE_DEBUG=1`

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

## Progress summary (as of 2025-08-16)
- M4.1 Sum Types & Match: DONE
   - Implemented sums (constructors, tag test, field get), match helper with binds, exhaustiveness checks, and result-as-value form with PHI merge.
   - Error codes E1400–E1423 added; structured diagnostics with JSON option.
   - Tests: positive/negative suites and golden IR snapshots for sums/match.
- M4.8 Opt‑In Optimization Pipeline: DONE
   - Pass pipeline behind `EDN_ENABLE_PASSES`; disabled during golden tests for determinism. Added DCE and mem2reg smoke tests.
- M4.10 Lints/Verifier & Golden IR Tests: PARTIAL
   - Golden IR harness in place for ADTs/match. Lints (unreachable, missing terminators, unused) pending.
   - Update: lints recognize `(panic)` as a terminator for reachability (no false positives after it).
- M4.2 Generics – Monomorphization: DONE
   - Reader‑macro expander implemented (`include/edn/generics.hpp`), integrated pre‑type‑check.
   - Rewrites `(gcall ...)` → `(call ...)`, generates specializations, preserves header, dedups instances.
   - Tests added and passing: basic gfn/gcall, two type params, instantiation dedup.
- M4.3 Traits/VTables: DONE
    - Expander implemented (`include/edn/traits.hpp`), integrated prior to generics in `IREmitter::emit`.
    - Generates `<Trait>VT` and `<Trait>Obj`, lowers `make-trait-obj` and `trait-call` to core IR.
    - Tests passing; example target `edn_traits_example` verifies IR.
    - Docs: `docs/TRAITS.md` (shape, lowering, constraints).
- M4.4 Closures & Captures: DONE (phase 4 scope)
   - Implemented minimal, non‑escaping closures with a single capture.
   - IR form accepted: `(closure %dst (ptr <fn-type>) %fn [ %env ])`.
   - Lowering (minimal): per‑site private thunk that loads `%env` from a private global and calls the target as `Target(env, args...)`; result is the thunk function pointer.
   - Type checker validation added for closure form (arity, `%dst`/types, callee signature, single capture type) and error codes reserved in E143x.
   - Test added: `tests/phase4_closures_min_test.cpp` (verifies thunk synthesis/builds); suite green.
   - Record path: Added `(make-closure ...)` and `(call-closure ...)` with explicit struct `{ i8* fn, <EnvType> env }` named `__edn.closure.<callee>`; `call-closure` indirect-calls using the loaded fnptr and the callee’s function type (env first).
   - Tests: `tests/phase4_closures_record_test.cpp` and negative cases in `tests/phase4_closures_negative_test.cpp` and `tests/phase4_closures_capture_mismatch_test.cpp`.
   - JIT runtime smoke passes.
   - Next (out of current scope): multi‑capture env struct and escaping closures with lifetime management.
- M4.5 Exceptions & Panics: DONE (abort + unwind on Itanium)
    - `(panic)` implemented with two modes: default abort lowers to `llvm.trap` + `unreachable`; with `EDN_ENABLE_EH=1` and `EDN_PANIC=unwind`, lowers to platform-specific unwind calls (Itanium `__cxa_throw`, SEH `RaiseException`) + `unreachable`.
    - Personality wiring implemented: `EDN_EH_MODEL=itanium|seh` tags functions with `__gxx_personality_v0` or `__C_specific_handler`; functions are marked `uwtable`.
    - Try/Catch lowering implemented on both models:
       - Itanium: `invoke`/`landingpad` with catch-all clause and handler block.
       - SEH: `catchswitch`/`catchpad`/`catchret` handler path; invokes emitted in try bodies.
    - Tests: panic (abort/unwind), invoke smoke, SEH try/catch, Itanium try/catch are present and green in configured environments.
    - TypeChecker recognizes `(try)` and validates `:body`/`:catch` vectors with new diagnostics.
- Remaining milestones (M4.6 Coroutines, M4.7 GC/Statepoints, M4.9 Debug Info): M4.6 PARTIAL, others NOT STARTED

### Pointers (docs & examples)
- Docs:
   - `docs/TRAITS.md`, `docs/GENERICS.md`, `docs/SUMS.md`
- Examples (CMake targets):
   - `edn_traits_example`, `edn_generics_example`, `edn_sum_example`
   - Built in `build/examples/Release/` and verified via LLVM IR verifier

Error code allocation note: M4.1 consumed E1400–E1423 (including result-as-value). Adjust later milestone ranges to avoid collisions (e.g., use E147x+ for Generics).

## Open issues / notes (2025‑08‑16)
- Phase 3 examples smoke: `edn/phase3/union_access.edn` still throws during type check ("invalid vector subscript"). We currently tolerate one failure in the examples smoke harness. Decide whether to fix the example or mark it explicitly as expected‑negative.

## Phase 4 status summary (2025‑08‑17)

- M4.1 Sum Types & Match: DONE
- M4.2 Generics: DONE
- M4.3 Traits/Interfaces: DONE (macro-based expander and vtable pattern)
- M4.4 Closures & Captures: DONE (record-based closures, non-escaping)
- M4.5 Exceptions & Panics: DONE (Itanium + SEH; panic abort/unwind)
- M4.6 Coroutines & Async: DONE (v1)
   - Implemented: coro.id, coro.begin, coro.save, coro.suspend (isFinal=false/true), coro.end, coro.promise, coro.resume, coro.destroy, coro.done, coro.size, coro.alloc, coro.free. Golden IR tests cover both minimal and expanded paths. Gated via `EDN_ENABLE_CORO`.
   - Docs: `docs/COROUTINES.md` finished with size/alloc/free, ABI note (Switch‑Resumed), the presplit attribute requirement, and an explicit pass pipeline.
   - Tests: lowering smoke runs CoroEarly → CoroSplit → CoroCleanup and passes; goldens remain green; a JIT smoke lowers, JITs, and resolves the coroutine symbol (lookup‑only) to validate end‑to‑end integration.
   - Optional follow‑ups: expand negative tests for additional arity/type errors; consider promise payload conventions; add a safe runtime invocation demo wired to an allocation strategy (coro.size + alloca or custom allocator) before resuming/destroying.
- M4.7 GC/Statepoints: NOT STARTED
- M4.8 Opt-in Pass Pipeline: DONE (mem2reg, DCE; tests included)
- M4.9 Debug Info & Spans: STARTED (compile unit/file, subprograms, per-inst locations; smoke test added)
- M4.10 Lints/Verifier & Goldens: DONE (lints incl. unused global; goldens in place)

Next steps for M4.6 (Coroutines) – optional polish:
- Provide a tiny runtime sample that allocates a frame (via coro.size/alloc or custom shim) and safely resumes/destroys.
- Expand negative tests for additional ops (E1467–E1471) and token/handle misuse.


## Out-of-scope polish (deferred beyond Phase 4)

The following enhancements are acknowledged but intentionally excluded from Phase 4 scope. They can be scheduled in a subsequent phase once core M4.x goals are locked.

- Debug Info (M4.9):
   - Remove or gate temporary "[dbg]"/"[di]" logs behind a verbose flag.
   - Additional DI tests: (a) DI disabled gating (EDN_ENABLE_DEBUG=0), (b) struct member offset monotonicity, (c) DI stability through mem2reg/DCE, (d) DI type coverage for sums/unions (tag/payload shape).
   - Full diagnostic span enrichment in JSON beyond current minimal spans.

- Coroutines (M4.6):
   - End-to-end runtime demo beyond lookup-only (allocate via coro.size/alloc, resume/destroy, cleanup semantics) and a small harness exercising it.

- Closures (M4.4):
   - Multi-capture environments and escaping closures with lifetime management (ARC/GC integration).

- Generics (M4.2):
   - Cross-module specialization reuse/caching; validation diagnostics for malformed gfn/gcall (reserve E147x); demangling/documented name scheme.

- Traits (M4.3):
   - VTable layout versioning, improved diagnostics for method resolution/arity, and perf tuning for indirect calls.

- Pass pipeline (M4.8):
   - Additional passes and configurable presets beyond mem2reg/DCE; pipeline tuning docs.

- Lints/Verifier (M4.10):
   - Extra lints (dead stores, interprocedural unreachable after panic chains) and selective suppression pragmas.

- Examples/Smoke harness:
   - Resolve or mark `phase3/union_access.edn` as expected-negative to remove the tolerated failure.

- Docs:
   - Consolidated environment flags reference (EDN_ENABLE_DEBUG/EDN_DEBUG_FILE/EDN_DEBUG_DIR, EH, CORO, PASSES) and user HOWTOs for DI and coroutines.

- GC/Statepoints (M4.7):
   - Entire milestone deferred to a later phase; no Phase 4 delivery expected here.
