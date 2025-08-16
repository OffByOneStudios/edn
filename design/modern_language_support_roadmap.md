# Modern Language Support Roadmap

Status: Draft (2025-08-15)

North Star: EDN should be a compact, deterministic S‑expression IR and toolchain capable of hosting front‑ends for modern languages (Python, TypeScript, Rust, Zig, etc.) by providing the right semantic primitives, ABIs, and runtime integration points.

## 1) Scope and Non‑Goals
- In scope: IR features, type system extensions, ABI hooks, runtime integration shims, diagnostics, and testing practices needed to compile modern languages to LLVM via EDN.
- Out of scope (for this roadmap): Full language front‑end implementations. We target enabling, then prototype small subsets as proofs of capability.

## 2) Cross‑Cutting Capabilities (Building Blocks)
These are the platform pieces we’ll invest in so multiple languages can target EDN consistently.

1. Advanced Type System Primitives
   - Algebraic data types (tagged unions / sum types) with payloads and pattern‑match support.
   - Parametric generics (monomorphization pipeline) and reified generics (dictionary/itables) in a limited form.
   - Traits/Interfaces and dictionary passing (trait object vtables) for dynamic dispatch.
   - Slices/views and fat pointers (ptr + len) as first‑class types.
   - Nullable/optional types; result/error unions with ergonomic matching.

2. Ownership, Memory, and Lifetimes
   - Pluggable memory model “profiles”: manual (C/Zig), borrow/own (Rust‑style, static), reference counting/ARC, and GC (via statepoints/stackmaps).
   - Explicit move semantics and deterministic destructors (RAII) hooks.
   - Safe pointer arithmetic boundaries (already present) and bounds metadata for slices.

3. ABI & Calling Conventions
   - CallConv selection per function (cdecl/fastcall/vectorcall/thiscall), sret handling, varargs (done), byval/byref attrs.
   - Interop with C ABI on major platforms; stable name mangling conventions.

4. Exceptions, Panics, and Error Handling
   - Zero‑cost unwinding support via LLVM EH (Itanium/SEH) with landingpads/personality.
   - Lightweight error unions for languages without EH (Zig‑style) and panic=abort/unwind toggles.

5. Closures, Lambdas, and First‑Class Functions
   - Capture lowering: environment structs + function pointer thunks; escaping/non‑escaping variants.
   - Partial application (optional) via generated trampoline stubs.

6. Coroutines and Async
   - LLVM coro.* intrinsic integration (stackless coroutines) with lowering utilities.
   - Async/await sugar lowering to coroutines or state machines; cooperative scheduling hooks.

7. Runtime & GC Integration Layer
   - Statepoint/stackmap emission and safepoint annotations.
   - Allocation/GC API shims (pluggable runtime: RC, ARC, or precise GC).

8. Reflection and Metadata
   - Optional RTTI/typeid for dynamic languages; lightweight reflection tables for fields/methods.
   - Debug info via DIBuilder; line/col mapping (extend current diagnostics and JSON mode).

9. Modules, Packages, and Linking
   - Symbol visibility, versioning, and simple package metadata for name resolution.
   - Incremental build support and deterministic codegen options.

10. Verification, Passes, and Testing
   - Opt‑in LLVM pass pipeline (mem2reg, instcombine, simplifycfg, dce, sroa).
   - Golden IR tests and a linter/verify pass for EDN‑level invariants.

## 3) Language Requirements and EDN Mappings

### Python (CPython‑compatible subset)
Key features:
- Dynamic typing; boxed values; dictionaries; attribute lookup; exceptions; generators/coroutines; refcounting + GIL semantics.

EDN requirements:
- Tagged Value representation (NaN‑boxing or pointer‑tag) and dynamic dispatch helpers.
- Exceptions mapped to LLVM EH or setjmp/longjmp stubs; rich traceback diagnostics optional.
- Closures (captures), generators → coroutines (coro.*) with iteration protocol.
- Refcount/GC hooks; FFI path to CPython C‑API for interop; module initialization ABI.

Initial prototype path:
- Compile a tiny, typed subset (or Python AST → EDN that calls the CPython API) to validate closures, exceptions, and generator lowering.

### TypeScript (AssemblyScript‑like typed subset)
Key features:
- Structural typing; generics (erased at runtime); union/intersection types; exceptions; async/await; GC.

EDN requirements:
- Structural interface compatibility via dictionary passing or shape tables.
- Generics lowered via monomorphization for performance subsets; erased types for dynamic edges.
- ADTs for union types; nullable/optional modeling; exceptions and async via coroutines.
- GC/statepoints and array/string runtime.

