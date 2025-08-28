# EDN-0013: Rustlite Surface vs Macro Test Plan

Status: Open
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
- [ ] [P1] Create directory scaffold `languages/rustlite/surface_tests` with CMake integration.
- [ ] [P1] Decide and document file extension (`.rl` or `.rustlite`) and normalization rules.
- [ ] [P1] Implement small normalization utility (`rustlite_surface_norm` executable) that:
  - Reads surface file, invokes existing parser entry (TBD hook), outputs EDN macro form.
  - Optionally sorts keyword pairs for deterministic output.
- [ ] [P1] Add CMake function to register each sample pair as a CTest (compare produced EDN vs golden; diff on mismatch).
- [ ] [P2] Add `--update-goldens` env or CTest property to rewrite goldens.
- [ ] [P2] Seed initial tests: tuples, arrays, enum + ematch exhaustive, rtry Result + Option, rwhile-let, range literal + inclusive, rfor-range with range value, closure capture inference (flag on/off), bounds index load (flag on), compound assign + bitwise ops.
- [ ] [P2] Add negative surface tests: ematch non-exhaustive (E1600), rtry wrong sum (E1603), tuple index OOB (E1601), bounds OOB expansion path parsing mismatch if any, closure capture inference disabled (no :captures inserted).
- [ ] [P3] Documentation: update `docs/RUSTLITE.md` with section on test layering & how to add surface tests.
- [ ] [P3] CI integration note: ensure new tests run in existing workflow.

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

## Follow-Ups (Future)
- Consider JSON diff output for easier tooling.
- Add free variable multi-capture inference tests if feature expands.

