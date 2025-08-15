# Phase 3 Plan – C-like Surface Frontend & Semantic Expansion

Status: In Progress (2025-08-14) – core Phase 3 feature set functionally implemented in S-expression form; remaining work focused on diagnostic polish & optional optimization hooks.

## Current Progress Snapshot (2025-08-14)
Completed (implemented + tests passing):
1. Pointer arithmetic: `(ptr-add)`, `(ptr-sub)`, `(ptr-diff)` (E130x) with scaling + diff semantics.
2. Address-of / Deref: `(addr)` plus deref sugar path via existing `load`/`store` (E131x).
3. Function pointers & indirect call: `(call-indirect ...)`, function type literal parsing & validation (E132x).
4. Typedef aliases: `(typedef ...)` integration in type resolver (E133x).
5. Enums: `(enum ...)` constants usable as typed symbols (E134x).
6. Unions: `(union ...)` definition + member access validation (E135x).
7. Variadic functions & runtime intrinsics: vararg params, `(va-start / va-arg / va-end)` plus runtime support (E136x).
8. For loop & Continue: `(for ...)` desugaring + `(continue)` handling (E137x/E138x).
9. Switch construct: `(switch ...)` lowering to existing control flow (E139x).
10. Cast sugar: `(as %dst <type> %src)` generic dispatcher (E13Ax).
11. Diagnostics upgrade (phase 1): Structured expected/found type notes via `type_mismatch` helper applied to arithmetic, comparisons, calls (direct & indirect), pointer ops, addr/deref, loads/stores, returns, assignment, members, indexing, struct/array literals, gload/gstore.
12. Diagnostics JSON mode: `EDN_DIAG_JSON=1` emits machine-readable errors/warnings/notes (diagnostics_json.hpp).

Partially Complete / Outstanding:
1. Remaining mismatch conversions: confirm / convert global initializer literal mismatches (E1220/E1223/E1225) and ensure phi incoming (E0309) still uses structured notes.
2. Suggestion coverage: baseline fuzzy suggestions exist for some symbol spaces; need audit & possible addition for union field names and typedef/enum name typos (if any gaps remain).
3. Source span mapping: current diagnostics still location-limited to IR node context; full span mapping from a higher-level parser (if reintroduced) deferred.
4. Optimization hooks: EDN_ENABLE_PASSES flag & simple pass pipeline not yet implemented.
5. Documentation polish: README update for new forms & JSON diagnostics usage; error code reference table expansion.

Deferred / Explicitly Out of Scope (current iteration):
* Separate C-like surface parser (we retained single S-expression surface per revised vision below).

Immediate Next Steps:
* Finish remaining mismatch conversions (global init / phi audit) and add tests asserting JSON note presence.
* Implement optional simple optimization pass toggle scaffold.
* Expand docs & finalize error code reference before declaring Phase 3 complete criteria met.

Progress Metrics:
* All implemented feature tests pass (pointer arith, addr/deref, fnptr, typedef, enum, union, variadic runtime, for/continue, switch, cast sugar).
* No open compilation failures; test suite green after diagnostics refactor.


## Vision
Introduce a minimal C-like surface syntax that lowers to the existing EDN IR, enabling more natural authoring while reusing the mature type checking, diagnostics, and LLVM emission pipeline built in Phases 1–2. Deliver in thin vertical slices: parse a tiny program, lower, type-check, emit, run. Then iterate feature-by-feature, keeping tests green.

## Guiding Principles
1. Incrementality: Each milestone adds one coherent surface feature (e.g. return statements, variable declarations) and corresponding lowering.
2. Reuse: Leverage existing EDN IR rather than invent new middle layers (optionally add an intermediate later if complexity justifies it).
3. Determinism: Keep lowering rules explicit and simple; no implicit integer promotions beyond what Phase 2 semantics already define until needed.
4. Diagnostics Parity: Map parser + lowering errors to existing structured error framework; enrich messages with source spans.
5. Test First: For each feature add positive, negative, and (when interesting) round‑trip or JIT execution tests.

## Milestones Overview
| Milestone | Theme | Vertical Slice Output |
|-----------|-------|-----------------------|
| 3.1 | Minimal TU | `int main(){return 0;}` parses, lowers, runs (JIT) |
| 3.2 | Returns & Int Exprs | Constant & simple binary int expressions in `return` |
| 3.3 | Local Vars | `int x=1;` declarations, block scope, simple init, return variable |
| 3.4 | Expressions | Precedence & associativity for + - * / % (sdiv/udiv based on type) |
| 3.5 | Conditionals | `if`, `if/else` (lower to existing control IR + phi where needed) |
| 3.6 | Loops | `while`, later `for` desugared to `while` |
| 3.7 | Pointers | `&`, `*`, pointer deref/load/store, pointer arithmetic (+/- integer) |
| 3.8 | Functions | Multiple functions, parameters, calls, prototype ordering |
| 3.9 | Structs | `struct` declarations, member access `.` and `->` lowering to member/member-addr |
| 3.10 | Arrays | Static arrays, indexing, decay to pointer when passed to functions |
| 3.11 | Casts | Explicit `(type)` casts mapped to Phase 2 cast opcodes |
| 3.12 | Enums & Typedef | Enum constants (integral), typedef name substitution |
| 3.13 | Unions | Shared storage layout, basic access checks |
| 3.14 | Function Pointers | Declaration, assignment, indirect call lowering |
| 3.15 | Variadics | `...` params, builtin va_start/arg/end intrinsic lowering |
| 3.16 | Switch/Continue | Control constructs: `switch`, `continue` |
| 3.17 | Diagnostics Upgrade | Source span mapping, expected/actual mismatch notes, JSON output mode |
| 3.18 | Optimization Hooks | Optional simple passes (mem2reg, instcombine) post-lowering |