Initial prototype path:
- A typed TS subset (no `any`) with classes, interfaces, generics (mono for used instantiations), exceptions, and async over a simple GC runtime.

### Rust (safe subset with `unsafe` escape hatches)
Key features:
- Ownership/borrowing with lifetimes; zero‑cost generics; traits and trait objects; enums with payload (ADTs); pattern matching; slices; panics.

EDN requirements:
- Borrow/own profile (static checks in front‑end; EDN enforces layout and ABI).
- Monomorphization pipeline; trait objects via vtables and dictionary passing.
- ADTs with exhaustiveness support in the front‑end; panic=abort/unwind toggle.
- No GC; precise layout, niches, and sret conventions for aggregates.

Initial prototype path:
- Rust‑like mini‑front‑end to validate ADTs, monomorphization, trait objects, and panic handling.

### Zig (systems subset)
Key features:
- Manual memory; error unions; defer/defer errdefer; comptime evaluation; no hidden control flow.

EDN requirements:
- Error unions as result/union types; no EH by default; optional panic abort.
- Deterministic destructors via `defer` lowering; packed structs/bitfields; strict callconv selection.
- Optional const‑eval hooks (separate phase) for simple computations.

Initial prototype path:
- Lower a Zig‑like subset validating error unions, defers, packed layouts, and ABI selection.

## 4) Phased Delivery Plan

Phase 4 (Platform Infrastructure):
1. Sum types (tagged unions) and match/select helpers; result/option types.
2. Generics: monomorphization pipeline scaffolding; type substitution and codegen caching.
3. Traits/Interfaces: dictionary passing and trait‑object vtables.
4. Closures: capture env structs, escaping/non‑escaping conventions.
5. Exceptions/Panics: EH personality wiring; panic=abort/unwind toggle; basic landingpad emission.
6. Coroutines: LLVM coro.* integration + async sugar lowering helpers.
7. GC/Statepoints: initial statepoint emission and allocation hooks.
8. Opt‑in pass pipeline; golden IR tests; EDN lints/verify.
9. Debug info (DIBuilder) and expanded diagnostic spans.

Phase 5 (Language Prototypes):
1. Rust‑like subset: ADTs, traits, monomorphization, panics.
2. TypeScript‑like subset: classes/interfaces, GC, async/await, generics.
3. Python interop subset: closures, exceptions, generators via CPython API.
4. Zig‑like subset: error unions, defers, ABI selection.

Phase 6 (Consolidation & Performance):
1. Optimize closures/coroutines (escape analysis; stack vs heap envs).
2. Strengthen pass pipeline; add PGO hooks (optional).
3. Expand library/runtime shims (strings, arrays, hash maps) with portable ABIs.

## 5) Feature Matrix (Snapshot)

| Capability | Python | TypeScript | Rust | Zig |
|------------|:------:|:----------:|:----:|:---:|
| ADTs / Sum Types | via tagged boxes | native unions | enums (payload) | error unions |
| Generics | erased | mono + erased | mono | none (comptime) |
| Traits/Interfaces | duck typing | structural interfaces | traits + objects | none |
| Ownership Model | RC/GC | GC | borrow/own | manual |
| Exceptions | yes | yes | panic (abort/unwind) | none (errors) |
| Coroutines/Async | generators/async | async/await | async (executors) | async (cooperative) |
| Closures | yes | yes | yes | yes |
| GC/Statepoints | needed | needed | no | no |
| Debug Info | useful | useful | essential | essential |

## 6) Risks and Mitigations
- Feature creep: keep phases small, green tests, and hard completion criteria.
- ABI pitfalls: validate with C interop tests per platform early.
- Coroutine/EH complexity: start with minimal viable paths; gate behind flags.
- GC integration: begin with RC; migrate to statepoints when ready.

## 7) Success Criteria
- Each Phase 4 item lands with tests (positive/negative) and minimal docs.
- One minimal prototype per language in Phase 5 demonstrating end‑to‑end compile and run.
- Stable opt‑in optimization pipeline with measurable improvements and no regressions.

---
Appendix: Environment Flags (planned)
- EDN_ENABLE_PASSES=1 – enable optimization pipeline.
- EDN_ENABLE_EH=1 – enable exception/unwind emission; EDN_PANIC=abort|unwind.
- EDN_ENABLE_CORO=1 – enable coroutine lowering.
- EDN_ENABLE_GC=rc|arc|statepoint – select runtime memory model.
