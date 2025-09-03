# EDN-0014: Rustlite Remaining Surface Roadmap

Status: Draft
Owner: (assign)
Created: 2025-08-29
Related Epics: `epics/epic_surface_languages.md`, `epics/epic_macro_system.md`, `epics/epic_diagnostics_tooling.md`

## Goal
Define the remaining Rust-inspired surface features to reach a **pleasant MVP** for Rustlite before pivoting to diagnostics hardening, ABI v0, and a second surface language. Provide prioritization, grouping, and acceptance criteria to enable incremental implementation with low churn.

## Non‑Goals
- Full Rust language parity.
- Advanced borrow checker (full dataflow / region inference).
- Procedural macros / macro_rules hygiene fidelity.
- Complex module/package system (crates, extern crates, multi-file graph).

## Prioritization Legend
- P0 (MVP Must): Required for the target “pleasant” baseline (ship before pivot).
- P1 (Nice): Valuable but can follow immediately after pivot if time allows.
- P2 (Defer): Keep on long tail backlog; implement only if specifically unblocked by other work.

## Summary Table
| ID | Feature | Group | Priority |
|----|---------|-------|----------|
| 1  | Struct field init shorthand / update syntax | Data & Patterns | P1 |
| 2  | Tuple/struct pattern destructuring (let / match arms) | Data & Patterns | P0 |
| 3  | Pattern alternation (`|`) in match arms | Data & Patterns | P1 |
| 4  | Range patterns in matches | Data & Patterns | P2 |
| 5  | Slice / array prefix patterns | Data & Patterns | P2 |
| 6  | Match guards (`if <cond>`) | Data & Patterns | P1 |
| 7  | Type aliases | Types & Generics | P0 |
| 8  | Multi-parameter generic fns (complete) | Types & Generics | P0 |
| 9  | Basic trait bounds (`T: Trait`) | Types & Generics | P0 |
| 10 | Associated types (minimal) | Types & Generics | P1 |
| 11 | Const generics stub (length passthrough) | Types & Generics | P2 |
| 12 | `&` / `&mut` reference value forms | Ownership | P0 |
| 13 | Deref sugar (`*x`) lowering | Ownership | P0 |
| 14 | Simple borrow checker pass (exclusive mut, shared immut) | Ownership | P1 |
| 15 | Move vs Copy semantics audit (Copy marker trait stub) | Ownership | P1 |
| 16 | Slice type `&[T]` | Ownership | P1 |
| 17 | `str` / string literal split (interned vs owned) | Ownership | P2 |
| 18 | Iterator adaptor stub (`into_iter` on arrays) | Control Flow | P2 |
| 19 | `loop {}` with value on `break` | Control Flow | P0 |
| 20 | `while let` pattern lowering extension | Control Flow | P0 |
| 21 | `if let` sugar | Control Flow | P0 |
| 22 | Labeled `continue` / `break` | Control Flow | P2 |
| 23 | Result propagation refinement (post `rtry`) | Error Handling | P0 |
| 24 | `panic!` surface macro sugar | Error Handling | P1 |
| 25 | Option unwrap/expect macro (`runwrap`/`rexpect`) | Error Handling | P1 |
| 26 | Trait definitions (methods complete) | Traits & Impls | P0 |
| 27 | Impl blocks for concrete types | Traits & Impls | P0 |
| 28 | Derive-like marker macros (`Debug`, `Copy`) | Traits & Impls | P1 |
| 29 | Trait object safety checks (minimal) | Traits & Impls | P2 |
| 30 | `dyn Trait` sugar | Traits & Impls | P1 |
| 31 | Single-file `mod` declarations | Modules | P0 |
| 32 | `pub` visibility export (symbol retention) | Modules | P0 |
| 33 | `use` / import alias (flat) | Modules | P1 |
| 34 | Namespace collision diagnostics | Modules | P1 |
| 35 | Exhaustiveness checker improvements (wildcards + guards) | Tooling/Quality | P0 |
| 36 | Duplicate match arm detection | Tooling/Quality | P0 |
| 37 | Unused variable / dead code warnings (basic) | Tooling/Quality | P1 |
| 38 | Lint scaffold + suppression attribute | Tooling/Quality | P1 |
| 39 | Restricted hygienic macro_rules-lite | Macros/Misc | P2 |
| 40 | Attribute capture/preserve on items | Macros/Misc | P1 |
| 41 | Simple `cfg` flag handling (strip blocks) | Macros/Misc | P2 |
| 42 | Inline consts in expressions | Macros/Misc | P1 |

