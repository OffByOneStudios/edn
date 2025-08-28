# EDN-0012: Rustlite Macro Restoration Retrospective & Follow-Up

Status: Closed
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

## Deferred Follow-Up Tasks
All polish items have been spun out (or will be) into separate issues for clearer tracking. Original checklist retained here for historical context:
- Remove temporary compatibility wrappers & unused helper warnings (make_kw, gensym) across macro source files.
- Normalize macro source style (consistent rl_make_* helper usage everywhere).
- Add micro-tests for rcall intrinsic rewrite equivalence (binary ops & logical not lowering).
- Add trait object round-trip test verifying vtable structure after expansion.
- Move legacy rmatch docs to a 'Legacy / Transitional' section in ENUMS_MATCHING.md.
- Consider merging single-op block wrappers from rset / rindex-store.
- Add macro expansion golden diff utility.
- Add CI step invoking rustlite_jit_driver on representative samples.

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

## Closure Rationale
Test suite is fully green (76/76 tests run, 3 disabled) after final parser adjustments (optional mut/typed params, tail expression implicit return) and macro restorations (rdot conditional lowering, rindex value semantics). Remaining cleanup is non-blocking and tracked separately. Closing on commit b47efdf.

## Definition of Done (Met)
- Stable passing test suite validating restored macro & parser behavior.
- No unknown-instruction diagnostics in Rustlite paths.
- Enum / match feature tests and CLI samples pass.
- Residual tasks documented for future issues.

## References
- EDN-0009 (original macro restoration & phantom source anomaly documentation)
- EDN-0011 (incremental feature roadmap; future enhancements depend on this stable base)
- `languages/rustlite/src/macros_*.cpp` (restored sources)
- `tests/phase5_complex_lowering_gtest.cpp` (path fallback logic)

---
(Generated initial draft via assistant; refine ownership and task priority as needed.)
