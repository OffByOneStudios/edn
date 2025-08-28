# EDN-0012: Rustlite Macro Restoration Retrospective & Follow-Up

Status: Open
Created: 2025-08-27
Owner: (unassigned)
Related: EDN-0009 (initial restoration plan), EDN-0011 (incremental roadmap)

## Summary
This issue captures the completed restoration of the Rustlite macro expansion layer after its earlier truncation, documents the concrete fixes applied to re-green the test suite, and tracks remaining polish / cleanup tasks now that functionality is back. All previously failing Rustlite-driven tests (field/index sugar, struct/trait shapes, enum & match result mode, closures, complex lowering JIT) are passing again. Two phase5 complex lowering gtests failed due to a path resolution issue and were fixed by introducing robust sample path discovery.

## Timeline Highlights
| Date (UTC) | Event |
|-----------|-------|
| 2025-08-26 | Field & index macros rewritten to emit only core ops (member / member-addr+store / index / load / store). |
| 2025-08-26 | Tuple & array macros (tuple, tget, arr, rarray) restored; tuple arity tracking reinstated. |
| 2025-08-26 | Discovered driver ordering inconsistency: some drivers called expand_traits before expand_rustlite. Identified as source of unresolved trait-call placeholders. |
| 2025-08-27 | Struct & trait macros fixed to preserve keyword/value pairs; rdot remapped to trait-call form. |
| 2025-08-27 | rcall intrinsic rewrite macro corrected (consistent optional return; eliminated mismatched lambda return). |
| 2025-08-27 | Sum/enum macros repaired (make_sym replaced with rl_make_sym helpers; duplicate rnone removed). |
| 2025-08-27 | Enum match expansions (ematch, legacy rmatch) validated; result-mode diagnostics (E1421/E1422/E1423) passing. |
| 2025-08-27 | Complex lowering gtests failing due to sample path; added multi-path fallback logic; tests now PASS. |

## Fixed Root Causes
- Placeholder op names (get/set/index-*) not recognized by type checker -> Rewritten to core ops.
- Struct macro dropped keywords (:name, :fields) -> Preserved full pairs; eliminated 'unknown struct in member-addr'.
- Trait dispatch chain broken (rdot emitted unsupported 'dot' op) -> rdot now emits trait-call, consumed by expand_traits.
- Expansion order mismatch (traits before rustlite) -> Standardized: expand_rustlite first, then expand_traits.
- rcall macro lambda returned node_ptr in helper path vs std::optional<node_ptr> -> Unified signature.
- Sum/enum macros referenced removed helpers (make_sym) & duplicate macro definitions -> Updated to rl_make_* helpers; deduped.
- GTest complex lowering sample path brittle relative to working directory -> Introduced candidate path iteration.

## Current Test Status
All phase3/phase4/phase4_full binaries pass. Phase5 complex lowering gtests (2) now pass. No remaining unknown-instruction or unknown-struct diagnostics in Rustlite paths.

## Remaining Cleanup / Polish Tasks
(Tracked here unless spun out into separate issues.)
- [ ] Remove temporary compatibility wrappers & unused helper warnings (make_kw, gensym) across macro source files.
- [ ] Normalize macro source style (consistent rl_make_* helper usage everywhere).
- [ ] Add micro-tests for rcall intrinsic rewrite equivalence (binary ops & logical not lowering) – could be a small driver or gtest.
- [ ] Add trait object round-trip test verifying vtable structure after expansion (existing driver covers basics, may expand to inspect synthesized struct fields).
- [ ] Consider moving legacy rmatch (non-exhaustive default semantics) docs into a separate 'Legacy / Transitional' section in ENUMS_MATCHING.md.
- [ ] Evaluate opportunity to merge block wrappers emitted by rset / rindex-store when they contain a single core op (micro-IR cleanliness; low priority).
- [ ] Introduce a lightweight macro expansion assertion utility (golden diff) for a few representative Rustlite surface snippets (guard future regressions).
- [ ] Add CI step to run rustlite_jit_driver on a curated subset of samples (including complex_lowering) to catch path or JIT drift early.

## Metrics / Evidence
- Unknown instruction count after initial restoration: 20 → 0.
- Rustlite complex lowering JIT result: restored from parse failure (-999 sentinel) to Result: 6.
- Trait macro path: trait-call + make-trait-obj forms observed and lowered successfully (no EGEN diagnostics).

## Risks / Follow-Ups
| Risk | Mitigation |
|------|------------|
| Reintroduction of placeholder ops via future macro edits | Add linter / static assertion in expansion verifying only whitelisted op symbols produced. |
| Divergence between rmatch & ematch semantics confuses users | Strengthen docs; consider deprecating rmatch in favor of ematch once ecosystem migrated. |
| Accidental regression of intrinsic rewrites (rcall) | Add explicit gtest verifying direct (add) vs rcall(add) produce identical IR sequences (hashed). |

## Definition of Done (for this Issue)
1. All cleanup tasks above either completed or split into their own numbered issues.
2. Added IR equivalence tests for rcall intrinsic ops.
3. Added JIT sample CI step.
4. Documentation updated to reflect stable restored macro layer and note any deprecated legacy forms.
5. Issue closed with commit references for each task.

## References
- EDN-0009 (original macro restoration & phantom source anomaly documentation)
- EDN-0011 (incremental feature roadmap; future enhancements depend on this stable base)
- `languages/rustlite/src/macros_*.cpp` (restored sources)
- `tests/phase5_complex_lowering_gtest.cpp` (path fallback logic)

---
(Generated initial draft via assistant; refine ownership and task priority as needed.)
