# Phase 5 Plan — Language Prototypes toward the Modern Language Support North Star

Status: In progress (2025-08-17) — Rustlite subset mostly complete; see `design/rustlite.md`.
Update (2025-08-17): Rustlite macros and demos expanded (control flow, ADTs/match, traits sugar, assertions, field/index/pointer sugar, while‑let, and rdot including dot‑path). Full Rustlite driver suite runs green on Windows; IR shape checks cover ADTs, trait calls, generics, extern calls, and panic models.

North Star (from modern_language_support_roadmap.md): EDN is a compact, deterministic S‑expression IR and toolchain enabling modern language front‑ends (Python, TypeScript, Rust, Zig) via the right semantic primitives, ABIs, and runtime integration points.

See also: `design/abi.md` for the frozen v0.1 ABI referenced by this plan, and `design/rustlite.md` for the Rustlite subset design and demo.

## 1) Objective and Outcomes

Demonstrate end‑to‑end viability by shipping minimal, but real, language prototypes compiled to LLVM via EDN. Each prototype validates critical mappings (types/ABI/runtime) and exercises cross‑cutting platform features added in Phase 4.

Success means:
- Each target language has a tiny subset that compiles and runs end‑to‑end using EDN.
- ABIs for closures, traits/vtables, and sum/union layout are frozen and documented.
- Debug info and optimization presets work without breaking dev experience.

## 2) Scope (In / Out)

In scope:
- Four minimal prototypes (Rust‑like, TypeScript‑like, Python interop subset, Zig‑like) with runnable examples and tests.
- ABI freeze + docs for closure envs, vtables/trait objects, sum/union representation.
- DI coverage for sums/unions and generics; DI preservation under optimization.
- Opt presets (O0/O1/O2) and a tiny microbench harness.
- Minimal runtime shims required by prototypes (e.g., RC/GC stub, coroutine helper).

Out of scope (Phase 5):
- Full language front‑ends. We build just enough transforms/emitters or hand‑written EDN to prove capability.
- Production GC. Begin with RC or a simple bump/mark stub; full statepoint GC may land later.

## 3) Entry Criteria (from Phase 4)

Must be DONE to start Phase 5 (blocking):
- ABI surfaces identified: closure env layout, trait object/vtable, sum/union tagging. Golden tests to verify stability.
- DI foundations: functions/params/locals/structs (DONE); extend to sums/unions and generics (planned below).
- CI runs split test suites (edn.core/phase2/phase3/phase4) for Debug/Release; both EH models covered.

## 4) Workstreams and Deliverables

### 4.1 Rust‑like Subset

Goal: Validate ADTs (enums with payload), traits + trait objects, monomorphized generics, panic handling.

Deliverables:
- EDN examples implementing: Option/Result, an enum match, a trait + object call, and a generic function (mono) used at 2+ instantiations.
- Golden IR tests for ADT layout and vtable call sequences; C interop smoke (extern "C" function call).
- Switchable panic model: abort|unwind; personality wiring verified.

Notes:
- A minimal Rustlite front-end exists under `languages/rustlite/` with macros for `rlet`, `rmut`, and `rif-let`; the demo driver verifies PHI correctness for result-mode match. Details in `design/rustlite.md`.