MVP (P0) Count: 17
Nice-to-have (P1): 17
Defer (P2): 8

## MVP (P0) Acceptance Criteria Details
### Patterns & Control Flow
2. Destructuring: `let (a, b) = tup;` and `match v { (x, _) => ... }` lowers to tuple element extracts (`tget`) with binding diagnostics on arity mismatch (E-code TBD).
19. `loop { ... break <expr>; }` lowers to synthetic block capturing the break value with type consistency check.
20/21. `while let` / `if let`: Pattern against `Option*/Result*` expands to existing match lowering; unreachable pattern arms flagged.

### Types & Generics
7. Alias: `type MyInt = i32;` only textual substitution at expansion stage; appears in diagnostics index.
8/9. Generics & bounds: Parameterized fn duplication (monomorphization) plus simple bound check verifying required trait methods exist.

### Ownership
12/13. References & deref: Introduce `ref` / `deref` IR ops or macro expansions; type checker enforces pointer/reference categories.

### Error Handling
23. Result propagation: `rtry` already emits; add mapping macro(s) or improved diagnostic for mixing Option/Result incorrectly (E1603 done) + mismatched binding shape.

### Traits & Impls
26/27. Traits and impl blocks: Method dispatch stable (existing drivers) + impl insertion semantics; duplicate impl detection.

### Modules
31/32. Single-file `mod name { ... }` collects items under a namespace; `pub` marks names for export listing used by later linking/ABI planning.

### Tooling / Diagnostics
35. Exhaustiveness: Detect missing variants for enums & unmatched wildcard; guard presence defers arm requirement.
36. Duplicate arm: Same pattern (structurally identical) flagged with new error code.

## Cross-Cutting Diagnostics
- Each new surface construct must either produce a dedicated error code path or be explicitly documented as lowering to existing IR validations.
- Update `scripts/gen_error_index.py` run to include new codes.

## Implementation Order (Proposed)
1. Destructuring (2)
2. while/if let (20/21)
3. loop-with-value (19)
4. Type alias (7)
5. Multi-param generics + bounds (8/9)
6. References + deref (12/13)
7. Result propagation refinement (23)
8. Trait + impl completion (26/27)
9. Modules + pub (31/32)
10. Exhaustiveness + duplicate arm (35/36)

Then branch into parallelizable P1 items.

### Implementation Progress (Rolling)
| Order | Feature(s) | Status | Notes / Next Steps |
|-------|------------|--------|--------------------|
| 1 | Destructuring (2) | Phase 1 complete (let tuples + struct) | Tuple let patterns implemented (cluster → member) with diagnostics (E1454 arity mismatch, E1455 non-tuple) and over-arity aggregation. Struct let patterns implemented: member op emission, placeholder `_` skipping, unknown field (E1456) and duplicate field (E1457) diagnostics, harvested into TypeChecker. Remaining (Phase 2): match arm tuple/struct patterns, nested patterns, aliases, rest (`..`), duplicate binding names, gap robustness tests. |
| 2 | while/if let (20/21) | Complete | Macros (`rif-let`, `rwhile-let`) implemented; negative tests for missing / type mismatch / variant mismatch (E1421/E1422/E1423/E1405) and undefined :value symbol (E1421) landed. |
| 3 | loop-with-value (19) | Complete | `rloop-val` macro validated. Test matrix: break with value, plain break (retain zero-init), no break body, multi break (first wins), negative type mismatch (E1107). |
| 4 | Type alias (7) | Complete | Positive + negatives (E1330–E1333) plus cycle (self-ref) and sum shadow tests landed (E1333/E1401). |
| 5 | Generics + bounds (8/9) | Partially complete | Core single & multi-parameter parsing, monomorphization, and bounds diagnostics (E1700–E1702) implemented; specialization IR test green. Remaining: broaden multi-param instantiation matrix, bound failure negative tests (missing trait method), redundant bound handling policy. |
| 6 | References + deref (12/13) | Pending | Needs IR op or macro design draft. |
| 7 | Result propagation refinement (23) | Pending | Extend `rtry` diagnostics (Option/Result mixing already E1603). |
| 8 | Traits + impl (26/27) | Pending | Baseline dispatch exists; need completeness + duplicate impl diagnostics. |
| 9 | Modules + pub (31/32) | Pending | Namespace + export retention design TBD. |
| 10 | Exhaustiveness + duplicate arm (35/36) | Pending | Exhaustiveness algorithm + structural pattern hash for duplicate detection. |

