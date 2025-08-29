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
