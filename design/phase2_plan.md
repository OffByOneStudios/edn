# Phase 2 Design Plan

Goal: Evolve the experimental EDN -> LLVM IR pipeline from a minimally functional subset (Phase 1) to a richer, more semantically complete and ergonomic system, while preserving simplicity and incremental testability.

## High-Level Themes
1. **Unsigned Integer Semantics** (core requirement).
2. Floating‑point arithmetic & comparisons.
3. Conversions & casts (int<->int, int<->float, bitcast, pointer casts).
4. Proper SSA merging via `phi` (optional fallback to current memory-based merge).
5. Richer comparison & division instruction set (signed vs unsigned clarity).
6. Struct / array literals and aggregate initialization conveniences.
7. Global constants & data (read‑only, zero / explicit initializers, arrays of consts).
8. Diagnostics & developer experience (named errors, richer context, suggestion hints).
9. Execution tooling & CLI improvements (argument passing, IR dumps, verification flags).
10. Optional optimization pipeline (configurable pass pipeline per module / function).
11. Test suite expansion (golden IR, negative coverage for new semantics, fuzz/property tests).

## Incremental Milestones
| Milestone | Focus | Key Artifacts |
|-----------|-------|---------------|
| M1 | Unsigned integers + comparison refactor | Updated type system, parser, tests |
| M2 | Arithmetic expansion & conversions | New opcodes, type checker & emitter paths |
| M3 | SSA `phi` support | CFG lowering changes, tests for if/while merges |
| M4 | Aggregates & literals | `(struct-lit ...)`, `(array-lit ...)`, initialization lowering |
| M5 | Globals v2 (const + data arrays) | Emission of constant data, relocation tests |
| M6 | Diagnostics overhaul | Error formatting module, improved messages |
| M7 | Tooling & JIT CLI enhancements | Driver arg passing & IR dump modes |
| M8 | Optimization pipeline (optional) | Pass manager integration |

## 1. Unsigned Integer Support
### Design
Add new base type names: `u8 u16 u32 u64`. (Keep `i1` as single-bit predicate; no separate `u1`.) Underlying LLVM type identical width; signedness influences:
- Division / remainder: introduce `udiv`, `urem` (retain existing `sdiv`, add `srem`).
- Comparisons: signed vs unsigned predicates.
- Shifts: existing `lshr` (logical) already unsigned; `ashr` signed arithmetic.
- Extensions: introduce `(zext %dst <to-type> %src)` vs `(sext ...)`.

### IR Syntax Changes
Current comparison ops (`lt gt le ge eq ne`) implicitly signed. Phase 2 introduces canonical predicate form:
```
(icmp %dst <type> :pred <pred-name> %a %b)
; pred-name in { eq ne slt sgt sle sge ult ugt ule uge }
```
Backward compatibility: keep old forms for one phase; internally desugar to `icmp`.

### Type System Update
Extend `BaseType` enum to include unsigned variants. `parse_type` maps symbol names accordingly.
Stringification keeps canonical symbol (e.g. `u32`). No change to pointer or function types.

### Type Checker Changes
- Validate operand types for new ops.
- For `icmp`, ensure predicate compatibility (reject e.g. `slt` on floating type later when floats added).
- For zero/sign extensions ensure allowed direction: width(to) > width(from) (no widen mismatch) else require trunc.

### Emission Changes
- Map new base types to same LLVM integer types.
- `icmp` lowering chooses `llvm::CmpInst::Predicate` from predicate keyword.
- Division: `udiv` -> `CreateUDiv`; `sdiv` unchanged; remainders analogous (`SRem`, `URem`).

## 2. Floating-Point Arithmetic & Comparisons
Add types `f32 f64` already exist—extend operations:
- Arithmetic: `fadd fsub fmul fdiv`.
- Comparisons unify under `(fcmp %dst <type> :pred <pred>)` where `<pred>` ∈ { oeq one olt ogt ole oge ord uno ueq une ult ugt ule uge } (subset initially: `oeq one olt ogt ole oge` for simplicity).
- Conversions: `(sitofp) (uitofp) (fptosi) (fptoui)`.

## 3. Integer Bitwidth & Casting Instructions
Add instructions:
- `(zext %dst <to-type> %src)`
- `(sext %dst <to-type> %src)`
- `(trunc %dst <to-type> %src)`
- `(bitcast %dst <to-type> %src)` (same bit width reinterpret)
- `(ptrcast %dst (ptr <to>) %src)` (optional alias to bitcast when widths match).

## 4. PHI Nodes
Introduce instruction:
```
(phi %dst <type> [ (%incomingVar %labelName) ... ])
```
Parser & emitter: Build `llvm::PHINode` in the corresponding block; requires two-pass CFG emission for structured constructs. Option: phased: first implement minimal manual phi in `if` with both branches assigning to an alloca then rewrite to `phi` if simple pattern recognized (optimization pass: MemoryToPhi).

## 5. Aggregate Literals & Initialization
Syntax proposals:
```
(struct-lit Pair :fields [ a %va b %vb ]) => lowered to sequence of stores into an allocated struct temp; returns pointer or value? Phase 2: returns pointer.
(array-lit <elem-type> :elems [ %e1 %e2 ... ])
```
Alternative: `(const-struct %dst Pair [ <int> <int> ])` for constant global contexts.

## 6. Globals V2
Allow constant arrays & structs with initializer data:
```
(global :name TABLE :type (array :elem i32 :size 4) :init [1 2 3 4] :const true)
```
Emission: produce `ConstantArray` or nested `ConstantStruct`.
Validation: ensure element count matches declared size.
Add read-only flag -> set `GlobalVariable::isConstant(true)`.

