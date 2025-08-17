# Phase 4 EDN Examples

This folder contains small EDN programs showcasing Phase 4 features.

Files
- `sums_match.edn` — Sum types (ADT) with `sum-new`, `match` (with binds and result-as-value).
- `generics.edn` — Reader-macro generics: `(gfn ...)` and `(gcall ...)` rewrites to monomorphized functions.
- `traits.edn` — Trait/vtable macro: define `Show`, build a vtable on the stack, make a trait object, call a method via `trait-call`.
- `closures.edn` — Record-based closures: `make-closure` and `call-closure` with a single captured value.
- `eh_try_catch.edn` — Try/catch with `(panic)`; behavior depends on EH flags.
- `coroutines.edn` — Minimal coroutine ops demo; gated by `EDN_ENABLE_CORO`.

Environment flags (when applicable)
- Debug info (optional): `EDN_ENABLE_DEBUG=1` to emit DWARF.
- Exceptions/EH: set `EDN_ENABLE_EH=1`, and choose `EDN_EH_MODEL=itanium|seh`; panic mode via `EDN_PANIC=abort|unwind`.
- Coroutines: `EDN_ENABLE_CORO=1` to enable lowering.
- Passes: `EDN_ENABLE_PASSES=1` to run the opt-in optimization pipeline when emitting.

Notes
- Traits/generics are expanded by macro passes prior to type checking.
- These are minimal examples intended for IR emission and verification; adapt as needed for runtime demos.
