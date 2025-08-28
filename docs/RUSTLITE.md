# Rustlite Quickstart

Rustlite is a tiny Rust-inspired frontend that lowers to EDN IR. It lives under `languages/rustlite` and can be driven either from the CLI (`rustlitec`) or via the node-based `rustlite::Builder`.

## Build and run tests

```bash
cmake -S . -B build -DEDN_BUILD_TESTS=ON
cmake --build build --target rustlitec rustlite_e2e_driver
ctest --test-dir build/languages/rustlite -R ^rustlite\.
```

## CLI samples

Sample sources live under `languages/rustlite/samples/*.rl.rs`.

Run the CLI on a sample:

```bash
./build/languages/rustlite/rustlitec languages/rustlite/samples/return.rl.rs
```

Representative samples:
- `return.rl.rs` – simple return
- `let_stmt.rl.rs` – let bindings
- `let_mut.rl.rs` – mutable binding and assignment
- `if_stmt.rl.rs` / `else_if_chain.rl.rs` – control flow
- `logical_ops.rl.rs` – short-circuit && and ||
- `fn_params.rl.rs` – function parameters and optional return type

## End-to-end driver

To lower Rustlite → EDN → expand → typecheck → LLVM IR:

```bash
./build/languages/rustlite/rustlite_e2e_driver languages/rustlite/samples/logical_ops_e2e.rl.rs --dump
```

This prints both the high-level EDN and the final IR.

## Grammar subset (current)

### Integer / Bitwise Operators (Phase D Incremental)

Rustlite currently surfaces arithmetic, comparison, and logical operations by emitting explicit `rcall` forms with the operator symbol. Phase D extends the recognized intrinsic binary set so these names lower directly without additional macro wrappers:

Arithmetic / division / remainder:
  add sub mul div (alias for signed sdiv) sdiv udiv srem urem

Bitwise and shifts:
  and or xor shl lshr ashr

Comparisons (unchanged):
  eq ne lt le gt ge

Example forms:
```
(rcall %q i32 and %mask %val)
(rcall %r i32 shl %x %shift)
(rcall %s i32 urem %a %b)
```
All division/rem ops require the operand types to be integer base types; `div` maps to `sdiv`. Unsigned variants must be explicitly requested via `udiv` / `urem`.

Precedence-aware infix parsing for these operators in the high-level `.rl.rs` surface grammar is still pending; at present they must appear as explicit `rcall` invocations in EDN macro form.

- Calls: `(rcall %dst <op-ty or ret-ty> callee args...)` with intrinsic rewrites

### Compound Assignment Sugar (Phase D Incremental)

Rustlite adds a small macro `rassign-op` to express a read-modify-write sequence without manually introducing a temporary. Pattern:

```
(rassign-op %var Ty op %rhs)
```

Expands to a two-step block:

```
(block :body [
  (op %__rl_cassn.N Ty %var %rhs)
  (assign %var %__rl_cassn.N)
])
```

Supported `op` names are the same intrinsic binary operator symbols recognized by `rcall` (see previous section). `div` is treated as an alias for signed `sdiv` just like in direct calls. The macro does not itself perform type inference; you must supply the correct operand/result type `Ty`.

Example:

```
(rassign-op %x i32 add %one) ; %x += 1
(rassign-op %mask i32 and %flag)
```

The generated temporary symbol is gensym'd and not reusable outside the macro expansion.

See `design/rustlite.md` for background and roadmap. See `docs/ENUMS_MATCHING.md` for detailed enum construction and exhaustive matching (`ematch`) semantics and diagnostics (E1600 for non-exhaustive `ematch`).

## Additional Macros (Selected)

### Enum Construction and Matching (`renum`, `rmatch`)
Rustlite supplies a light enum facility over core sum types.

Example:

```
(module :id "enum_demo"
  (renum :name Color :variants [ Red Green Blue ])
  (rfn :name "pick" :ret i32 :params [ ] :body [
    (const %zero i32 0)
    (const %one i32 1)
    (const %two i32 2)
    ;; Construct a variant (lowered to sum injections)
    (const %v i32 1) ; pretend color index
    ;; Match over a value
    (rmatch %out i32 %v [
      (case Red   %zero)
      (case Green %one)
      (case Blue  %two)
    ])
    (ret i32 %out)
  ])
)
```

