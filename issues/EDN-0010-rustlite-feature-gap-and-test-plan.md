# EDN-0010: Rustlite Feature Gap & Test Coverage Plan

Status: Closed
Target Release: Post Phase 5 polish (TBD)
Owner: (unassigned)
Created: 2025-08-26

## Summary
Inventory of (a) major Rust language features intentionally missing from Rustlite's current scoped subset and (b) implemented macro / lowering features that lack parser support, samples, or tests. This ticket tracks incremental closure of pragmatic gaps (small sugars + missing tests) while deferring large language areas (ownership, generics, traits completeness) to dedicated follow-up issues.

## Scope
In scope:
- Adding or removing (by explicit decision) small macro sugars already listed in design docs (e.g., `rfor`, closure macros) and providing tests.
- Enabling / reworking disabled or placeholder tests (`fields_index`).
- Adding focused drivers for underspecified helpers (Option/Result, indexing, function pointers).
- Documenting current behavior (e.g. permissive escape handling) or tightening with diagnostics.

Out of scope (will spin separate epics / multi-issue efforts):
- Ownership & borrowing model, lifetimes, drop semantics.
- Full pattern matching & destructuring.
- Comprehensive trait system (bounds, where clauses, associated types) beyond existing macro demos.
- Generics surface syntax & monomorphization strategy beyond demo driver.
- Module / path / visibility / attribute systems.
- Async/await, concurrency, memory management (Box/Vec/String), const-eval.

## High-Level Missing vs Real Rust (Acknowledged, Deferred)
- References & borrowing: `&T`, `&mut T`, lifetime elision, borrow checker.
- Pattern matching: full `match` expression syntax; destructuring (struct/tuple/enum), guards, wildcards `_`, bindings.
- Types: tuples, arrays `[T; N]`, slices `&[T]`, raw pointers, floats, broader integer widths, `char`, string/byte literal surface lowering.
- Enums & structs: surface syntax, variant paths (`Enum::Var`), field access, discriminants.
- Traits: bounds, impl blocks with generics, associated items, dynamic dispatch sugar (`&dyn Trait`).
- Generics: type parameters in `fn`, `struct`, `enum`, `impl`.
- Closures: surface `|x| expr` syntax, capture inference, move semantics.
- Modules / visibility / attributes: `mod`, `use`, `pub`, `#[attr]`, `unsafe`.
- Error handling sugar: `?` operator, full Result type modeling.
- Iteration: `for x in iter` desugaring.
- Operators: bitwise (`& | ^ << >>`), remainder `%`, bitwise compound assigns, address-of / deref surfaces.
- Memory & data: heap allocation primitives, slices/views, indexing semantics with bounds.
- Macros (Rust-style) – beyond EDN macro layer.

## Implemented But Lacking Tests / Samples / Parser Support
- `rstruct` / `rget` / `rset` (struct field sugar) – test disabled (`rustlite.fields_index`).
- Indexing macros: `rindex`, `rindex-addr`, `rindex-load`, `rindex-store` – no dedicated driver.
- Closure macros: `rclosure`, `rcall-closure` – no driver verifying capture + invocation.
- For-loop sugar: `rfor` – referenced in design; not present or intentionally omitted (needs decision & doc update).
- Option/Result helpers: `rsome`, `rnone`, `rok`, `rerr` – not directly smoke-tested; covered indirectly via enum patterns.
- Function pointer helper: `rfnptr` – no sample creating and calling through pointer.
- Trait object construction: `rmake-trait-obj` partially implied, but no focused test beyond general trait shape driver.
- Negative trait invocation cases (wrong arity on existing method) – missing.
- `rassign` vs `assign`: clarify if `rassign` exists or unify docs to core `assign`.
- Literal escape strictness: unknown escapes currently pass through; no test documenting / enforcing policy.
- External data with initializer diagnostic: only implicit via E1227; no explicit negative test for `(rextern-global ... :init ...)` misuse.