Progress snapshot (2025-08-17)
- Implemented macros (selected): locals and control (`rlet`, `rmut`, `rif/relse`, `rwhile`, `rfor`, `rloop`, `rbreak`, `rcontinue`), ADTs/match (`rmatch`, `rsum/rsome/rnone/rok/rerr`, `rif-let`, `rwhile-let`), functions/interop (`rfn`, `rextern-fn`, `rcall`), closures (`rclosure`, `rcall-closure`), traits sugar (`rtrait`, `rimpl`, `rmethod`, `rfnptr`, `rmake-trait-obj`, `rtrait-call`, `rdot` including `Trait.method` dot‑path), error handling (`rpanic`, `rassert`, `rassert-eq/ne/lt/le/gt/ge`), data and access sugar (`rstruct`, `renum`, `rtypedef`, `rget`, `rset`, `rindex-addr`, `rindex`, `rindex-store`, `rindex-load`, `raddr`, `rderef`). Short‑circuit logic `rand`/`ror` implemented SSA‑safely.
- Demos/tests: comprehensive `rustlite_driver` now green; focused drivers `rustlite_minidriver`/`rustlite_negdriver` (field/index/pointer sugar), `rustlite_traitdriver` (trait‑object IR shape), `rustlite_genericsdriver` (mono demo), `rustlite_externdriver` (extern C call), `rustlite_panicdriver` (abort/unwind), `rustlite_logicdriver` (rand/ror), `rustlite_rwhilelet_driver` (while‑let), and `rustlite_rdot_{driver,negdriver}` (method‑call sugar). All pass on Windows.
- Tooling: macros registered in the Rustlite expander; CMake targets and CTest wiring in place with positive/negative coverage and environment‑driven EH toggles.

Progress: ~90% of Rust‑like subset deliverables complete.

Coverage vs deliverables
- Option/Result and enum match: Covered via `rsum` helpers and `rmatch`/`rif-let`; validated in drivers (PHI validation present).
- Trait + trait-object call: Implemented via `rustlite_traitdriver` with golden IR shape checks.
- Generic function (monomorphized, 2+ instantiations): Implemented via `rustlite_genericsdriver` with IR assertions for two specialized functions and direct calls.
- C interop smoke (extern "C" call): Implemented via `rustlite_externdriver` with IR checks for external declaration and a direct call.
- Panic model (abort|unwind) verification: Implemented via `rustlite_panicdriver` with tests that assert abort mode (`llvm.trap`/`unreachable`) and unwind mode (platform personality and `RaiseException`/`__cxa_throw`).

Next actions (ordered, small and verifiable)
1) Interop helpers: add `rextern-global`/`rextern-const` sugar over core `(global ...)`, `gload`, `gstore`; negative test for `gstore` to const (expect E1226).
2) Literal helpers: `rcstr` (NUL‑terminated i8 array returning `(ptr i8)`) and `rbytes` (i8 array; optionally return pointer and length), with IR checks and a small extern consumer.
3) ABI docs touch‑ups: confirm and document vtable layout and sum tagging used by Rustlite demos in `design/abi.md`.
4) Optional: a tiny try/catch EH demo (if keeping unwind model around) to complement `rpanic` tests.
5) CI: add an Itanium triple cross‑compile smoke for the Rustlite suite to validate non‑SEH EH paths.

Exit for 4.1 after next actions
- All 4.1 deliverables have concrete demos/tests; IR shape checks cover ADT layout and vtable call shapes on Windows; Itanium cross‑compile smoke added; interop/literal helpers in place. At that point we’re ready to invest in a real Rust frontend on top of the stable macro surface.

Acceptance:
- Examples compile and run, returning expected values.
- Layout and vtable golden checks pass on Windows (MSVC/SEH) and one Unix triple (Itanium).

### 4.2 TypeScript‑like Subset

Goal: Validate classes/interfaces, structural dispatch via dictionary/vtable, generics (mono for used instantiations), exceptions, and async/await over a simple GC/RC runtime.

Deliverables:
- EDN examples: class with interface, generic function used twice, async function awaiting a coroutine; exception thrown and caught.
- Minimal runtime shim: allocator + root tracking (RC or stub GC); array/string placeholders.
- Golden IR tests for interface table layout and coroutine lowering shape.

Acceptance:
- Examples run; async path produces expected sequencing.
- DI preserved at O0 and O1 for stepping through TS subset examples.

### 4.3 Python Interop Subset

Goal: Validate closures, exceptions, and generators via CPython C‑API interop or a thin shim; show module init and a function callable from Python.

Deliverables:
- EDN emitting calls to CPython API (or a stubbed ABI) for: raising an exception, creating a closure/callable, and a simple generator next/stop.
- Example: Python imports the built module and calls a function that uses EDN‑generated IR.

Acceptance:
- A smoke script runs and validates return values/exceptions (Windows dev box acceptable; Linux later if toolchain available).