Progress summary (current cycle): Completed if/while-let negatives (E1405, E1421–E1423); type alias suite incl. cycle + shadow; began generics/bounds planning. Fixed compound assignment RHS literal trimming regression (shift operators) – numeric literal now preserved (removed spurious zero const). Removed temporary debug logging and legacy unordered_set include from parser. Implemented tuple pattern over-arity detection (meta + backward scan over `tget`/`member`) and added over-arity negative driver; gapped pattern driver asserts no false positives.

### Recent Progress Log (2025-09-01 - Update)
Destructuring enhancements:
* Added struct let pattern lowering with `(member ...)` emission.
* Implemented placeholder `_` skip logic and `(struct-pattern-meta ...)` summarization.
* Added duplicate field detection meta → expansion diagnostic E1457.
* Added unknown field validation against struct declarations → E1456.
* Harvested E1454–E1457 expansion diagnostics into TypeChecker unified error channel.
* Added positive + negative struct pattern tests (unknown field, duplicate field, placeholder) and migrated them to rely on TypeChecker errors.
* Updated `docs/RUSTLITE.md` with struct destructuring section.
Next (Destructuring Phase 2): match arm tuple/struct patterns, nested patterns, alias/rest syntax, duplicate binding detection, gap ordering robustness tests.

### Recent Progress Log (2025-08-31)
Tuple pattern & match infrastructure advances:
* Implemented multi-arm tuple match lowering: parser now emits per-arm extraction clusters (`tget` + `(tuple-pattern-meta ...)`) followed by synthesized guard predicates and a reversed `rif` chain selecting the first satisfied arm. Destination is pre-zero-initialized; each taken arm assigns final value.
* Added literal element guard generation: integer literals in arm patterns generate `(const)` + `(rcall eq)` predicates, combined via `(rcall band)` for conjunction. Unconditional arms receive a synthesized true const `(const %c i1 1)`.
* Introduced chain terminator pattern (empty `[]`) for final `:else` in `rif` chain enabling simple linearization.
* Added tuple pattern over-arity detection (meta path + backward scan) and drivers:
	- `rustlite_match_tuple_over_arity_negdriver` (E1454 on highest out-of-range index)
	- `rustlite_match_tuple_gapped_driver` (asserts no E1454/E1455 for non-dense indices)
* Began guard-focused positive test (`rustlite_tuple_match_literal_guard_test`) validating lowering + type check (currently placeholder assertions; IR structure verification TODO).
Deferred / upcoming:
* Lazy extraction: only emit cluster for arms reachable given preceding guards (current implementation eagerly emits all clusters).
* Extend guards beyond integer equality (planned: variable equality, simple relational ops, future boolean expression embedding).
* Parser warning cleanup (unused `inclusive` flag) scheduled post guard generalization.

### Recent Progress Log (2025-08-29)
Added after parser/lowering stabilization work in active debugging session.

Completed / Fixed:
- Literal lowering regressions resolved: return statements no longer capture trailing semicolons; numeric literals (including negatives) now preserve correct values.
- Negative integer literals normalized to (sub 0 <pos>) pattern to align with golden IR expectations (stability for future diff-based tests).
- Assignment & compound assignment expression trimming prevents spurious zero constants (confirmed via compound_shift surface test; RHS '3' lowered correctly).
- Removed temporary compound assignment debug instrumentation and unused unordered_set include (eliminates intermittent stale build confusion).
- Generics specialization IR test passes post parser adjustments (no regressions introduced).