(Actual ordering may interleave when dependencies allow.)

## Grammar (Initial Subset)
# Phase 3 Plan – Extend S-Expression Language with C-Like Features

Status: Revised (2025-08-14) – Removed standalone C parser scope. We continue evolving the existing EDN S-expression DSL, adding constructs that approximate C semantics while preserving one uniform homoiconic representation.

## Vision
Keep authoring in S-exprs; introduce new forms/opcodes for features traditionally associated with C (pointers, enums, unions, control constructs, variadics, etc.). Avoid a second surface syntax. Each feature is an additive IR form plus type checking + emission support.

## Guiding Principles
1. Single Surface: Only S-exprs; no parallel C grammar.
2. Composability: New forms nest like existing `(if ...)`, `(while ...)` constructs.
3. Explicitness: Prefer explicit ops over implicit coercions (e.g. `(ptr-add ...)` instead of overloading `add`).
4. Incremental Delivery: One feature per milestone with tests & docs.
5. Diagnostics First: Every new form has defined error code range & negative tests.

## Milestones (S-Expr Feature Additions)
| Milestone | Feature | New / Extended Forms (Draft) |
|-----------|---------|------------------------------|
| 3.1 | Pointer Arithmetic Basics | `(ptr-add %dst (ptr <T>) %base %offset)` `(ptr-sub ...)` producing pointer; integer difference form `(ptr-diff %dst isize %a %b)` |
| 3.2 | Address-of & Deref | `(addr %dst (ptr <T>) %val)` (SSA -> pointer); existing `load`/`store` suffice for deref; add `(deref %dst <T> %ptr)` sugar (optional) |
| 3.3 | Function Pointers | Function type literal `(ftype :ret <type> :params [ <type>* ] :vararg? bool)` and `(bitcast %fptr (ptr <ftype>) %fn)` + `(call-indirect %dst <ret> %fptr %arg...)` |
| 3.4 | Typedef / Aliases | `(typedef :name Alias :type <type>)` (affects type resolver symbol table) |
| 3.5 | Enums | `(enum :name E :underlying i32 :values [ (eval :name A :value 0) (eval :name B :value 1) ])` -> constants become symbols |
| 3.6 | Unions | `(union :name U :fields [ (ufield :name a :type i32) (ufield :name b :type f32) ])` + access via `(union-member %dst U %ptr field)` |
| 3.7 | Variadic Functions | Extend `fn` form `:vararg true`; add intrinsic forms `(va-start ...) (va-arg ...) (va-end ...)` |
| 3.8 | For Loop | `(for :init [ <stmts>* ] :cond %c :step [ <stmts>* ] :body [ <stmts>* ])` lowered to while + init + step sequencing |
| 3.9 | Continue | `(continue)` inside `while/for` blocks -> block structure update |
| 3.10 | Switch | `(switch %expr :cases [ (case <const> [ <stmts>* ])* ] :default [ <stmts>* ])` lowered to nested if / jump table |
| 3.11 | Advanced Casts | Implicit convenience wrappers or synonyms mapping to existing cast instructions (e.g. `(as %dst <type> %src)`) |
| 3.12 | Enhanced Diagnostics | Expected vs actual notes, suggestion coverage expansion for new symbol spaces (typedef, enum, union fields) |
| 3.13 | JSON Diagnostics | `(diagnostics :format json)` mode or CLI flag environment var `EDN_DIAG_JSON=1` |
| 3.14 | Optimization Hooks | Optional post-emit pass pipeline flag `EDN_ENABLE_PASSES=1` |

Ordering can shift if dependencies arise.

## Proposed New Forms (Detailed Draft)

### Pointer Arithmetic
```
(ptr-add %p2 (ptr <T>) %p1 %i) ; %i integer (signed/unsigned) scaled by sizeof(T)
(ptr-sub %p2 (ptr <T>) %p1 %i)
(ptr-diff %d isize %pA %pB) ; result = (#elements difference) signed integer
```
Validation: base pointer type matches; offset integer type accepted (any int); scaling by element size done in emitter (LLVM GEP). Error codes range E1300–E1309.