### Literal Convenience (`rcstr`, `rbytes`)
High level string / raw byte literal helpers. They expand to core ops `cstr` / `bytes` which the emitter lowers into private `i8` array globals and returns an `i8*` (pointer to first element). Identical contents are interned.

Escapes supported in `rcstr`: `\n`, `\t`, `\\`, `\"`, `\0`, `\xNN` (two hex digits). Unknown escapes currently degrade to the escaped character literally (the backslash is dropped). Example: `"a\\qb"` becomes bytes `a q b \0`. This permissive policy is intentional for now and covered by test `rustlite.cstr_unknown_escape`.

```
(rcstr %hello "hello")
(rbytes %hdr [ 0 255 1 2 ])
```

Failure cases (diagnostic codes E1500–E1504, E1510–E1516) include malformed quoting, empty byte arrays, non-integer or out-of-range (0..255) elements.

### Core Literal Ops (`cstr`, `bytes`)
Canonical IR-level forms (you can write directly):

```
(cstr %sym "text")    ; -> %sym : (ptr i8)
(bytes %data [ 1 2 3 ]) ; -> %data : (ptr i8)
```

### External Data Macros (`rextern-global`, `rextern-const`)
Sugar for declaring externally provided data symbols (no initializer). Both forms currently behave identically, adding `:external true` to a core `(global ...)`:

```
(rextern-global :name EXTG :type i32)
; expands to (global :name EXTG :type i32 :external true)
```

If you need a const extern with an initializer you can explicitly write `(global :name X :type T :const true :init ... :external true)` (a const global requires an initializer: E1227). The negative test `rustlite.extern_global_init_neg` asserts that specifying `:const true` without `:init` triggers E1227.

### Interning Behavior
Identical decoded string contents (excluding implicit terminator) or identical raw byte sequences are mapped to a single synthesized global (`__edn.cstr.N` / `__edn.bytes.N`).

### Quick Reference

| Macro | Purpose | Result Type |
|-------|---------|-------------|
| `rcstr %dst "lit"` | Interned C string literal (NUL added) | `ptr i8` |
| `rbytes %dst [ b* ]` | Interned raw bytes | `ptr i8` |
| `renum :name N :variants [ V* ]` | Enum (sum type) declaration | N/A |
| `ematch %dst Ret EnumType %val :arms [...]` | Exhaustive enum match (diagnostic if non-exhaustive) | Ret |
| `rextern-global ...` | External data symbol | N/A |
| `rextern-const ...` | External data symbol (alias) | N/A |
| `rassign-op %v Ty op %rhs` | Compound assignment sugar (`%v = (%v op %rhs)`) | N/A |
| `rfor :init [ ... ] :cond %c :step [ ... ] :body [ ... ]` | For loop sugar | N/A |
| `rfor-range %i Ty <start> <end> :body [ ... ]` | Counted range loop (expands to `rfor`) | N/A |
| `rrange %dst Ty <start> <end> :inclusive <bool>` | Range literal tuple (start,end,inclusive) | tuple type |
| `rassign %dst %src` | Alias for core mutation | N/A |
| `rstruct :name S :fields [ (name Ty)* ]` | Struct type declaration | N/A |
| `rget %dst StructType %obj field` | Load struct field | field type |
| `rset %obj field %src` | Store to struct field | N/A |
| `tuple %dst [ %v* ]` | Tuple literal (auto-declares `__TupleN`) | `__TupleN` |
| `tget %dst Ty %t <idx>` | Tuple field access | `Ty` |
| `arr %dst Ty [ %v* ]` | Fixed-size array literal (core `array-lit`) | `ptr (array Ty N)` |
| `rindex %dst (ptr T) %base %idx` | Pointer to element (no load) | `ptr T` |
| `rindex-load %dst T %base %idx` | Load element | `T` |
| `rindex-store %base %idx %src` | Store element | N/A |
| `rclosure %dst (ptr (fn-type ...)) (captures [ %cap* ]) :body [ ... ]` | Allocate closure env + thunk | closure ptr |
| `rcall-closure %dst Ret %clos %arg*` | Invoke closure | Ret |
| `rfnptr %dst (ptr (fn-type ...)) fnName` | Obtain function pointer | pointer type |
| `rsome %dst SumType %val` | Wrap Some/Ok variant | SumType |
| `rnone %dst SumType` | Construct None | SumType |
| `rok %dst SumType %val` | Construct Ok | SumType |
| `rerr %dst SumType %val` | Construct Err | SumType |
| `rmatch %dst Ret SumType %val :arms [...] :else [...]` | Match sum | Ret |
| `rif-let %dst Ret SumType Variant %val ...` | Conditional extract | Ret |
| `rtry %dst ResultType %sum` | Unwrap or early-return (Err / None) | binds payload |
| `rwhile-let SumType Variant %sum :bind %x :body [ ... ]` | Loop while variant holds | N/A |