## 7. Diagnostics Overhaul
Add `ErrorReporter` collecting rich entries:
- Fields: message, primary span (line/col), secondary spans (vector), hint.
- Helper macros or inline functions for uniform formatting.
Type mismatch messages: `expected <typeA> but got <typeB> for operand 'lhs' in 'add'`.
Display nearest available variable names on undefined symbol suggestions (Levenshtein distance <= 2).

## 8. CLI / JIT Tool Enhancements
Extend `phase1_driver` into `edn-run`:
- Flags: `--dump-ir`, `--dump-llvm`, `--func=<name>`, `--arg <int>` (repeatable)
- Optionally run optimization level: `--O=0|1|2` selecting pass pipeline.
- Return value printed; non-i32 returns supported (bool prints 0/1, void silent).

## 9. Optimization Pipeline (Optional)
Integrate LLVM PassBuilder:
- Construct minimal pipeline at O0 (none) / O1 (instruction combining, mem2reg, dead inst elim) / O2 (add GVN, LICM, inlining threshold small).
- Run after module emission, before JIT load.
- Guard with build option `EDN_ENABLE_PASSES`.

## 10. Testing Strategy
Category | Examples
---------|---------
Unsigned | `udiv/urem` vs `sdiv/srem` on edge cases (min int, large values)
Comparisons | Signed vs unsigned ordering around high-bit set
Casts | zext then trunc round-trip, out-of-range detection (semantic tests)
Float | NaN comparison distinctions (optional) / basic arithmetic
Phi | If/else returning merged value; while loop accumulation
Aggregates | struct and array literal values loaded correctly
Globals V2 | Constant array accessible; read-only enforcement (attempt to store -> type checker error)
CLI | IR dump presence, function arg passing
Optimization | Golden IR before/after mem2reg (feature-flagged)

Add golden file tests under `tests/golden/` with expected textual IR (strip variable numbering unpredictability—maybe just assert presence of predicates/opcodes). Consider lightweight pattern matcher: search for lines containing `icmp ult` etc.

## 11. Backward Compatibility & Migration
- Keep old comparison op forms for one phase; emit deprecation warning via stderr when `EDN_WARN_DEPRECATED=1` env var set.
- Document new canonical forms in README & CHANGELOG.

## Proposed Instruction Set Additions (Summary)
Category | Instructions
---------|-------------
Unsigned | `udiv`, `urem`
Remainder | `srem`
Comparisons | `icmp`, `fcmp` (predicates)
Casts | `zext`, `sext`, `trunc`, `bitcast`, `ptrcast`, `sitofp`, `uitofp`, `fptosi`, `fptoui`
Float arithmetic | `fadd`, `fsub`, `fmul`, `fdiv`
Phi | `phi`
Aggregates | `struct-lit`, `array-lit`, `const-struct`, `const-array`

## Data Structures & Internal Refactors
1. Extend `BaseType` enum: add `U8,U16,U32,U64`.
2. Helper: `bool is_integer(TypeId, bool* isSigned, unsigned* bits)`.
3. Predicate mapping table for `icmp` / `fcmp`.
4. Intermediate representation (optional) prior to LLVM emission to facilitate pattern-based improvements (mem2phi).

## Risks & Mitigations
Risk | Mitigation
-----|-----------
Explosion of ad-hoc opcodes | Introduce predicate-driven `icmp/fcmp` instead of many op strings
Complex phi lowering | Start with pattern-based alloca->phi rewrite; only add explicit `(phi ...)` after stable
Optimization brittleness | Provide flag to disable passes; keep baseline tests at O0
Diagnostics scope creep | Implement minimal structured reporter first, iterate
Unsigned semantics errors | Centralize signedness decisions in helper utilities

## Rough Effort Estimate (Relative)
Feature | Size (S/M/L)
--------|--------------
Unsigned ints + cmp refactor | M
Float arithmetic & fcmp | M
Casts suite | M
Phi support (baseline) | L
Aggregate literals | M
Globals V2 constants | M
Diagnostics overhaul | L
CLI improvements | S
Optimization pipeline | M
Test expansion | M

## Implementation Order (Dependency-Aware)
1. Unsigned + `icmp` unification.
2. Division/remainder & new comparisons tests.
3. Float ops & `fcmp`.
4. Cast instructions.
5. Diagnostics module (so later features use improved errors).
6. Phi (introduce explicit form + optional mem2phi pass).
7. Aggregates & global constants (depends on casts for pointer reinterpret if needed).
8. CLI enhancements & optimization pipeline.
9. Golden tests & documentation update.

## Documentation Tasks
- README: new instruction list (Phase 2) separate table.
- CHANGELOG: mark additions & deprecations.
- Add `docs/ir-spec.md` formalizing grammar (BNF-like) for instructions / types / predicates.

## Completion Criteria
Phase 2 considered complete when:
- All new instructions parsed, type-checked, emitted.
- Unsigned & float semantics validated by tests (including edge cases: wrap, division by negative for signed, comparisons near limits).
- `phi` introduced and at least one prior memory-merge pattern eliminated.
- Aggregate literals produce correct memory layout loads.
- CLI can run any sample with `--dump-ir` and pass integer args.
- Diagnostics include variable name + expected/actual types for all type mismatches.
- README & CHANGELOG updated; deprecated forms documented.

---
*Prepared for Phase 2 kickoff. Ready to begin with Unsigned + icmp unification once approved.*
