# EDN-0015: Rustlite Destructuring Continuation

Status: Draft
Owner: (assign)
Created: 2025-09-01
Related Epics: `epics/epic_surface_languages.md`, `epics/epic_diagnostics_tooling.md`, `epics/epic_tooling_platform.md`
Depends On: `EDN-0014`, prior (untracked) PEGTL grammar work

## Context / Summary
During refactoring to remove a duplicate `let_stmt` specialization in the PEGTL-based Rustlite parser, large helper sections were accidentally removed, leaving dangling `action<>` template references and corrupting `parser.cpp`. To quickly restore a compiling baseline and preserve recent tuple & struct destructuring progress, the entire PEGTL grammar was temporarily replaced with a **minimal handwritten line‑oriented parser** (`languages/rustlite/parser/parser.cpp`). This parser currently supports:

- let bindings (simple, tuple, struct patterns)
- tuple pattern element extraction via `(tget ...)`
- struct pattern field extraction via `(member ...)`
- placeholder `_` handling (skips binding)
- emission of pattern metadata:
  * `(tuple-pattern-meta <val> <arity>)`
  * `(struct-pattern-meta <val> <TypeName> <field-count> <field-names...>)`
  * `(struct-pattern-duplicate-fields <val> [ dup1 dup2 ... ])`
- simple expression lowering (integers, identifiers, + - * /, parens)
- assignment & compound assignment (+=, -=, *=, /=, &=, |=, ^=, <<=, >>=)
- return statements (inserting implicit zero return if absent)
- simple call form lowering `(rcall <dst> i32 <callee> <args...>)`

Meta emission lays groundwork for planned diagnostics:
- E1454: Tuple arity mismatch (needs consumer pass)
- E1455: Tuple destructuring of non-tuple (needs type info integration)
- E1456: Unknown struct field (requires comparing pattern fields vs struct decl; partially implemented previously outside current parser scope)
- E1457: Duplicate struct fields (meta already emitted; diagnostic consumer pending)

## Current File State (`parser.cpp`)
The file now contains only the minimal handwritten implementation. All legacy PEGTL grammar fragments and patch artifact markers have been removed. Build succeeds, though warnings remain (string literal comparisons, missing field initializers, covered switch default, unused variables).

## Why This Ticket
We must either (a) complete destructuring + diagnostics atop the minimal parser or (b) reintroduce a robust grammar (PEGTL or alternative) without re‑introducing prior corruption risks. This ticket captures precise continuation steps, risks, and acceptance criteria so we can safely resume after a pause.

## Scope
IN: Tuple & struct destructuring completion (let + future match arms), associated diagnostics (E1454–E1457), restoration/improvement of parser infrastructure, warning cleanup.
OUT: Reintroduction of full macro expansion layer (tracked elsewhere), advanced pattern forms beyond those listed until baseline diagnostics land.

## Implementation Status
| Area | Status | Notes |
|------|--------|-------|
| Simple `let name = expr;` | Working | Emits `(as %name type value)` (type currently always `i32` unless explicit future extension) |
| Tuple `let (a, _, b) = expr;` | Working (Phase 1) | Emits `(tget ...)` per element + `(tuple-pattern-meta ...)` |
| Struct `let Point { x, y: ry } = expr;` | Working (Phase 1) | Emits `(member %x Point %val x)` lines + meta; aliasing supported |
| Duplicate struct field detection | Meta emitted | `(struct-pattern-duplicate-fields ...)` present; no diagnostic consumer yet |
| Unknown struct field detection | NOT active | Requires struct schema lookup (future) |
| Placeholder `_` | Working | Generates temp binding or skip (tuple); skip binding for struct fields with `_` or `_:` alias form |
| Nested patterns | Not implemented | Needs recursive descent or restored grammar |
| Rest pattern `..` | Parsed but ignored | No effect currently; must record & enforce coverage later |
| Match arm patterns | Not implemented | Deferred to Phase 2 |
| Diagnostics E1454–E1457 | Partially scaffolded | Only duplicate-fields meta produced; no arity/unknown error emission yet |
| Warning cleanup | Pending | See Technical Debt section |

