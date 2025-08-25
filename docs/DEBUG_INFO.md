# Debug Information Strategy (Phase 4)

Status: Draft (2025-08-24)

This document summarizes the design and current implementation of EDN's LLVM
Debug Info (DI) emission after the Phase 4 modularization and refactors tracked
under EDN-0004 / EDN-0007.

## Goals

- Provide source-level traceability for generated LLVM IR sufficient for
  inspecting sums, matches, closures, and coroutine scaffolding with `lldb`
  or `llvm-dwarfdump`.
- Keep the frontend architecture modular: DI emission logic is centralized in
  `ir/di.cpp` and thin helpers, avoiding ad-hoc `DIBuilder` calls spread across
  unrelated lowering code.
- Support lexical scoping and true variable shadowing (distinct allocas / SSA
  values, separate `DILocalVariable` entries).
- Validate correctness with lightweight IR tests (no external debugger
  dependency): member offset tests, scope tests, and match binding offset tests
  (including DI cross‑checks).

## Scope & Non-Goals

Covered:
- Function `DISubprogram` creation.
- Lexical blocks for `(block ...)` constructs and implicit top-level function
  body scope.
- Local variable emission via `dbg.declare` for addressable slots and
  `dbg.value` for pure SSA temporaries (match binds, closure captures).
- Struct and sum type member debug metadata (composite + derived types).
- Field offset validation tests to prevent divergence between layout and DI.

Deferred / Future:
- Full coroutine frame variable mapping (currently only the coroutine function
  itself gets `DISubprogram` + basic locals; frame internals are opaque).
- Rich source file / line mapping (current line scheme is a monotonic counter).
- Windows CodeView specific tuning (skipped for now per EDN-0007 Part C).

## Architecture

Central components:

| Component | Responsibility |
|-----------|----------------|
| `setup_function_entry_debug` | Create `DISubprogram`, initialize lexical scope, prime line counter. |
| `declare_local` | Decide between `dbg.declare` and `dbg.value` based on storage and create `DILocalVariable`. |
| `finalize_module_debug` | Call `DIBuilder::finalize()` once per module and perform any late attachment. |
| `emit_struct_type_debug` / helpers | Build `DICompositeType` + `DIDerivedType` chain for aggregate layouts. |

A per-module `DIBuilder` instance lives in the lowering state. Each function
pushes/pops lexical scopes as we enter blocks.

### Line Mapping

Rather than attempting to preserve original EDN source coordinates (which are
not yet tracked through the parser), we assign incrementing pseudo line numbers
when emitting instructions / locals. This enables liveness and scope queries in
basic debugger sessions and is stable for regression tests.

### Locals & Shadowing

`declare_local` accepts a variable name and a storage descriptor:
- If a stack slot (alloca) exists, we use `dbg.declare` so debuggers can take
  the address and watch the slot.
- If there is only an SSA value (e.g., after a `match` bind or constant
  folding), we use `dbg.value`.

Shadowing: entering a new lexical block with a variable of the same name creates
an independent `DILocalVariable`; the scopes test ensures we register three
shadowed locals with distinct allocas and that they appear separately in the IR
(debug intrinsics referencing different pointer values).

### Struct & Sum Members

When a struct (or sum backing struct) type is instantiated, we build a
`DICompositeType` where each field becomes a `DIDerivedType` with an offset in
bits. Offsets are computed from LLVM's `DataLayout` to ensure canonical source
of truth. Tests:
- `phase4_debug_info_struct_member_offsets_test.cpp`
- `phase4_match_binding_offsets_test.cpp` (cross-checks DI offsets for bound
  variant fields vs GEP immediates)

### Match Bindings

Variant field binds produce raw GEPs named `<bind>.raw` into the sum's payload
structure. We immediately emit `dbg.value` for the loaded SSA values and rely on
naming stability. The offsets test asserts both:
- GEP index / constant offset matches expected packed layout (e.g., i32 at 0,
  i64 at 4)
- Corresponding `DIDerivedType` field offsets in bytes match the same values.

### Closures

Closure thunks receive a synthetic environment parameter (pointer to captured
struct). We:
1. Emit the struct type DI (captures as fields).
2. Emit a `DILocalVariable` for the environment pointer param.
3. Optionally emit locals for individual captures if materialized separately.

### Coroutines

Current coroutine support focuses on intrinsic sequencing and minimal attribute
validation. We emit `DISubprogram` entries and baseline locals; coroutine frame
layout & promise field DI are not yet modeled (future enhancement—tie into
`coro.promise` lowering once stable).

### Panic / EH Paths

Exception handling paths (Itanium landingpads, Windows cleanuppads when added)
are emitted within the same lexical scope chain; no special DI constructs are
needed for Phase 4 beyond consistent line numbering.

## Testing Strategy

| Test | Purpose |
|------|---------|
| `phase4_debug_info_scopes_test.cpp` | Confirms shadowed locals each have DI entries. |
| `phase4_debug_info_struct_member_offsets_test.cpp` | Verifies member offsets in DI vs `DataLayout`. |
| `phase4_match_binding_offsets_test.cpp` | Verifies sum variant field GEP and DI offsets. |
| `phase4_coro_*` tests | Ensure coroutine lowering doesn't regress DI finalization (presence of `DISubprogram`). |

All tests use lightweight IR inspection (no external debugger invocation). This
keeps CI fast and deterministic.

## Future Work

- Replace pseudo line counter with real source span tracking once parser retains
  positions for tokens.
- Model coroutine frame structure & promise object in DI (map to synthetic
  composite type) and add tests.
- Provide a script (Phase I task) to grep for expected DI patterns (e.g.,
  `!DILocalVariable(name: "x"`) and fail if missing; integrate into CI.
- Windows CodeView validation pass after macOS parity tasks are complete.

## Reference

- LLVM Language Reference: Debugging Information
- Source modules: `ir/di.cpp`, `ir/sum_ops.cpp`, `ir/closure_ops.cpp`,
  `ir/control_ops.cpp`

---
(Generated as part of EDN-0007 documentation tasks.)
