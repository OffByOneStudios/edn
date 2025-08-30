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
| 1 | Destructuring (2) | Not started | Will introduce tuple pattern lowering + arity mismatch E-code. |
| 2 | while/if let (20/21) | Complete | Macros (`rif-let`, `rwhile-let`) implemented; negative tests for missing / type mismatch / variant mismatch (E1421/E1422/E1423/E1405) and undefined :value symbol (E1421) landed. |
| 3 | loop-with-value (19) | Complete | `rloop-val` macro validated. Test matrix: break with value, plain break (retain zero-init), no break body, multi break (first wins), negative type mismatch (E1107). |
| 4 | Type alias (7) | Complete | Positive + negatives (E1330–E1333) plus cycle (self-ref) and sum shadow tests landed (E1333/E1401). |
| 5 | Generics + bounds (8/9) | Partially complete | Core single & multi-parameter parsing, monomorphization, and bounds diagnostics (E1700–E1702) implemented; specialization IR test green. Remaining: broaden multi-param instantiation matrix, bound failure negative tests (missing trait method), redundant bound handling policy. |
| 6 | References + deref (12/13) | Pending | Needs IR op or macro design draft. |
| 7 | Result propagation refinement (23) | Pending | Extend `rtry` diagnostics (Option/Result mixing already E1603). |
| 8 | Traits + impl (26/27) | Pending | Baseline dispatch exists; need completeness + duplicate impl diagnostics. |
| 9 | Modules + pub (31/32) | Pending | Namespace + export retention design TBD. |
| 10 | Exhaustiveness + duplicate arm (35/36) | Pending | Exhaustiveness algorithm + structural pattern hash for duplicate detection. |

Progress summary (current cycle): Completed if/while-let negatives (E1405, E1421–E1423); type alias suite incl. cycle + shadow; began generics/bounds planning.

### Recent Progress Log (2025-08-29)
Added after parser/lowering stabilization work in active debugging session.

Completed / Fixed:
- Literal lowering regressions resolved: return statements no longer capture trailing semicolons; numeric literals (including negatives) now preserve correct values.
- Negative integer literals normalized to (sub 0 <pos>) pattern to align with golden IR expectations (stability for future diff-based tests).
- Assignment & compound assignment expression trimming prevents spurious zero constants.
- Generics specialization IR test passes post parser adjustments (no regressions introduced).

Outstanding (Short-Term Next):
- Run broader surface test suite to flush any remaining literal/operator edge cases beyond validated unary minus & assignment scenarios.
- Refactor outdated type alias negative driver sources referencing removed Builder::raw / fn_raw APIs (blocking a subset of negative tests from compiling).
- Address sign-conversion warning in `expand.cpp` (iterator vs index erase loop) to keep warning budget clean.
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
- [ ] Tuple pattern arity mismatch (once destructuring implemented) – new E-code.
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