Outstanding (Short-Term Next):
- Run broader surface test suite to flush any remaining literal/operator edge cases beyond validated unary minus & assignment scenarios.
- Refactor outdated type alias negative driver sources referencing removed Builder::raw / fn_raw APIs (blocking a subset of negative tests from compiling).
- Address sign-conversion warning in `expand.cpp` (iterator vs index erase loop) to keep warning budget clean.
- Silence trivial parser warning (unused 'inclusive' flag in for-range lowering) or implement inclusive semantics.
- Add regression tests for: compound assignment with negative literal, nested negative literals inside generic instantiations, and alias use across generic specialization boundaries.

Risk Watch:
- Gensym naming dependency for IR golden files (currently uses "add" base before subtraction step). Any future change must update goldens atomically.
- Potential unseen parse edge cases for expressions ending in comment + semicolon; add targeted tests if encountered.

Planned Follow-Up (post-cleanup):
- Finalize remaining generics bound failure diagnostics (missing method usage scenarios) and duplicate bound policy (warn vs accept silently).
- Expand multi-parameter generics instantiation test matrix (distinct type arg ordering & reuse) to guard specialization cache correctness.

## Risks & Mitigations
- Scope Creep: Strictly freeze after P0 list; create separate issues for any discovered sub-features.
- Diagnostic Drift: Add unit tests for every new E-code; regenerate errors index in CI pre-commit.
- Borrow Checker Complexity: Keep P1 (14) minimal (single-pass aliasing rules) to avoid blocking P0 timeline.

## Open Questions
- Do we need early slice type (16) for second language interop, or can we defer until ABI v0 work? (Leaning defer -> P1 remains.)
- Should inline consts (42) be required for ergonomics in pattern contexts? (Monitor after destructuring.)

## Definition of Done (Issue)
- All P0 features have individual tracking subtasks or linked PRs.
- Documentation updated (`docs/RUSTLITE.md`) summarizing implemented subset.
- Errors index regenerated; no orphan codes.
- At least one negative test per new lowering path.

---

Add comments below for refinements or reprioritization.

## Negative Test Backlog (Tracking)
Pending / planned negative (and a few edge positive) tests to ensure every lowering path and diagnostic is exercised. Items move to crossed-out once implemented.

### Control Flow Macros
- [x] rloop-val: break without :value (spec: allowed; destination retains zero-init; test added).
- [x] rloop-val: multiple rbreak :value statements (first break assigns and exits; later presently unchecked/unreachable; test added).
- [x] rloop-val: no rbreak in body (positive: result is zero-init; test added).
- [x] rloop-val: break value type mismatch -> E1107 (assign mismatch).

### if/while let (rif-let / rwhile-let)
- [x] Missing :value in case arm -> E1421.
- [x] Missing :value in default arm -> E1422.
- [x] Case arm :value type mismatch vs default -> E1423.
- [x] Default arm :value type mismatch vs case -> E1423.
- [x] Pattern variant mismatch (rif-let) -> E1405.
- [x] :value references undefined variable (rif-let) -> E1421.
- [ ] Pattern variant mismatch (rwhile-let parity) -> expect E1405.
- [ ] :body references undefined symbol in rwhile-let loop body result position -> E1421.
- [ ] Unused bound variable warning (future when unused-variable lint lands) – placeholder.