## Required Next Steps
### Phase 1 Completion (Let Patterns + Diagnostics)
1. Add consumer pass (post-parse, pre-type-check) to transform pattern metadata into diagnostics:
   - Tuple arity mismatch (E1454): Compare emitted arity vs actual tuple type length (needs tuple type info; interim: count of emitted `tget` vs known forward-declared arity if available, else defer with TODO).
   - Duplicate struct fields (E1457): Convert `(struct-pattern-duplicate-fields ...)` meta to diagnostic entries.
   - Unknown struct fields (E1456): Implement struct field set lookup (requires access to struct declarations; add symbol table query). Emit one diagnostic per unknown field.
   - Non-tuple destructuring (E1455): If expression type isn't tuple but tuple-pattern-meta present; requires type inference hook.
2. Integrate new diagnostics into error index & regenerate (`scripts/gen_error_index.py`).
3. Add positive & negative tests: tuple arity mismatch; unknown field; duplicate field; non-tuple destructuring attempt.

### Phase 2 (Pattern Feature Expansion)
4. Implement nested patterns (tuple inside tuple / struct inside tuple, etc.). Decide representation (recursive pattern tree vs flat extracted sequence + hierarchical meta). Update lowering accordingly.
5. Add rest pattern (`..`) semantics for structs & tuples: enforce that fields/elements after rest are disallowed (struct) or capture remainder (tuple) – define minimal semantics (maybe ignore binding but allow pattern to match variable length tuples later).
6. Introduce match arm destructuring using same extraction logic (emit per-arm `tget` / `member` + metadata). Ensure lazy extraction (only for executed arm) or accept eager extraction with later optimization.
7. Alias tracking & duplicate binding detection within a single pattern; emit diagnostic (new code TBD) if name appears more than once.

### Phase 3 (Refinement & Grammar Decision)
8. Decide to either:
   - Continue evolving handwritten parser (add tokenization & small recursive descent for patterns), OR
   - Restore PEGTL grammar from history (carefully reconstruct helpers) isolating pattern rules into dedicated modules.
9. If restoring PEGTL: create unit tests for grammar minimal slices (pattern-only parse fixtures) to guard against future corruption.
10. Abstract common pattern lowering into reusable utilities proxied by both let/match contexts.

## Design Considerations
- Metadata-first approach decouples pattern recognition from full semantic validation; allows incremental addition of diagnostics without rewriting parse layer.
- For nested patterns, extend metadata shapes:
  * `(tuple-pattern-meta %val <arity> :children [ (%idx %child_sym %child_kind) ... ])` (optional)
  * For struct: add field-level nested markers.
- Decide whether to canonicalize placeholder `_` as generated symbol or omit entirely from metadata (current approach omits; continue for simplicity).

## Test Plan (Initial)
Add tests under `tests/` (new or existing Rustlite suite):
- `rustlite_tuple_let_destructure_positive_test`
- `rustlite_tuple_let_arity_mismatch_neg_test` (E1454)
- `rustlite_struct_let_duplicate_field_neg_test` (E1457) – meta -> diag
- `rustlite_struct_let_unknown_field_neg_test` (E1456)
- `rustlite_tuple_non_tuple_destructure_neg_test` (E1455)
- Future: nested pattern positive / rest pattern placeholder tests.

## Technical Debt / Cleanup
- Warnings in `parser.cpp`:
  * Missing field initializer for `Node` (add explicit initializers or a constructor).
  * String literal comparison loop for compound assignments (replace iteration with std::array + explicit strcmp already partly addressed; finalize cleanup).
  * Switch with default over fully covered enum (remove `default` or handle all cases explicitly; we already do handle all; remove default to silence warning).
  * Potential unused variable future (audit once nested patterns added).
- Lack of source location tracking: pattern diagnostics will benefit from span info; consider augmenting metadata instructions with line/column attributes.

## Risks
- Continuing with handwritten parser may lead to incremental complexity compromising maintainability (recreating a grammar slowly). Mitigate with early decision in Phase 3.
- Without location spans, diagnostics may feel vague; risk of rework when spans added later.
- Meta instruction shape changes could break early consumers; define stable minimal contract now.

## Acceptance Criteria
This ticket is DONE when:
1. All Phase 1 steps implemented; diagnostics E1454–E1457 emitted where appropriate during let destructuring.
2. Corresponding tests added and passing.
3. Warnings in `parser.cpp` related to destructuring changes reduced to zero (or documented intentional exceptions).
4. Decision recorded for Phase 3 parser direction with a mini design note (grammar vs handwritten) appended to this issue.
5. Error index regenerated including any new or updated codes.

## Follow-On Tickets To Spin Out Later
- EDN-0016: Match Arm Destructuring & Nested Patterns
- EDN-0017: Rest Pattern Semantics & Duplicate Binding Diagnostics
- EDN-0018: Pattern Span Attribution & Rich Diagnostic Messages
- EDN-0019: Parser Direction Decision (PEGTL Restoration vs Handwritten Evolution)