### 4.4 Zig‑like Subset

Goal: Validate error unions (sum types without EH), defer semantics (destructors/RAII hooks), packed layouts and callconv selection.

Deliverables:
- EDN examples: function returning error union; caller matches on error/success; defer lowering releasing a resource; one packed struct; a function using a non‑default calling convention.
- Golden IR tests verifying byval/byref attrs and packed layout.

Acceptance:
- Examples run; error handling path and defers behave deterministically.

## 5) Cross‑Cutting Tasks

### 5.1 ABI Freeze and Docs
- Closure environment layout: field order, alignment, heap vs stack capture conventions.
- Trait object and dictionary passing: method table order, pointer sizes, fat pointer form.
- Sum/union tagging: discriminant size, payload layout, niche usage (documented, even if minimal for now).

Artifacts: design/abi.md (new), golden IR layout tests, interop smoke tests.

### 5.2 Debug Info Completion and Preservation
- Map DI for sums/unions and generic instantiations (type params in signatures where applicable).
- Add DI‑on + optimized build preservation tests (O0/O1); ensure stepping/locals survive typical opts.

### 5.3 Optimization Presets and Microbench (Backlog)
- Status: Baseline presets exist (O0/O1/O2/O3) with a basic harness. Further tuning and pipeline customization are deprioritized for now.
- Move additional optimization work to the Phase 5 backlog; focus early Phase 5 on language prototypes and ABI/DI completion.

### 5.4 Minimal Runtime Shims
- RC or stub GC: allocation, retain/release hooks, and a root enumeration placeholder.
- Coroutine helper: small wrapper for resume/destroy and a trivial scheduler for async demo.

## 6) Acceptance Criteria (Phase Level)

- All four prototypes: build + run green on Windows (MSVC); at least Rust‑like runs on an Itanium triple (CI cross‑compile acceptable).
- ABI docs landed with golden tests; no ABI change without test updates.
- DI covers structs, sums/unions, generics; preservation verified at O0/O1.
- Opt presets selectable; microbench produces stable numbers (tracked over time).

## 7) Risks and Mitigations

- Runtime complexity bloat (GC/async): keep shims minimal; gate behind flags; defer full GC to Phase 6.
- ABI lock‑in too early: start with minimal, stable core; allow additive extensions; protect with golden tests.
- Platform variance (SEH vs Itanium): maintain both in CI for representative samples.

## 8) Milestones and Sequencing

M1 (Entry, 1 wk): ABI freeze + docs; DI sums/unions; CI matrix expanded; opt presets wired.

M2 (2 wks): Rust‑like subset examples + tests; Itanium smoke; panic model verified.

M3 (2 wks): TypeScript‑like subset with async over RC/GC stub; DI preservation tests at O1.

M4 (1–2 wks): Python interop subset (CPython calls) with a working import/call demo.

M5 (1 wk): Zig‑like subset (error unions, defer, callconv) + packed layout tests.

M6 (1 wk): Hardening: docs, CI stability, backlog: optimization tuning (pipelines/microbench deltas), follow‑up fixes.

## 9) Implementation Notes

- Reuse Phase 4 examples and expand under `edn/phase5/*` or extend `edn/phase4/` with prototype folders per language.
- Keep per‑phase test runners; consider additional split (phase4.di/eh/coro) if build time grows.
- Environment flags (existing): EDN_ENABLE_PASSES, EDN_ENABLE_EH, EDN_PANIC=abort|unwind, EDN_ENABLE_CORO, EDN_TARGET_TRIPLE, EDN_ENABLE_GC (planned values rc|arc|statepoint). Map EDN_OPT_LEVEL to pass presets.

## 10) Exit Criteria

- Prototypes and cross‑cutting tasks meet acceptance and are documented.
- CI green across Debug/Release on Windows; at least one Itanium triple job runs Rust‑like subset.
- ABI and DI docs published; CHANGELOG and roadmap updated; Phase 6 priorities clear (perf, GC/statepoints, library shims).
