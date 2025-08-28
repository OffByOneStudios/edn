# EDN-0013: Rustlite Surface vs Macro Test Plan

Status: In Progress
Created: 2025-08-28
Owner: (unassigned)

## Summary
Rustlite macro development to date validated expansion using hand-authored EDN macro forms. The surface parser paths (`.rl.rs` / future Rustlite syntax) have minimal direct test coverage. Introduce a dual-layer test strategy:

1. Surface Parsing Tests: Ensure Rustlite surface samples lower to the expected EDN macro forms.
2. Macro Expansion Tests: Preserve and extend existing EDN macro-form tests validating lowering into core EDN / IR.

This separation increases confidence that syntax changes or parser regressions do not silently break macro expansion assumptions.

## Goals
- Add a structured test suite for surface syntax -> EDN macro output.
- Maintain existing macro expansion tests; avoid duplication by keeping each suite narrowly focused.
- Provide golden file mechanism with update toggle.
- Cover positive and negative (diagnostic) cases.
- Integrate into CTest for CI.

## Non-Goals
- Full Rust parsing fidelity (ownership, lifetimes, advanced patterns).
- Replacing existing macro-form tests.
- Implementing free-variable capture inference beyond current heuristic.

## Test Layer Definitions
### 1. Surface Parsing Layer
- Input: `.rustlite` (or `.rl`) files containing surface constructs (enums, rtry, rwhile-let, rfor-range, ematch, tuples, arrays, closure, indexing, operators, bounds flag behavior examples, capture inference flag cases).
- Process: parse -> (optionally minimal normalization) -> serialize EDN macro-level representation (pre `expand_rustlite`).
- Oracle: golden EDN file (`<name>.gold.edn`) stored alongside source.
- Negative tests: expected diagnostics in `<name>.diag` file (one per line: `CODE: substring`).

### 2. Macro Expansion Layer (Existing)
- Input: EDN macro forms (current drivers).
- Process: `expand_rustlite` -> type check / IR emission.
- Oracle: pass/fail + targeted pattern assertions (already implemented via existing drivers).

## Directory Structure
```
languages/rustlite/surface_tests/
  samples/
    tuple_basic.rustlite
    tuple_basic.gold.edn
    tuple_basic.diag (optional)
  CMakeLists.txt (adds custom test commands)
```

## Implementation Tasks
- [x] [P1] Create directory scaffold `languages/rustlite/surface_tests` with CMake integration.
- [x] [P1] Decide and document file extension (`.rl.rs`) and normalization rules (semantic equality fallback via structural EDN compare).
- [x] [P1] Implement normalization utility (`rustlite_surface_norm`) invoking parser and emitting EDN macro form.
- [x] [P1] Add CTest registration via CMake glob with per-sample compare.
- [x] [P2] Add `UPDATE_RUSTLITE_GOLDENS=1` env to regenerate goldens.
- [x] [P2] Seed initial & extended tests (see Coverage below).
- [ ] [P2] Add advanced samples: arrays, enum match (ematch), rtry Result/Option forms, rwhile-let, rfor-range tuple form, closure capture inference (flag on/off), bounds index load with flag on, bitwise ops.
- [ ] [P2] Add negative surface tests covering: ematch non-exhaustive (E1600), rtry wrong sum (E1603), tuple index OOB (E1601), bounds OOB, closure capture inference disabled scenario.
- [ ] [P3] Documentation: update `docs/RUSTLITE.md` with section on test layering & adding tests.
- [ ] [P3] CI integration verification note (ensure included in pipeline doc) .

## Risks & Mitigations
| Risk | Mitigation |
|------|------------|
| Parser changes reorder keyword arguments causing spurious diffs | Apply normalization step before comparison. |
| Golden churn slows iteration | Provide `UPDATE_RUSTLITE_GOLDENS=1` env to refresh intentionally. |
| Duplication of effort between layers | Keep each surface test minimal; macro tests focus on semantics / IR shape. |

## Acceptance Criteria
- At least 10 representative surface tests pass (positive and negative).
- Running with an intentionally altered surface file causes a failing diff.
- Setting update env var regenerates golden and test passes again.
- Documentation updated with contributor instructions.

## Current Coverage Snapshot (2025-08-28)
Implemented passing surface tests (positive unless prefixed `neg_`):
```
assign_stmt
break_continue
call_args
compound_assign
empty_call
enum_basic
eq_ne_ops
fn_call
fn_params
for_in_inclusive
for_in_var_bounds
if_else
implicit_tail_return
let_mut
let_stmt
let_typed
logical_ops
loop_stmt
mixed_precedence
neg_incomplete_if (negative)
neg_invalid_token (negative)
neg_unmatched_brace (negative)
nested_blocks
paren_exprs
range_for
range_inclusive
range_literal
rel_ops
tuple_basic
unary_minus
unary_not
while_loop
```
Count: 33 samples (including 3 negative).

Recently added in this iteration:
- eq_ne_ops
- for_in_inclusive
- for_in_var_bounds
- mixed_precedence
- implicit_tail_return (tail expression return support added)

Remaining planned surface feature areas (not yet covered by tests):
- Arrays / indexing read & write (+ bounds flag on case)
- Tuple index out-of-bounds negative
- Enum match / pattern (ematch) positive & non-exhaustive negative
- rtry with Result / Option forms (success & error paths; error codes E1600/E1603 variants)
- rwhile-let loop construct
- rfor-range tuple form (using rrange + rfor-range) inclusive & exclusive
- Closure syntax & capture inference flag on/off; negative when capturing disabled
- Bitwise operators (& | ^ << >>) and compound assignments for them if supported
- Inclusive for-in semantics (currently parser ignores inclusive, need semantic test once implemented)
- Identifier-based inclusive ranges (a..=b) once supported
- Empty range / degenerate (5..5, 5..=5) expected behavior
- Additional negative syntax cases: malformed range (0..), (..5), bad for header, unterminated block comment, missing semicolon diagnostics, keyword as identifier

## Next Steps
1. Implement/extend parser & lowering for: bitwise ops, arrays/indexing, rfor-range tuple form.
2. Add tests for rrange + rfor-range tuple usage (positive & inclusive variant once semantics defined).
3. Introduce ematch & rtry surface samples (positive + negative diagnostics).
4. Closure & capture inference samples gated by feature flag.
5. Add remaining negative diagnostics (malformed range, bad for, unterminated comment, keyword ident, missing semicolon).
6. Update documentation once major gaps filled.

Progress metric: 33 current / ~55 projected initial comprehensive set (~60% complete).

## Follow-Ups (Future)
- Consider JSON diff output for easier tooling.
- Add free variable multi-capture inference tests if feature expands.

