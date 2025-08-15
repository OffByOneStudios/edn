# Phase 3 Plan – C-like Semantic Expansion (Finalized)

Status: Complete (2025-08-15) – All in‑scope Phase 3 features implemented & tested in S-expression form. Remaining originally proposed items explicitly marked Out of Scope per latest directive.

## Completion Snapshot (2025-08-15)
Implemented (tests passing):
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
11. Diagnostics upgrade: Structured expected/found type notes for phi incoming mismatch (E0309) and global const initializer mismatch (E1220/E1223/E1225) plus existing arithmetic / call / member cases.
12. Diagnostics JSON mode: `EDN_DIAG_JSON=1` emits machine-readable errors/warnings/notes (diagnostics_json.hpp) – covered by dedicated test.

Out of Scope (removed from Phase 3 scope):
1. Separate C-like surface parser (retained single S-expression surface instead).
2. Extended suggestion coverage audit beyond currently implemented symbol spaces.
3. Source span mapping for alternate surface syntax.
4. Optimization hooks / pass pipeline (`EDN_ENABLE_PASSES`).
5. Union write convenience op & active variant tracking.
6. Additional JSON test matrices for every error family (core representative cases only).

Documentation & error code tables have been updated (README, error_codes.md). All tests green.


## Original Vision (Superseded)
An earlier variant targeted a separate C-like parser; this was intentionally de-scoped. The single homoiconic S-expression surface now directly accumulates C-like semantic features (pointers, enums, unions, loops, switch, variadics) without an alternate grammar.

## Guiding Principles
1. Incrementality: Each milestone adds one coherent surface feature (e.g. return statements, variable declarations) and corresponding lowering.
2. Reuse: Leverage existing EDN IR rather than invent new middle layers (optionally add an intermediate later if complexity justifies it).
3. Determinism: Keep lowering rules explicit and simple; no implicit integer promotions beyond what Phase 2 semantics already define until needed.
4. Diagnostics Parity: Map parser + lowering errors to existing structured error framework; enrich messages with source spans.
5. Test First: For each feature add positive, negative, and (when interesting) round‑trip or JIT execution tests.

## Final Scope Summary
Implemented directly as new S-expression forms; no separate surface translation pipeline introduced.

## Grammar (Initial Subset)
# Phase 3 Plan – Extend S-Expression Language with C-Like Features

Status: Revised (2025-08-14) – Removed standalone C parser scope. We continue evolving the existing EDN S-expression DSL, adding constructs that approximate C semantics while preserving one uniform homoiconic representation.

## Active Vision (Retained)
Continue evolving the S-expression DSL with additive forms while maintaining stable error code allocations and structured diagnostics (including JSON output). Future phases may re-evaluate optimization or alternate surfaces.

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

## Completion Criteria (Met)
1. All planned semantic forms implemented with documented error codes.
2. Positive & negative tests present for each new form (pointer arithmetic, addr/deref, fnptr, typedef, enum, union, variadic, for/continue, switch, cast sugar, global const init notes).
3. Structured mismatch notes for phi incoming & global const initializer cases.
4. JSON diagnostics mode covered by dedicated test.
5. README & error_codes.md updated.

---
Phase 3 is now closed. Further work proceeds under future phase design docs or targeted proposals (e.g., optimization pipeline, extended suggestions, alternate surfaces) and is not part of this completed scope.
