# EDN-0011: Rustlite Incremental Roadmap (Post EDN-0010)

Status: Open
Target Release: Phase 5 follow-on (incremental drops)
Owner: (unassigned)
Created: 2025-08-26

## Summary
With EDN-0010 closed, Rustlite has a stable nucleus (functions, ints/bools, basic control flow, sums + simple match helpers, struct/indexing/closure/trait-object macro demos). This issue defines a pragmatic next slice of features that deliver clear expressive wins without embarking on full Rust semantics (ownership, full traits, generics, lifetimes). Each item is intentionally small, macro-first where possible, and independently valuable.

## Goals (Incremental, Low-to-Moderate Effort)
1. Data ergonomics: tuples & fixed-size arrays with literal and indexing sugar.
2. Safer enums: surface enum syntax + variant-only exhaustive match checking (simple exhaustiveness for closed set).
3. Quality-of-life control flow sugar: `?` operator macro for Result-like sums; `while let` macro wrapper using existing `rwhile` + `rmatch`.
4. Optional safety: bounds-checking mode for indexing macros under a feature/env flag.
5. Basic operator completion: bitwise ops (& | ^ << >>) + remainder (%) + corresponding compound assignment macros.
6. Minimal range literals and for-loop integration `(0..N)` expansion into init/cond/step pattern.
7. Closure polish: optional implicit capture inference (explicit list remains supported) – gated behind flag.
8. Enum payload ergonomics: simple variant constructor sugar `(Color::Red)` with optional payload list mapping to current sum ops.

## Non-Goals (Remain Deferred)
- Ownership/borrowing, lifetimes, move semantics.
- Generic type parameters / trait bounds / associated types.
- Full pattern syntax (guards, multi-pattern, nested destructuring).
- Async/await, concurrency primitives, memory management abstractions (Box/Vec).
- Macro_rules / procedural macro systems.
- Comprehensive diagnostics & lints (unused, dead code, borrow errors).

## Proposed Feature Details
### 1. Tuples
Surface: `(tuple %dst [ %a %b %c ])`, `(tget %dst <Ty> %tuple <index>)`.
Lowering: new tuple type alias to "struct-like" aggregate with positional fields or direct core aggregate if available.
Diagnostics: index out of bounds at expansion time if static.

### 2. Fixed Arrays
Surface literal: `(arr %dst <ElemTy> [ v0 v1 ... vN ])` producing pointer to element 0 or aggregate handle.
Indexing: reuse existing `rindex*`; optional bounds check flag `RUSTLITE_BOUNDS=1` inserts compare + conditional panic.

### 3. Simple Enum Surface Syntax
Surface: `(enum :name Color :variants [ (Red) (Green) (Blue) (Num (i32)) ])`.
Expands to existing `renum` + metadata for variant count; variant constructors: `(Color::Red)` or `(Color::Num %val)`.
Match sugar: `(match-enum %dst <RetTy> %val [ (Red <expr>) (Num (%n) <expr>) ] (else <expr>))` with exhaustiveness check when all variants present.

### 4. `?` Operator Macro
Use form: `(rtry %dst <RetTy> %expr)` inside a function returning `ResultT` (or OptionT) where `%expr` is a sum result.
Expands to `rmatch` pattern that early returns on Err / None, otherwise yields inner Ok / Some value.

### 5. `while let` Sugar
`(rwhile-let Variant %sumSym :bind %x :body [ ... ])` → loop with match each iteration, breaking on non-Variant.

### 6. Bounds Checking Mode
Environment flag `RUSTLITE_BOUNDS=1` causes `rindex-load` / `rindex-store` expansions to wrap in `(if (cmp ... ) (panic ...) ...)` or emit diagnostic in constant OOB cases.

### 7. Bitwise & Remainder Operators
Parser: recognize `& | ^ << >> %` with precedence tiers.
Lowering: map to core ops (add new intrinsic ops if absent) and macros for compound assigns: `rassign_and`, `rassign_or`, etc., or unified `(rassign-op %x <op> %rhs)`.

### 8. Range Literals + For Integration
Surface: `(rrange %dst i32 %start %end :inclusive false)` → struct `{start,end,inclusive}`.
For macro adaptor: `(rfor-range %it %range :body [ ... ])` expands to counted loop using temps.

### 9. Closure Capture Inference (Optional)
Flag `RUSTLITE_INFER_CAPS=1` triggers scan of closure body for free symbols; explicit `(captures [...])` list still allowed and overrides.
Fallback: if inference includes a mutable symbol, emit diagnostic (deferring borrow rules) or require explicit listing.