Diagnostic ranges: literals use E150x (cstr) and E151x (bytes).

---

### Loop Sugar: `rfor` and `rfor-range`

```
(rfor :init [ <init forms>* ] :cond %condSym :step [ <step forms>* ] :body [ <body forms>* ])

(rfor-range %i Ty <start-int> <end-int> :body [ <body forms>* ])
; expands to a counted (for ...) with init: %i=<start>, limit=<end>, one=1, cond=%i < limit
```

`rfor-range` is a convenience for the exceedingly common counted loop pattern. Example (direct literal bounds):

```
(rfor-range %i i32 0 4 :body [ (add %tmp i32 %i %i) ])
```

Expands (conceptually) into:

```
(for :init [ (const %i i32 0)
       (const %__rl.end.N i32 4)
       (const %__rl.one.N i32 1)
       (lt %__rl.cond.N i1 %i %__rl.end.N) ]
     :cond %__rl.cond.N
     :step [ (add %__rl.next.N i32 %i %__rl.one.N)
       (assign %i %__rl.next.N)
       (lt %__rl.cond.N i1 %i %__rl.end.N) ]
     :body [ (add %tmp i32 %i %i) ])
```

All internal temporaries are gensym'd; only `%i` is user-visible/mutable.

Range literal + adapter (tuple form):

```
(rrange %r i32 0 4 :inclusive false)
(rfor-range %i i32 %r :body [ (add %tmp i32 %i %i) ])
```

`rrange` constructs a tuple `[start end inclusive]` (the `inclusive` flag is reserved; current `rfor-range` ignores it and treats ranges as half-open). The adapter form detects a tuple argument and emits initialization by extracting fields with `tget`.

Expands directly to core `(for ...)`. You maintain the condition symbol (e.g. recompute `%cond` inside `:step`).

### Mutation Sugar: `rassign`

`rassign` is a direct alias for core `(assign %dst %src)`. Prefer it in macro expansions mirroring Rust surface `x = expr`.

```
(rassign %x %y)
```

### Struct & Field Access (`rstruct`, `rget`, `rset`)

Structs declare a nominal aggregate; field access uses name-based lookup lowered to index-based GEP + load/store operations. Declaration validation now enforces:

- Non-empty `:name` (E1400) and presence of a `:fields` vector (E1401)
- Each field list shaped like `(field :name sym :type <type>)` (malformed => E1402)
- Field must have a name (E1403) and valid parsable type (E1404)
- No duplicate field names (E1405)
- No duplicate struct redefinition (E1406)
- `:fields` cannot be empty (E1407)

These diagnostics appear during the initial module struct collection phase so later member accesses only see well-formed aggregates.

```
(module :id "struct_demo"
  (rstruct :name Pair :fields [ (a i32) (b i32) ])
  (rfn :name "sum" :ret i32 :params [ ] :body [
    (const %one i32 1) (const %two i32 2)
    (alloca %p (ptr Pair))
    (rset %p a %one)
    (rset %p b %two)
    (rget %x Pair %p a)
    (rget %y Pair %p b)
    (add %r i32 %x %y)
    (ret i32 %r)
  ])
)
```

### Indexing (`rindex*` family)

```
(rindex %eltPtr (ptr i32) %base %idx)      ; pointer math only
(rindex-load %val i32 %base %idx)          ; load value
(rindex-store %base %idx %src)             ; store value
```
`%base` is expected to be a pointer to the element type. Bounds are unchecked.

### Tuples (`tuple`, `tget`)

Positional tuples are modeled as synthetic structs named `__TupleN` with fields `_0 .. _{N-1}`. The `tuple` macro records arity and auto-injects a single struct declaration per distinct arity used in the module.

```
(tuple %t [ %a %b %c ]) ; arity 3 -> (struct-lit %t __Tuple3 [ _0 %a _1 %b _2 %c ])
```

Field type inference: each tuple struct field type is inferred (when possible) from a preceding defining `(const %sym <Ty> ...)` or `(as %sym <Ty> ...)` for the value symbol supplied. Unresolved fields fall back to `i32`. (Heuristic – future expansion may widen sources or require explicit typing.)