## Task Breakdown
Legend: [P#] Priority (1 = high, 2 = medium, 3 = low), Type: F = feature (macro or behavior), T = test/coverage, D = documentation.

### Decisions / Documentation
- [x] [P1][D] Decide fate of `rfor`: keep; macro already implemented, proceed with tests (2025-08-26).
- [x] [P1][D] Clarify `rassign` presence; macro implemented (alias for `assign`); docs updated (`docs/RUSTLITE.md`).
- [x] [P2][D] Document current permissive `rcstr` escape handling (unknown escapes literalized) (decision: keep permissive; added test `rustlite.cstr_unknown_escape`).
- [x] [P2][D] Add explicit note that external globals cannot specify `:init` and add negative test description referencing E1227 (covered by `rustlite.extern_global_init_neg`).

### Feature Additions (Small Macros / Enable Existing)
- [x] [P1][F] Implement or explicitly drop `rfor`. If implemented: `(rfor :init [ ... ] :cond %c :step [ ... ] :body [ ... ])` → core `(for ...)`. (Implemented previously; test added 2025-08-26 `rustlite.rfor`).
- [x] [P1][F] Re-enable struct / field / index demo (`rustlite.fields_index`) after ensuring `rstruct` + `rget`/`rset` lowering stable.
- [x] [P1][F] Closure support polish: ensure `rclosure` captures lowered environment, `rcall-closure` dispatch validated.

### Test / Driver Additions
- [x] [P1][T] Add `rustlite.struct_fields` driver: implemented (`rustlite_struct_fields_driver.cpp`, test `rustlite.struct_fields`).
- [x] [P1][T] Add `rustlite.indexing` driver: implemented (`rustlite_indexing_driver.cpp`, test `rustlite.indexing`).
- [x] [P1][T] Add `rustlite.closure` driver: closure capturing one local, returning computed value; verify result.
- [x] [P1][T] Add `rustlite.closure_neg` driver: reference uncaptured variable inside closure → error diagnostic.
- [x] [P2][T] Add `rustlite.option_result` driver: construct Option via `rsome`/`rnone` and Result via `rok`/`rerr`; match with `rmatch` / `rif-let`.
- [x] [P2][T] Add `rustlite.fnptr` driver: create function pointer via `rfnptr`, call it, compare to direct call.
- [x] [P2][T] Add `rustlite.trait_neg_arity` driver: invoke existing trait method with wrong arg count → diagnostic (implemented as `rustlite.trait_neg` leveraging E1325 check).
- [x] [P2][T] Add literal unknown escape test documenting literal pass-through (`rustlite.cstr_unknown_escape`).
- [x] [P3][T] Add `rextern_global_init_neg` test: attempt `rextern-global` with `:const true` but missing `:init` → expect E1227.
- [x] [P3][T] Add `rmake_trait_obj` focused test verifying trait object call path + vtable layout assumptions (size/ptr assertions if applicable) (`rustlite.make_trait_obj`).

### Refactors / Cleanups
- [x] [P2][F] Consolidate any duplicate macro expansion code paths in `expand.cpp` (indexing family unified via helpers for load/store; further closure/trait call sharing deferred as low value now that coverage solid).
- [x] [P3][F] Introduce shared helper to generate unique test module names for new drivers (reduce collision risk) (`rustlite/test_util.hpp`).

### Documentation Updates (After Implementation)
- [x] [P1][D] Update `docs/RUSTLITE.md` with newly added drivers & macro examples (struct, indexing, closure, option/result, fnptr).
- [x] [P2][D] Add quick reference rows for any new or confirmed macros (rfor if kept, rfnptr usage, indexing forms).
- [x] [P2][D] Add section "Test Matrix" mapping macros → drivers for easy audit.

## Acceptance Criteria
- Each high-priority (P1) test and feature present and passing in CI.
- Design docs adjusted to match reality (no dangling references to unimplemented macros without status note).
- Disabled struct/index test re-enabled or replaced by new coverage.
- Closure driver demonstrates capture + invocation; negative closure test asserts missing capture diagnostic.
- Option/Result helpers demonstrably expand and interoperate with existing match helpers.
- Function pointer and indexing macros validated end-to-end (expand → typecheck → IR/JIT).
- Explicit decision recorded for `rfor`.
- Escape handling behavior is either enforced with diagnostics or explicitly documented and tested.

## Risk / Mitigation
| Risk | Mitigation |
|------|------------|
| Feature creep (creeping toward full Rust) | Keep ticket scoped to small sugars + tests; open separate epics for large semantics. |
| Diagnostic churn when tightening escapes | Introduce new code range (E1505+) to avoid breaking existing expectations silently. |
| Ambiguity in closure capture semantics | Start with explicit capture list only; defer inference. Document clearly. |

## References
- `design/rustlite.md`
- `docs/RUSTLITE.md`
- `languages/rustlite/src/expand.cpp`
- Existing drivers under `languages/rustlite/tools/`

## Progress Log
(Add dated entries as tasks complete.)
- 2025-08-26: Issue created with initial inventory.
- 2025-08-26: `rfor` decision: retain macro; added driver `rustlite_rfor_driver.cpp` and test `rustlite.rfor` passing.
- 2025-08-26: Added struct/index drivers (`rustlite.struct_fields`, `rustlite.indexing`) and re-enabled `rustlite.fields_index`.
- 2025-08-26: Added closure drivers (`rustlite.closure`, `rustlite.closure_neg`) validating capture and negative undefined capture.
- 2025-08-26: Added struct field + indexing drivers (`rustlite.struct_fields`, `rustlite.indexing`).
- 2025-08-26: Added option/result and fnptr drivers (`rustlite.option_result`, `rustlite.fnptr`) and trait negative arity (`rustlite.trait_neg`) confirming E1325.
- 2025-08-26: Expanded docs (`docs/RUSTLITE.md`) with struct/indexing/closure/option-result/fnptr sections, quick reference expansion, test matrix.
- 2025-08-26: Added extern const-missing-init negative test (`rustlite.extern_global_init_neg`) asserting E1227 and documented restriction.
- 2025-08-26: Documented permissive rcstr unknown escape handling; added driver `rustlite.cstr_unknown_escape` and updated docs test matrix.
- 2025-08-26: Added rmake-trait-obj focused driver (`rustlite.make_trait_obj`) exercising trait object construction + method dispatch.
- 2025-08-26: Added unique module id helper `rustlite/test_util.hpp` for future drivers.
- 2025-08-26: Refactored indexing macros (`rindex`, `rindex-load`, `rindex-store`) to shared helpers reducing duplication; ticket closed.