### Address-of / Deref
```
(addr %p (ptr <T>) %v) ; produce pointer to existing SSA value (will require materializing value on stack)
(deref %v <T> %p)      ; sugar: expands to (load %v <T> %p)
```
If value has no stable address, implicitly allocate stack slot (one-time) and store. Error codes E1310–E1319.

### Function Pointers & Indirect Call
Function type literal reused for validation:
```
(call-indirect %dst <ret-type> %fptr %arg1 %arg2 ...)
```
Type checker: ensure `%fptr` is `(ptr (fn-type ...))`. Error codes E1320–E1329.

### Typedef
```
(typedef :name SizeT :type u64)
```
Adds alias to type symbol table; appears in subsequent `<type>` positions. Error codes E1330–E1334 for redefinition, unknown target.

### Enums
```
(enum :name Color :underlying u8 :values [ (eval :name RED :value 0) (eval :name BLUE :value 1) ])
```
Enum constant usage: symbol resolves to underlying integer literal with adopted type. Error codes E1340–E1349.

### Unions
```
(union :name U :fields [ (ufield :name i :type i32) (ufield :name f :type f32) ])
(union-member %v U %ptr i) ; load active field (user responsibility for correct active member) 
```
Type checking: union size = max(field sizes); alignment = max(field alignment). Error codes E1350–E1359.

### Variadic Functions
```
(fn :name "printf" :ret i32 :params [ (param (ptr i8) %fmt) ] :vararg true :body [ ... ])
```
Add intrinsic forms for varargs frame management (later). Error codes E1360–E1369.

### For Loop
Desugaring sketch:
```
(for :init [ S1 S2 ] :cond %c :step [ Sstep1 ] :body [ B1 B2 ])
=> S1 S2 (while %c [ B1 B2 Sstep1 ])
```
Error codes E1370–E1379.

### Continue
`(continue)` allowed only inside loop; error E1380 for misuse.

### Switch
```
(switch %expr :cases [ (case <const0> [ ... ]) (case <const1> [ ... ]) ] :default [ ... ])
```
Lowering: chain of `(if ...)` or jump table when dense (future). Error codes E1390–E1399.

### Generic Cast Sugar
```
(as %dst <to-type> %src) ; dispatcher choosing existing cast opcode
```
Error E13A0–E13A5 for invalid conversions.

## Diagnostics Extensions
Add suggestion spaces:
1. Enum constant misspelling (reuse edit distance).
2. Typedef name misspelling.
3. Union field name.

Expected vs actual note template:
`expected <T_expected> but got <T_actual> in <context>` appended as secondary note.

## Error Code Range Allocation (Tentative)
| Range | Feature |
|-------|---------|
| E130x | Pointer arithmetic |
| E131x | Address-of / deref |
| E132x | Function pointers / indirect call |
| E133x | Typedef |
| E134x | Enum |
| E135x | Union |
| E136x | Variadic |
| E137x | For loop |
| E138x | Continue misuse |
| E139x | Switch |
| E13Ax | Generic cast sugar |

## Testing Strategy (Per Milestone)
Each milestone adds:
1. Positive execution test (end-to-end through JIT where meaningful).
2. Negative tests (invalid type, undefined symbol, misuse of feature).
3. Diagnostic suggestion test (where applicable).

## Implementation Order (Revised)
1. Pointer arithmetic (foundation for later pointer-heavy features).
2. Address-of / deref (enables variable address semantics needed by function pointers / variadics).
3. Function pointers + indirect call.
4. Typedef & enums (lightweight symbol additions).
5. For loop & continue (control flow completeness).
6. Switch (bigger control feature; can leverage existing if/while lowering).
7. Unions (layout + basic access diagnostics).
8. Variadics (more complex calling convention details – postpone until base stable).
9. Cast sugar & diagnostic enhancements.
10. JSON diagnostics + optimization hook.

## Risks & Mitigations
| Risk | Mitigation |
|------|------------|
| Feature sprawl | Strict milestone boundaries; no overlapping partial implementations in main. |
| Pointer arithmetic correctness | Central size/align helper; unit tests for scaling and differences. |
| Union misuse (no active member tracking) | Initial phase: document undefined behavior; later add optional runtime tag pattern. |
| Variadics ABI complexity | Defer until core pointer + function pointer infrastructure stable. |

## Completion Criteria
Phase 3 complete when:
1. All milestone forms implemented with documented error codes.
2. Test suite covers positive & negative cases for each new feature.
3. Diagnostics support suggestions for new symbol spaces (enum, typedef, union field).
4. JSON mode emits structured diagnostics (optional flag/env var).
5. README & error code reference updated with new forms & ranges.

---
Next actionable milestone: 3.1 Pointer Arithmetic – design exact operand order, implement type checker rules, add emitter GEP logic, introduce E1300–E1305 codes, tests (add, sub, diff, type errors).