### Match / Pattern Infrastructure (prep for destructuring)
- [x] Tuple pattern arity mismatch – E1454 (implemented in expansion; negative driver added).
- [x] Tuple pattern non-tuple target – E1455 (implemented; negative driver added).
- [ ] Match arm tuple pattern lowering (emits same `tget` cluster per arm) – add positive + E1454/E1455 negatives.
- [ ] Struct pattern destructuring (let & match) – field name mapping; unknown field / missing field diagnostics (new E-codes TBD).
- [ ] Nested tuple patterns (e.g. `let (a,(b,c)) = ...`) – recursive cluster emission plan & tests.
- [ ] Placeholder `_` bindings (skip variable introduction) – ensure not emitted as `tget %_` and adjust clustering logic.
- [ ] Duplicate binding names in a single pattern – new diagnostic (reuses existing redefinition code path or new E-code TBD).
- [ ] Out-of-order / gapped indices should NOT trigger tuple pattern diagnostics (add negative test to assert suppression).
- [x] Over-arity pattern producing out-of-range `tget` indices: decided to emit single aggregate E1454 (highest index +1) instead of per-index OOB; implemented meta and backward scan; tests added (over-arity & gapped). Follow-up: ensure future per-index OOB diagnostics suppressed when part of recognized pattern cluster.

### Destructuring Remaining Gaps (Detail)
Phase 1 delivered only tuple destructuring for `let` statements. The following remain to fully satisfy roadmap item (2):
1. Match arm tuple patterns: Parser support to emit clustered `tget` sequences inside arm prologue prior to arm body lowering.
2. Struct patterns: Syntax draft (Rust-like `StructName { a, b: alias, .. }`) and lowering to `member` reads; decide on support for `..` rest (likely defer to P1/P2).
3. Nesting: Recursive descent to flatten nested tuples into ordered `tget` emission while preserving binding order.
4. Placeholder `_`: Skip binding emission & exclude from cluster size (affects arity mismatch logic—need to count underscores as elements for arity check while not generating destinations).
5. Duplicate names: Detect and emit dedicated diagnostic before expansion; pick code range (suggest E1456+) or reuse existing redefinition path with clearer message.
6. Index ordering robustness: Add tests ensuring non-dense or out-of-order manual `tget` sequences do not spuriously emit E1454/E1455 (current heuristic suppresses but untested).
7. Over-arity semantics: Decide precedence between per-index out-of-range diagnostic vs aggregate arity mismatch; may emit both or prefer aggregate.
8. Struct pattern partial / missing fields: Plan diagnostics for omitted mandatory fields vs use of a rest pattern (if adopted).
9. Exhaustiveness integration: Ensure tuple/struct patterns participate in future duplicate arm & exhaustiveness passes (features 35/36).
10. Documentation: Expand RUSTLITE.md once match/struct patterns land; current section documents only tuple let patterns.
\n+### Draft Pattern Syntax (Proposal)
Status: Working draft. Adjust before parser implementation.
\n+#### Tuple Patterns in Match Arms
Surface:
```
match v {
	(x, y) => expr1,
	(a, _, c) => expr2,
}
```
Lowering (arm prologue): bind scrutinee `%scrut`; emit dense `tget` for non-`_` entries preserving index order. `_` counts toward arity but produces no SSA var (implementation may temporarily emit a throwaway `%_ignoreN`). Reuse E1454/E1455.
\n+#### Struct Patterns
Let:
```
let Point { x, y } = p;
let Rect { width: w, height, .. } = r;
```
Match:
```
match p { Point { x, y } => expr, _ => expr2 }
```
Grammar sketch:
```
StructPattern := Ident '{' FieldPatList [ ',' RestPat ] '}'
FieldPat      := Ident (':' Ident)?
RestPat       := '..'
```
Diagnostics (proposed): unknown field (reuse E0803 or new E1456), duplicate field (E1457), bad/misplaced `..` (E1458), non-struct target (reuse E0805 or E1459). Rest currently ignored (no binding) in Phase 1.
\n+#### Nested Patterns
Example: `let (a, (b, c), Point { x, .. }) = value;`
Option A (flatten): only emit leaf `tget` ops sequentially for all tuple leaves; record mapping for diagnostics.
Option B (recursive): emit intermediate `tget` temps then descend. Start with flatten (simpler clustering) then revisit.
\n+#### Placeholder `_`
Counts for arity; no binding; unlimited occurrences. Implementation path: emit dummy vars initially, later teach cluster logic to accept sparse-with-placeholders metadata.
\n+#### Duplicate Bindings
Detect during parse before lowering. Reserve E145A (or sequential after final chosen). Avoid generating conflicting SSA definitions.
\n+#### Out-of-Order / Gapped Indices
Ensure user-authored non-dense `tget` sequences don’t trigger pattern diagnostics; add negative test to lock in suppression.
\n+#### Over-Arity Handling
Prefer single E1454 over multiple index OOB diagnostics; suppress per-index error when cluster recognized as pattern with > arity elements.
\n+#### New E-Code Reservation Table (tentative)
| Code  | Purpose                          | Reuse Option |
|-------|----------------------------------|--------------|
| E1456 | Struct pattern unknown field     | could reuse E0803 |
| E1457 | Struct pattern duplicate field   | new |
| E1458 | Struct pattern bad `..` usage    | new |
| E1459 | Struct pattern non-struct target | could reuse E0805 |
| E145A | Duplicate binding in one pattern | new |
\n+#### Parser Pseudocode Sketch
```
parsePattern():
	if '(' -> parseTuplePattern()
	if Ident then lookahead '{' -> parseStructPattern()
	if '_' -> Placeholder
	else Ident -> Binding(name)
```
\n+#### Lowering Outline
```
lowerLetPattern(p, val):
	if Tuple: for each element i:
		if Placeholder continue
		emit (tget %dest Ty val i)
		recurse if nested
	if Struct: for each field:
		emit (member %dest Struct val field)
```
\n+#### Test Matrix Additions
| Case | Expect |
|------|--------|
| `(a, b)` vs `__Tuple2` | success |
| `(a, b)` vs `__Tuple3` | E1454 |
| `(a, b, c, d)` vs `__Tuple3` | E1454 only |
| `(a, _, c)` | skip middle binding |
| `(a, a)` | duplicate binding diag |
| `Point { x, y }` | two member ops |
| `Point { z }` | unknown field diag |
| `Point { x, x }` | duplicate field diag |
| `Point { .., x }` | misplaced `..` diag |
| Nested `(a,(b,c))` | flattened leaf ops |
\n+#### Phasing
Phase 2a: Match arm tuple patterns.
Phase 2b: Struct patterns.
Phase 2c: `_` + duplicate binding diagnostics.
Phase 2d: Nesting + over-arity suppression.
Phase 2e: Finalize codes & docs.

