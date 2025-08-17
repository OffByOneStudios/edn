# Exception Handling (EH) in edn

This page summarizes how edn controls exception/unwind IR emission and how to test it across targets.

## Flags
- EDN_EH_MODEL=itanium|seh
  - Selects the EH personality and lowering model.
- EDN_ENABLE_EH=1
  - Enables IR emission with invoke-based control flow.
- EDN_PANIC=abort|unwind
  - Controls how `(panic)` lowers.
- EDN_TARGET_TRIPLE=...
  - Overrides the LLVM module target triple (useful for cross-target IR shape tests).

## IR shapes by model
- Itanium (Linux/macOS-like)
  - Personality: `__gxx_personality_v0`
  - Calls lower to: `invoke` -> `landingpad` (cleanup) -> `resume`
  - Try/Catch (minimal catch-all):
    - Surface: `(try :body [ ... ] :catch [ ... ])`
    - IR: invokes inside the `:body` region unwind to a `landingpad` of type `{ i8*, i32 }` with a catch-all clause (`catch i8* null`); the landingpad branches to the handler block. Calls outside try regions use a cleanup-only landingpad that resumes.
  - Panic=unwind: `call @__cxa_throw(null,null,null)` + `unreachable`
- SEH (Windows/MSVC-like)
  - Personality: `__C_specific_handler`
  - Calls lower to: `invoke` -> shared `cleanuppad`/`cleanupret` funclet
  - Panic=unwind: `call @RaiseException(code, 1, 0, null)` + `unreachable`
  - Functions with a personality are tagged `uwtable`.
  - Try/Catch (minimal catch-all):
    - Surface: `(try :body [ ... ] :catch [ ... ])`
    - IR: invokes inside the `:body` region unwind to a `catchswitch` which routes to a `catchpad`; the `catchpad` performs `catchret` to the catch handler block; handler executes `:catch` then falls through to the continuation.

## Behavior matrix
- With EDN_ENABLE_EH=1: `invoke` + appropriate handler path per model.
- Without EDN_ENABLE_EH: plain `call` (no funclets/landingpads) though a personality may still be attached for inspection.

## Tests (placeholders reference existing unit tests)
- Itanium
  - Invoke/landingpad smoke
  - Try/Catch smoke: presence of `invoke`, `landingpad`, and a `catch` clause (e.g., `catch i8* null`)
  - Panic=unwind lowers to `__cxa_throw`
- SEH
  - Invoke/cleanuppad smoke
  - Panic=unwind lowers to `RaiseException`
  - Cleanup funclet consolidation (single `cleanuppad`, multiple `invoke`)
  - Negative: no `invoke`/funclets when EH disabled
  - Try/Catch smoke: presence of `catchswitch`/`catchpad`/`catchret` and invokes in body

## Notes
- Both models are gated by env flags so golden IR remains deterministic by default.
- Cross-target testing via `EDN_TARGET_TRIPLE` ensures stable IR shape across platforms.
# Exception Handling (EH) Models in EDN

This document summarizes how EDN targets exception handling across platforms and how to control codegen hooks via environment flags during tests.

## Models

- Itanium C++ ABI (DWARF-based, zero-cost)
  - Platforms: Linux, macOS (Darwin), many Unix targets on x86_64/arm64
  - Personality: `__gxx_personality_v0`
  - IR: `invoke`/`landingpad`/`resume`, clauses for `catch`/`cleanup`
  - Unwind tables: DWARF `__eh_frame`; macOS also uses compact unwind in `__TEXT,__unwind_info`

- Windows SEH / C++ EH (funclets)
  - Platform: Windows
  - Personality: typically `__C_specific_handler` (C), C++ uses MSVC model with funclets
  - IR: funclet intrinsics/blocks (`catchswitch`/`catchpad`/`cleanuppad`) and unwind maps

## Current Status in EDN (Phase 4)

- Personality wiring: DONE
  - Set `EDN_EH_MODEL=itanium|seh` to tag functions with the corresponding personality.
- Target triple override: DONE
  - Set `EDN_TARGET_TRIPLE` (e.g., `x86_64-apple-darwin`, `x86_64-unknown-linux-gnu`) to direct the module triple for IR/codegen tests.
- Unwind IR scaffolding (Itanium): DONE (smoke path)
  - `EDN_ENABLE_EH=1` enables emission of Itanium-style `invoke`/`landingpad`/`resume` for direct calls. The exceptional edge lands in a cleanup-only landingpad that immediately `resume`s. This is sufficient to validate IR structure from Windows.
  - `EDN_PANIC=unwind` (Itanium) lowers `(panic)` to a `__cxa_throw(nullptr,nullptr,nullptr)` followed by `unreachable`. Default remains `abort` via `llvm.trap` when unset. This is IR-shape only; no actual exception object is constructed.

## Usage (Tests)

- Tag emitted functions with an Itanium personality and aim IR at macOS from Windows:
  - `EDN_EH_MODEL=itanium`
  - `EDN_TARGET_TRIPLE=x86_64-apple-darwin` (or `arm64-apple-darwin`)
- Keep default (no EH) by leaving flags unset.

## Notes for Darwin

- macOS uses the Itanium model. Use `__gxx_personality_v0`.
- The JIT/runtime layer is not required for IR verification in tests; we verify structure and personalities on Windows and leave runtime execution to macOS/Linux CI later.