Access:

```
(tget %x i32 %t 1) ; -> (member %x __Tuple3 %t _1)
```

Static out-of-range indices abort macro expansion (will surface as diagnostic E1601 once mapped). Maximum supported arity: 16.

### Arrays (`arr`, legacy `rarray` numeric size)

`arr` lowers directly to a core array literal form:

```
(arr %a i32 [ %x %y %z ]) ; -> (array-lit %a i32 3 [ %x %y %z ])
```

Legacy numeric-size path:

```
(rarray %buf i32 8) ; -> (alloca %buf (array :elem i32 :size 8))
```

No initialization is performed for the numeric form. Prefer `arr` when providing explicit element values. Future bounds checking (Phase D) will gate on `RUSTLITE_BOUNDS=1`.

### Closures (`rclosure`, `rcall-closure`)

Rustlite closures list captured variables (by value copy). The macro builds a tiny struct `{ fnPtr, env }` plus a thunk receiving the environment pointer first.

```
(rclosure %clos (ptr (fn-type :params [ i32 ] :ret i32)) (captures [ %cap ]) :body [
  (add %tmp i32 %cap %arg0)
  :value %tmp
])
(rcall-closure %r i32 %clos %arg0)
```

### Option / Result Helpers (`rsome`, `rnone`, `rok`, `rerr`, `rif-let`, `rmatch`)

Thin sugar over sum construction + match patterns. Canonical `OptionT` / `ResultT` are modeled as two-variant sums (tag + payload).

```
(const %seven i32 7)
(rsome %o OptionI32 %seven)
(rmatch %r i32 OptionI32 %o :arms [ (arm Some :binds [ %x ] :body [ :value %x ]) ] :else [ (const %z i32 0) :value %z ])
(rif-let %r2 i32 OptionI32 Some %o :bind %x :then [ :value %x ] :else [ (const %zero i32 0) :value %zero ])
```

### Early Return Sugar (`rtry` – `?` semantics)