- [ ] Duplicate match arm structurally identical (after feature 36) – new E-code.
- [ ] Non-exhaustive match without wildcard or all variants (after feature 35) – existing/new E-code.

### Error Handling Macros
- [ ] rtry mixing Result/Option categories already E1603 – add explicit negative for Option vs Result mismatch in while/if let contexts.
- [ ] rtry inside rloop-val with type mismatch of propagated value vs loop result (combined scenario) – ensure only one clear diagnostic.

### Type Aliases (remaining)
- [ ] Alias used in generics instantiation mismatch (alias expands, underlying mismatch) – ensure underlying diagnostic still references alias name.
- [x] Self-referential cycle -> E1333 (cycle detection added).
- [x] Shadowing existing type name (sum) -> E1401 (sum redefinition path).
- [x] Missing :name -> E1330.
- [x] Missing :type -> E1331.
- [x] Redefinition -> E1332.
- [x] Invalid underlying form -> E1333.

### Generics / Bounds
- [ ] Generic function parameter syntax (`rfn :generics [ T U ]`) parse & expand.
- [ ] Monomorphization of multi-parameter generic functions (instantiate distinct copies).
- [ ] Trait bound syntax placeholder (`:bounds [ (bound T TraitName) ]`).
- [ ] Missing trait bound method use (invoke method requiring bound not declared) – bound failure diagnostic.
- [ ] Redundant duplicate bound listing (warn or accept) – placeholder.
- [ ] Monomorphization failure due to unresolved associated type (after assoc types minimal) – diagnostic placeholder.

### Ownership (future when refs land)
- [ ] Mut borrow while immutable borrow active – borrow checker P1 test (placeholder, labeled for later).
- [ ] Immutable borrow after move (ensure move semantics enforced) – test after move vs copy audit.