## Immediate Next Action (When Work Resumes)
Create diagnostic consumer pass skeleton scanning emitted body instructions for pattern meta, collecting tuple/struct patterns, and applying provisional validations (duplicate fields immediate; tuple arity placeholder until tuple type info accessible).

---
Prepared: (auto-generated). Update owner, refine design details, and adjust scope boundaries when resuming work.

## 2025-09-02 – Additional Destructuring Continuation Notes (Appended)

New Items to Track (not previously explicit in this ticket):
1. Parity Matrix vs Future PEGTL Parser: Maintain a checklist (in EDN-0016) referencing each meta instruction & diagnostic path; mirror a summarized state here with links when PEGTL variant lands.
2. Struct Pattern Alias + Duplicate Combination Tests: Add cases where the same underlying field appears twice via an alias to ensure duplicate detection triggers (E1457 meta → diagnostic) regardless of aliasing.
3. Placeholder `_` Inside Struct Pattern: Confirm it is ignored (no `(member ...)` emission) and excluded from duplicate & unknown field scans; add positive + negative tests.
4. Rest Pattern Misuse Cases: Add tests for misplaced `..` (leading, multiple, trailing with extra fields) once `..` is parsed; link prospective E1458.
5. Non-Tuple Destructure Heuristic: Interim approach (before full type inference) uses absence of `__TupleN` schema to trigger E1455—document fallback & TODO to refine once tuple typing integrated.
6. Duplicate Binding Names (Tuple/Struct): Implement early parser check issuing E145A (reserved) and halting lowering for that pattern; test both `let (a,a)=t` and `let Point { x: a, y: a } = p`.
7. Rest Pattern Semantics Decision Stub: Phase 1 ignore; Phase 2 decide between (a) allow anywhere but ignore or (b) enforce Rust-like positioning; open follow-up if choice (b).
8. Pattern Metadata Span Attachment: Add optional span fields to `(tuple-pattern-meta ...)` / `(struct-pattern-meta ...)` for future precise diagnostics – record dependency on parser token span capture.
9. Over-Arity Suppression Confirmation: Add regression test ensuring only aggregate E1454 is produced (no per-index OOB) for `(a,b,c,d)` vs `__Tuple3` scenario.
10. Gapped Index Negative: Ensure manually authored `tget` sequence not introduced via recognized pattern does not spuriously produce E1454/E1455—test under expansion pass.
11. Golden IR Stability Guard: Add CI check diffing normalized IR (strip gensym numeric suffixes) to reduce spurious test churn; note as optional improvement.
12. Struct Unknown Field Diagnostic Path: Implement converter from `(struct-pattern-meta ...)` plus field list & struct schema comparison to E1456 (unknown) after schema lookup integration.
13. Non-Struct Target Detection: Reserve E1459; heuristic: absence of struct schema at expansion or type-check stage when encountering `(member ...)` cluster + `(struct-pattern-meta ...)`.
14. Future Nested Pattern Strategy: Flatten first; record mapping so nested future patterns can be re-hydrated; add TODO to include child index mapping in meta (e.g., `:children [...]`).
15. Lazy Match Arm Extraction (Future Optimization): Mark as performance enhancement; implement only after functional parity tests pass.
16. Duplicate Field vs Duplicate Binding Differentiation: Ensure E1457 (duplicate field names) distinct from E145A (duplicate binding identifiers) even when alias targets collide.
17. Alias Field Reversal Test: `Point { x: a, a: x }` (if allowed) – decide semantics (likely treat `a:` left side invalid unless `a` is a field) and ensure proper diagnostic (unknown or duplicate) – specify behavior before implementation.
18. Diagnostic Consumer Pass Outline: Build an ordered pipeline: (a) Parse & Lower → (b) PatternMetaCollect (new pass) → (c) TypeCheck. Consumer pass converts metas to diagnostics before type-based validation requiring structural info.
19. Span Collision Test: Once spans added, ensure duplicate binding diagnostic references both occurrences precisely.
20. Documentation Sync: After first struct pattern diagnostics land, update `docs/RUSTLITE.md` pattern section with examples for all emitted metas & diagnostics.

Acceptance Extension for This Ticket:
- Above items 1–10 must be either implemented or explicitly deferred with a linked follow-up issue before closing EDN-0015.

-- END ADDENDUM 2025-09-02