`rtry` unwraps a `Result*` / `Option*` value or performs an early return with the error / none variant (analogous to Rust's `?`). It is currently an explicit macro (surface `?` sugar may later desugar to this form).

Form:
```
(rtry %bindVar ResultI32 %sumSym)
```

Behavior:
- `%sumSym` must be a symbol of a sum type whose name starts with `Result` or `Option`.
- Result: `Err(e)` => early `(ret ResultI32 e)`; `Ok(v)` => bind payload to `%bindVar` and continue.
- Option: `None` => early return of that `None`; `Some(v)` => bind `%bindVar`.

Implementation detail: sum constructors yield a pointer to the sum value; the macro inserts an internal `(rderef ...)` before returning so the function sees the expected value type (not pointer) (see EDN-0011 Phase C notes). Driver code still dereferences the final success construction before the outer `ret`.

Simplified expansion (Result case):
```
(rmatch %__flag i1 ResultI32 %sumSym :arms [
  (arm Ok :binds [ %bindVar ] :body [ (const %true i1 1) :value %true ])
] :else [
  (rderef %__tmp ResultI32 %sumSym)
  (ret ResultI32 %__tmp)
  (const %false i1 0) :value %false
])
```

Ok path example:
```
(const %forty i32 40)
(rok %r1 ResultI32 %forty)
(rtry %x ResultI32 %r1)
(const %two i32 2)
(add %sum i32 %x %two)
(rok %out ResultI32 %sum)
(rderef %rv ResultI32 %out)
(ret ResultI32 %rv)
```

Err short‑circuit:
```
(const %five i32 5)
(rerr %e ResultI32 %five)
(rtry %x ResultI32 %e) ; returns here
```

Option behaves analogously with `rsome` / `rnone` and `Some` / `None` variant names.

### Loop Pattern Matching (`rwhile-let`)

Loop while an expression evaluates to a specific variant, binding its payload each iteration.

Form:
```
(rwhile-let SumType Variant %expr :bind %x :body [ ... ])
```

Expansion outline:
1. Evaluate `%expr` at loop head.
2. `rmatch` on the sum value.
3. On matching `Variant`, bind payload(s), execute `:body`, then continue.
4. Else branch triggers a `break`.

Example (conceptual):
```
(rwhile-let OptionI32 Some %opt :bind %v :body [
  (add %acc i32 %acc %v)
  ; refresh %opt here (omitted)
])
```
Driver `rustlite.rwhilelet` covers a basic lowering case.

### Function Pointers (`rfnptr`)

Validates that the annotated pointer type matches the named function (diagnostics E1323/E1324/E1329 for mismatches) and yields a typed pointer usable with `call-indirect`.

```
(rfnptr %fp (ptr (fn-type :params [ i32 i32 ] :ret i32)) add2)
(call-indirect %r i32 %fp %x %y)
```

### Test Matrix

| Driver / Test Name | Focused Macros Covered |
|--------------------|------------------------|
| `rustlite.struct_fields` | rstruct, rget, rset |
| `rustlite.indexing` | rindex, rindex-load, rindex-store |
| `rustlite.closure` | rclosure, rcall-closure |
| `rustlite.closure_neg` | rclosure (capture omission diagnostic) |
| `rustlite.option_result` | rsome, rnone, rok, rerr, rmatch, rif-let |
| `rustlite.fnptr` | rfnptr, call-indirect |
| `rustlite.tuple_basic` | tuple, tget |
| `rustlite.index_addr` | rindex-addr, rindex-load, rindex-store |
| `rustlite.tuple_array` | arr (array literal) |
| `rustlite.trait_neg` | rtrait-call (wrong arity E1325) |
| `rustlite.extern-globals` | rextern-global, rextern-const |
| `rustlite.extern-globals_neg` | rextern-global (duplicate symbol EGEN) |
| `rustlite.rfor` | rfor |
| `rustlite.extern_global_init_neg` | rextern-const (const without :init E1227) |
| `rustlite.cstr_unknown_escape` | rcstr (permissive unknown escape behavior) |
| `rustlite.make_trait_obj` | rmake-trait-obj, rtrait-call |
| `rustlite.enum_surface` | renum (enum surface) |
| `rustlite.ematch_exhaustive` | ematch (all variants covered) |
| `rustlite.ematch_non_exhaustive` | ematch (E1600 non-exhaustive diagnostic) |
| `rustlite.ematch_payload` | ematch (payload binding) |
| `rustlite.rmatch_non_exhaustive_legacy` | core match (legacy E1415) |
| `rustlite.rtry_result` | rtry (Result Ok / Err) |
| `rustlite.rtry_option` | rtry (Option Some / None) |
| `rustlite.rwhilelet` | rwhile-let loop variant binding |

Extend as additional drivers land (e.g., `rextern_global_init_neg`, `rmake_trait_obj`).

## Feature Flags
> Test Strategy Note: Surface-vs-macro layered tests are tracked in issue EDN-0013 to ensure parser output remains aligned with macro expectations.

Environment flags gate incremental / experimental behavior. Recognized flags (see issue EDN-0011 for roadmap context):

| Env Var | Purpose | Default | Status |
|---------|---------|---------|--------|
| `RUSTLITE_BOUNDS` | Enable bounds checks in (future) rindex / array related macros (emit compare + panic on OOB). | Off (unset/0) | Planned (not yet honored) |
| `RUSTLITE_INFER_CAPS` | Enable closure capture inference prototype when an `rclosure` omits explicit `:captures`. | Off | Implemented (heuristic) |

### Closure Capture Inference (`RUSTLITE_INFER_CAPS`)

When set (any non-empty / non-"0" value), a pre-expansion pass scans vector sequences for `(rclosure %c callee ...)` forms lacking a `:captures` keyword. If the *immediately preceding* instruction defines a symbol via `(const %sym Ty ...)` or `(as %sym Ty ...)`, that symbol is inferred and the form is rewritten to:

```
(rclosure %c callee :captures [ %sym ] ...)
```

Notes / Limitations:
* Only a single prior symbol is captured (no multi-symbol or deep free-variable analysis yet).
* Explicit `:captures` always wins; no inference occurs if provided.
* Disabled tests (`rustlite.closure_infer_disabled`) ensure no rewrite when the flag is off.
* Future work may generalize to scanning free symbols inside the closure body.

### Inclusive Range Literal Sugar

Literal `rfor-range` loops now support an inclusive upper bound via a `:inclusive true` keyword when using the literal form:

```
(rfor-range %i :from 0 :to 4 :inclusive true :body [ ... ])
```

This lowers using a `<=` comparison instead of `<`. Currently only the literal form (`:from` / `:to` integers) accepts `:inclusive`; the tuple / symbol form still uses exclusive semantics.