### Traits & Impl
- [ ] Duplicate impl block for same (Type, Trait) pair – duplicate impl diagnostic.
- [ ] Missing method implementation required by trait – trait completeness diagnostic.

### Modules (after introduction)
- [ ] Duplicate `mod` name in same scope.
- [ ] `pub` item not actually exported (symbol table retention failure) – regression guard.

### Lints / Tooling (future)
- [ ] Unused variable (basic) – once lint scaffold exists.
- [ ] Dead code function never referenced.
- [ ] Duplicate match arm (see above) – ensure both tooling + semantic test cover.

---
Maintenance:
- Keep this list trimmed: when an item lands in main with tests, flip to [x] and (optionally) annotate PR/commit hash.
- Introduce new bullets only if they represent distinct diagnostic behavior, not duplicates of already-covered code paths.

## 2025-08-31 update — tuple match literal guard lowering & protective snapshot

Summary
- Implemented multi-arm tuple match lowering for Rustlite surface `match t { (pat){..} ... }` producing:
  - Per-arm extraction clusters: `(tget %sym i32 %scrut <idx>)` + `(tuple-pattern-meta %scrut <arity>)` (one meta per arm).
  - Literal guard predicates: integer literals generate `(const)` + `(eq ...)` comparisons combined via `(band ...)` (currently appearing as nested `(if ...)` chain post-expansion; rif canonicalization deferred).
  - Destination pre-zero-init + per-arm `(assign %dst %valueSym)` inside conditional path.
- Added `rustlite_tuple_match_literal_guard_test` asserting:
  - >=3 `tuple-pattern-meta` (cluster per arm for this test shape).
  - Either a rif chain (future) or nested if chain length >=2.
  - >=2 `(eq ...)` literal guard comparisons (first two discriminatory arms).
- Fixed parser arm body scan depth bug (body brace depth initialized incorrectly); previously aborted lowering (`ok=0`).
- Removed all temporary debug instrumentation (match RHS, per-arm summaries, IR dump) after green test; kept robust RHS brace capture logic (now silent).

Rationale
- Capturing this state before type checker macro-op integration (tget/tuple-pattern-meta/tuple-match-arms-count) in case forthcoming edits regress lowering.

Next (planned immediately after this snapshot)
1. Extend type checker to recognize `tget`, `tuple-pattern-meta`, `tuple-match-arms-count`, and nested if chain semantics for match selection.
2. Re-enable type checking path within the guard test (currently structural-only) once ops validated.
3. Optional optimization: lazy cluster emission (skip extraction for arms ruled out by prior literal guards).
4. Broaden guard forms (variable equality, relational ops) + new positive/negative tests.

Risk Notes
- Current nested `(if ...)` structure will change once rif macro canonicalization lands; test tolerant to either form to avoid churn.
- Parser still emits unused variable warnings in earlier phases (not tuple-related); scheduled cleanup after type checker integration.

### 2025-09-01 – Tuple Meta Type Checker Integration Completed + If-Chain Unification
Status: Complete (Phase slice) / Next: Struct patterns

Delivered:
- Resolved prior structural compilation issue in `type_check.inl`; clean rebuild passes.
- Added semantic handlers:
	* `tuple-pattern-meta` – shape + optional arity validation against `__TupleN` synthetic struct pointer (benign in positive flows).
	* `tuple-match-arms-count` – metadata arity check only.
	* `(assign %dst %src)` – explicit arity + type match (E1106 arity / E1107 mismatch).
- Implemented if-chain assignment destination unification (E1108) to enforce single `%dst` across nested conditional selection chains produced by tuple match lowering (prevents silent divergence of result variable).
- Removed unused local lambda (`is_int`) eliminating a warning.
- Extended `rustlite_tuple_match_literal_guard_test` to run full type checking; asserts absence of unexpected tuple pattern diagnostics (E1454/E1456–E1459) in positive case – test passes.

Diagnostics / Codes:
- New: E1106 (assign arity / operand form), E1107 (assign type mismatch reuse), E1108 (if-chain destination inconsistency).
- Tuple provisional codes E1456–E1459 currently reserved; not yet repurposed.