## Task Breakdown
Legend: [P#] Priority (1 = highest), Type: F=Feature, P=Parser, T=Test, D=Docs, R=Refactor.

### Phase A (Foundations)
- [x] [P1][F] Tuple macro forms `tuple` / `tget` + tests (`rustlite.tuple_basic`). Implemented with auto struct declaration injection for used arities (up to 16) and arity-aware `tget` resolution.
- [x] [P1][F] Fixed array literal `arr` + reuse indexing, plus legacy `rarray` numeric-size path. Added direct lowering to core `array-lit` for initialized form and `(alloca (array ...))` for size form; added `rindex-addr`, `rindex-load`, `rindex-store` refactor.
- [x] [P1][F] Literal macro restorations (`rcstr`, `rbytes`) with robust handling of parser-provided string nodes and interning conformity; all literal tests passing.
- [x] [P1][D] Docs: add Tuple & Array section + examples. (Completed 2025-08-27)

### Phase B (Enums & Matching)
- [ ] [P1][F] Enum surface macro `(enum :name ...)` + variant constructor forms.
- [ ] [P1][T] Exhaustive variant-only match test (`rustlite.enum_match_exhaustive`).
- [ ] [P2][T] Non-exhaustive match diagnostic test (`rustlite.enum_match_non_exhaustive`).
- [ ] [P1][D] Docs update: Enum surface syntax + exhaustiveness rules.

### Phase C (Control Flow Ergonomics)
- [ ] [P1][F] `rtry` macro for `?` operator semantics (Result path).
- [ ] [P1][T] `rustlite.rtry_result` test (Ok path + early Err short-circuit).
- [ ] [P2][F] Extend `rtry` to Option (None short-circuit) if type matches Option*.
- [ ] [P2][T] `rustlite.rtry_option` test.
- [ ] [P2][F] `rwhile-let` macro.
- [ ] [P2][T] `rustlite.rwhile_let` loop test.
- [ ] [P2][D] Docs: `?` and `while let` sections.

### Phase D (Safety & Operators)
- [ ] [P1][F] Bounds checking flag integration for `rindex*` (env `RUSTLITE_BOUNDS`).
- [ ] [P1][T] `rustlite.index_bounds_ok` / `rustlite.index_bounds_oob_neg` tests.
- [ ] [P1][P] Parser support for `%` remainder + bitwise ops & shifts.
- [ ] [P1][T] `rustlite.bitwise_ops` test driver.
- [ ] [P2][F] Compound assign macros (or generic `rassign-op`).
- [ ] [P2][T] `rustlite.compound_assign` test.
- [ ] [P2][D] Docs: Operator table update.

### Phase E (Ranges & Closure Polish)
- [ ] [P2][F] Range literal `rrange` and `rfor-range` adapter.
- [ ] [P2][T] `rustlite.range_for` test (iterates expected counts).
- [ ] [P3][F] Closure capture inference (flag gated) + fallback diagnostics.
- [ ] [P3][T] `rustlite.closure_infer` test (flag on) & `rustlite.closure_infer_disabled` (flag off).
- [ ] [P3][D] Docs: flags reference.

### Cross-Cutting
- [ ] [P2][R] Centralize feature flags in a single header `rustlite/features.hpp` with helper queries.
- [ ] [P2][D] Add "Feature Flags" doc section (bounds, capture inference).

## Acceptance Criteria
- All P1 tasks implemented, documented, and tests passing.
- Enum match exhaustiveness: passing exhaustive test and failing non-exhaustive test (diagnostic code reserved, e.g., `E1600`).
- Bounds checking mode demonstrably traps or errors on OOB in test; disabled mode behavior unchanged.
- `rtry` yields identical IR shape to explicit match early-return form.
- Bitwise / remainder ops produce valid IR and interact with existing precedence rules.
- Documentation sections updated (Tuples, Arrays, Enums, ?, while-let, Operators, Ranges, Feature Flags).

## Diagnostics (Proposed New Codes)
- E1600: Non-exhaustive enum match.
- E1601: Tuple index out of range (static).
- E1602: Array literal size mismatch (internal invariant / reserved for future).
- E1603: `rtry` used with non-Result/Option sum.
- E1604: Bounds check failure (runtime trap path) — may map to existing panic mechanism.

## Risks & Mitigations
| Risk | Mitigation |
|------|------------|
| Feature creep toward full pattern matching | Limit to variant-only + simple bindings; open new issue for guards/nesting. |
| Enum constructor ambiguity | Enforce `Type::Variant` tokenization at macro layer only (no parser changes yet if not ready). |
| Performance overhead of bounds checks | Guard by env flag; default off. |
| Capture inference false positives | Keep explicit list authoritative; emit note when inference adds symbols not in explicit list. |

## References
- `docs/RUSTLITE.md`
- `design/rustlite.md`
- Closed Issue: `EDN-0010` (baseline coverage)

## Progress Log
(Add entries as work proceeds.)
- 2025-08-26: Issue created with prioritized incremental roadmap.
- 2025-08-26: Added feature flags header `rustlite/features.hpp` and docs Feature Flags section (preparing Phase D/E groundwork).
- 2025-08-26: Struct declaration validation added (E1400–E1407) and tests (`phase3_struct_decl_test`) – groundwork quality tightening before tuple/array.
- 2025-08-26: Began Phase A: designed tuple (`tuple` / `tget`) and array (`arr`) macro surfaces.
- 2025-08-27: Restored lost macros (`rcstr`, `rbytes`, `rextern-global`, `rextern-const`) and refactored indexing sugar (`rindex`, `rindex-load`, `rindex-store`, added `rindex-addr`).
- 2025-08-27: Implemented tuple construction + `tget`; added automatic struct declarations (`__TupleN`) with placeholder i32 field types; `rustlite.tuple_basic` passing.
- 2025-08-27: Implemented `arr` macro lowering directly to core `array-lit` and enhanced `rarray` for numeric size allocation; added array indexing test driver (`rustlite.index_addr`) passing after driver alignment to core compare op shape.
- 2025-08-27: Hardened `rcstr` macro to wrap raw string nodes, preserving quotes & escape semantics; all literal/extern global tests pass.
- 2025-08-27: Added arity tracking in expansion for precise `tget` lowering (removed earlier `__Tuple16` placeholder assumption).

### Current Status (Phase A)
Functional macros in place and validated by drivers/tests: tuples, arrays, indexing (address, load, store), literals, extern globals. Documentation updates for tuples/arrays still pending. Placeholder field type strategy (i32) for tuple auto-decls flagged for future refinement.

### Immediate Next Actions
1. Documentation: add Tuple & Array sections with examples and note placeholder typing / future type inference improvement.
2. Decide on tuple field type inference approach (defer, or track constructor operand types post type-check pass in a future enhancement issue).
3. Begin Phase B planning (enum surface) once docs merged.
