# Phase 4 – Exceptions on Linux and Darwin (Itanium ABI): Notes & TODOs

Status: implemented (2025-08-16)

This document tracks the remaining work to round out Itanium-style exception handling for Linux and macOS (Darwin). It complements docs/EH.md and focuses on codegen, tests, and portability specifics for non-Windows targets.

## Current status (Itanium in EDN)
- Personality wiring: __gxx_personality_v0 attached when `EDN_EH_MODEL=itanium`.
- Calls: with `EDN_ENABLE_EH=1`, direct calls are emitted as `invoke`. Outside try-regions, the exceptional edge goes to a cleanup-only `landingpad` that `resume`s.
- Try/Catch (catch-all): `(try :body [...] :catch [...])` lowers to `invoke` → `landingpad { i8*, i32 }` with `catch i8* null` → handler block.
- Panic=unwind: with Itanium + EH enabled and `EDN_PANIC=unwind`, `(panic)` lowers to `call void @__cxa_throw(null,null,null)` + `unreachable`.
- Target triple override: `EDN_TARGET_TRIPLE` supports cross-target IR-shape testing (e.g., `x86_64-apple-darwin`, `x86_64-unknown-linux-gnu`).

## Gaps to close
1) Typed catch (deferred initially)
   - If/when needed, model clauses `catch i8* @typeinfo_for_T`. This implies either:
     - Emitting/declaring typeinfo objects for handled types, or
     - Restricting front-end to catch-all in v1 and skipping typed matching.

2) Resume and cleanup correctness
   - Keep cleanup-only landingpads for calls outside try-regions.
   - Within try, ensure catch-all landingpads do not emit `resume` unless we add typed filtering.

3) Attributes and platform notes
   - Keep personality attached and mark functions `uwtable` (already done) to enable unwinder table emission.
   - No additional attributes are required for IR-shape tests; for real linking/execution, platform libc++/libstdc++/libunwind differences apply but are out of scope for IR tests.

4) Tests to add
   - Negative test (EH disabled): no `invoke`/`landingpad` even if personality is selected.
   - Optional: Panic inside try should be caught by catch-all handler (assert structural dominance: `__cxa_throw` present and handler path exists).

5) Docs
   - `docs/EH.md` now includes the Itanium try/catch shape. Keep examples in sync as we add typed catches.

6) CI / portability
   - Keep using IR-structure tests on Windows via cross-target triples; actual linking/runtime validation can be deferred to Linux/macOS CI runners later.
   - Note: macOS uses compact unwind in addition to DWARF; LLVM handles this from IR + attributes; no special EDN changes required for IR tests.

## Minimal acceptance criteria
- With Itanium model + EH enabled, `(try :body [...] :catch [...])` lowers to `invoke`/`landingpad` with a catch-all clause and branches to handler.
- Existing panic and cleanup smoke tests remain green; the Itanium try/catch smoke test passes under Darwin/Linux triples.
- `docs/EH.md` documents the Itanium try/catch shape and flags.

## Stretch goals (later)
- Typed catches with typeinfo objects and selector matching.
- Multiple catch handlers with dispatch.
- Mixed cleanup + catch landingpads and structured cleanups.
- Execution tests on real Linux/macOS runners.