Verification:
- All builds succeed; only pre-existing link duplicate-library warnings remain (benign linking noise to address later).
- Guard test output: `[rustlite-tuple-match-literal-guard] ok` after type check integration.

Next Focus:
1. Struct pattern parsing + lowering (Phase 2b) with planned diagnostics (unknown field, duplicate field, bad `..`, non-struct target).
2. Nested tuple pattern support and placeholder `_` suppression (Phase 2c/d).
3. Regression test: negative tuple arity mismatch still emits single E1454 (already covered by existing neg drivers) post type checker changes.
4. Decide final numbering or reuse of E1456–E1459 before struct pattern landing.
5. Optional: integrate lazy tuple arm extraction (skip clusters for arms proven unreachable by earlier literal guards) – performance micro-optimization.

Notes:
- If-chain unification deliberately shallow: it only scans branch vectors for `(assign ...)` ops; future enhancements may track nested blocks.
- No observable impact on existing ematch / rif-let tests (assign mismatch negative still surfaces E1107).

-- END UPDATE 2025-09-01

## 2025-09-02 – Parser Direction & Pattern Roadmap Addendum (Appended)

Added (from gap analysis and current session observations):
- Parser Direction Checkpoint: Decide PEGTL restoration vs continued handwritten evolution by 2025-09-10; fallback = keep handwritten but modularize (split helpers, add span capture shim). Record decision & rationale.
- Modularization Goal: Split future PEGTL variant into `grammar.(hpp|cpp)`, `actions.(hpp|cpp)`, `pattern_lowering.(hpp|cpp)` to avoid monolithic churn seen earlier.
- Pattern Nesting Strategy: Initial implementation will FLATTEN nested tuple/struct patterns (emit only leaf `tget`/`member` ops) + include a metadata note enabling later hierarchical reconstruction. Re-evaluate after diagnostics stable.
- Exhaustiveness & Duplicate Arm Integration: Ensure emitted pattern metadata (`tuple-pattern-meta`, pending `struct-pattern-meta`) is consumed by future features 35/36 for arm coverage & structural duplicate detection.
- Error Code Reservation Finalization: Confirm E1456–E145A table (unknown field, duplicate field, bad `..`, non-struct target, duplicate binding) before struct pattern implementation to avoid renumber churn.
- Dual Parser Parity Risk: Introduce golden IR parity tests comparing handwritten vs PEGTL lowering for a shared corpus (activated once PEGTL restored) to prevent drift.
- Span / Source Locations: Plan lightweight span attachment to pattern meta (fields start_line, start_col, length) prerequisite for richer diagnostics—target after basic struct pattern diagnostics.
- Match Arm Extraction Performance: Track metric once multi-arm patterns land; may switch from eager to lazy cluster emission if >25% redundant extraction across representative workloads.
- Borrow Checker Interaction Note: When references/borrows (feature 12) introduce aliasing, destructuring of references (`let (&x, &mut y) = ..`) will require move vs borrow semantics spelled out—reserve design slot.
- Rest Pattern (`..`) Semantics: Phase 1 treat as syntactic placeholder (ignored). Future: enforce coverage constraints (struct) and permit suffix capture (tuple) – create follow-up issue if semantics diverge from Rust.
- Duplicate Binding Detection: Implement single-pass unordered_set check during pattern parse; emit E145A before lowering; avoid generating colliding SSA names.
- Over-Arity Suppression Rules: Document heuristic: when a recognized tuple pattern exceeds arity, emit only aggregate E1454 and suppress per-index OOB diagnostics.
- Gapped Index & Placeholder Handling: Negative tests to ensure non-dense user-authored `tget` sequences or placeholders do not spuriously trigger pattern diagnostics.
- Golden IR Stability Guidelines: Preserve gensym base names (`add`, `arr`, `enum`, etc.) to minimize churn; any systematic rename must update goldens atomically.
- Diagnostic Consumer Pass (Planned): New pass converts struct/tuple pattern metadata to concrete diagnostics (E1454–E1459, E145A) rather than embedding all logic in parser or type checker—improves separation of concerns.

-- END ADDENDUM 2025-09-02

