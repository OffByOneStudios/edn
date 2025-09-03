# EDN-0016: Rustlite PEGTL Parser Restoration & Gap Analysis

Status: Draft
Owner: (assign)
Created: 2025-09-02
Related: `EDN-0015-rustlite-destructuring-continuation.md`
Depends On: Prior PEGTL grammar history (pre-minimal handwritten parser swap)

## Background
The original PEGTL-based `languages/rustlite/parser/parser.cpp` provided a structured grammar with rule/action specializations. During attempts to integrate struct pattern duplicate detection and match arm destructuring, large accidental edits removed or misplaced numerous rule declarations (e.g. `plus_equal`, `expr_text`, `if_stmt`, `while_stmt`, `let_stmt`), corrupting the file. A temporary handwritten parser supplanted the grammar to regain a compiling baseline and advance destructuring features.

We now intend to restore a modular PEGTL parser while retaining the working destructuring semantics and metadata emission introduced in the handwritten implementation.

## Purpose of This Issue
1. Catalog every feature currently supported ONLY in the handwritten parser that is MISSING from (or not yet reimplemented in) the PEGTL grammar.
2. Define a safe incremental plan to reintroduce PEGTL parsing without repeating prior large-file corruption.
3. Provide acceptance criteria and spin-out subtasks.

## Current State Summary
| Area | Handwritten Parser | PEGTL (previous state, now reverted) | Gap |
|------|--------------------|--------------------------------------|-----|
| Let simple bindings | Yes | Yes | Parity |
| Let tuple patterns | Yes (emits `(tget ...)` + `(tuple-pattern-meta ...)`) | Partial (older tuple only maybe absent) | Verify & port meta emission |
| Let struct patterns | Yes (emits `(member ...)`, `(struct-pattern-meta ...)`, `(struct-pattern-duplicate-fields ...)`) | Missing | Port logic |
| Duplicate struct field detection (E1457 meta) | Yes (meta only) | Missing | Port |
| Match tuple patterns | Yes (multi-arm lowered with guards + `(tuple-match-arms-count N)`) | Missing / earlier simpler form | Port cluster + metadata |
| Match struct patterns | Not implemented yet | N/A | Future (out-of-scope) |
| `ematch` enum pattern sugar | Yes | Missing | Port simplified lowering branch |
| `rtry` | Yes | Missing | Port |
| `rwhile_let` | Yes | Missing | Port |
| Array literals `[a,b]` | Yes | Missing or partial | Port |
| Enum constructor `Type::Variant(args...)` | Yes | Missing | Port |
| Compound assignment ops (+=, -=, *=, /=, &=, |=, ^=, <<=, >>=) | Yes (direct op or `rcall`) | Partial (basic ones) | Add bitwise & shift support |
| Index assignment `a[i] = v;` | Yes | Missing | Port |
| For-in range sugar `for x in start..end {}` (if present) | Handwritten uses placeholder (need confirm) | Older had stub | Audit |
| Pattern metadata instructions | Yes | Missing | Port generation hooks |
| Duplicate binding detection (future) | Not yet | N/A | Plan only |
| Unknown struct fields detection meta | Not yet | N/A | Plan only |
| Non-tuple destructure detection meta | Not yet | N/A | Plan only |

## Missing From PEGTL (Port List)
Concrete items to reintroduce inside PEGTL-based lowering layer:
1. Struct pattern branch inside `action<let_stmt>` with field parsing + duplicate detection + meta emission.
2. Tuple pattern `(tget ...)` extraction + `(tuple-pattern-meta ...)` meta emission for `let (..)`.
3. Multi-arm tuple match lowering inside expression lowering (recreate in a well-scoped helper):
   - Arm parsing (pattern + body) capturing literals and wildcards.
   - Emission: `tget` extractions, `(tuple-pattern-meta %scrut <Arity>)` per arm, guard condition chain of `(rif ...)`, destination temp initialization, `(tuple-match-arms-count N)` meta.
4. Array literal lowering `[a,b,c]`.
5. Enum constructor detection `Type::Variant` with optional args.
6. `ematch` lowering (enum match sugar) producing `(ematch <dst> i32 <EnumType> %scrut :cases [ (arm Variant :body [...]) ...])`.
7. `rtry` surface sugar lowering.
8. `rwhile_let` surface sugar lowering.
9. Index assignment lowering `(ridx_set ...)`.
10. Extended compound assignment operators: `&= |= ^= <<= >>=` plus bitwise/shift mapping to intrinsic ops.
11. `(struct-pattern-duplicate-fields ...)` meta emission for duplicate detection.
12. `(struct-pattern-meta ...)` emission including ordered field list.
13. `(tuple-pattern-meta ...)` emission for tuple patterns (let + match arms).
14. `(member %bind Type %val field)` extraction emission for struct patterns.
15. `(tuple-match-arms-count N)` meta emission.

## Non-Feature Infrastructure Gaps
- Lack of modularization: Single monolithic `parser.cpp` increases risk. Need to split into:
  * `grammar.hpp` / `grammar.cpp` (rules only)
  * `actions.hpp` / `lowering.cpp` (action specializations + lowering helpers)
  * `pattern_lowering.hpp` (shared tuple/struct pattern utilities)
- No unit tests validating grammar fragments in isolation: add targeted parse tests with golden EDN outputs.
- No AST or span tracking: minimal for now; add span scaffolding later.

## Incremental Restoration Plan
1. Create new files (`languages/rustlite/parser/grammar.hpp`, `.../lowering.hpp`, `.../pattern.hpp`) without touching existing handwritten parser.
2. Implement a skeletal PEGTL grammar mirroring tokens and statement structure. Include only: identifiers, literals, let, assignment, compound assignment, return, basic expressions.
3. Port tuple pattern in `let` only; compile & test.
4. Port struct pattern in `let`; compile & test; ensure duplicate meta emission parity with handwritten parser goldens.
5. Add switch (build flag or macro) allowing selection between handwritten and PEGTL parser for transition.
6. Port expression sugars (array literal, enum ctor, rtry, rwhile_let) one by one with build checks after each.
7. Port tuple match lowering.
8. Add new tests covering each ported feature (one positive, one negative if applicable).
9. Remove obsolete handwritten paths after parity validated.

## Acceptance Criteria
Restoration is complete when:
- New PEGTL modular parser builds alongside existing code (flag-enabled) and passes all existing destructuring & pattern meta tests.
- Each item (1–15) either implemented or explicitly deferred with reference to follow-up issue.
- Handwritten parser no longer required for baseline features (retained temporarily behind flag until stabilization).
- Tests enforce pattern meta emission parity.

## Risks & Mitigations
| Risk | Impact | Mitigation |
|------|--------|------------|
| Re-introducing large monolithic file | Hard to review / regressions | Modular split & incremental PRs |
| Action specialization duplication | Divergent semantics | Centralize pattern helpers in one header |
| Incomplete pattern coverage causing test flakiness | Slows adoption | Maintain dual-parser flag until parity confirmed |
| Grammar drift vs. future macro system | Rework later | Keep lowering surface minimal; macros operate on emitted meta |

## Follow-Up Tickets
- EDN-0017: Diagnostic Consumer for Pattern Meta (E1454–E1457 conversions)
- EDN-0018: Match Arm Struct Patterns & Nested Patterns
- EDN-0019: Span Tracking & Rich Diagnostics
- EDN-0020: Parser Selection Flag Removal & Cleanup

## Immediate Next Action
Decide owner; snapshot current handwritten destructuring goldens as reference; start step (1) modular skeleton.

---
Prepared automatically. Refine details & adjust numbering once owner assigned.
