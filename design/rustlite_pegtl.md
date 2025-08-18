# Rustlite PEGTL Parser Plan (Phase 5)

Status: Plan (2025-08-17)
Audience: EDN/Rustlite contributors
Goal: Specify a small, practical PEGTL-based parser for the Rustlite surface that lowers to existing Rustlite macros and EDN Core IR.

## Objectives and scope

What we will parse now (subset):
- Items: functions (fn), extern "C" fn declarations; optional struct/enum/trait stubs for sugar mapping (minimal for v1).
- Statements: let/mut bindings; expression statements; return; break/continue; blocks { ... }.
- Control flow: if/else; while; loop; match.
- Expressions: literals (int, float, bool), identifiers, path expressions (`Foo::bar`, `Trait::method`), field/index (`a.b`, `a[i]`), calls, unary/binary ops (selected), method-call sugar (`obj.method(args)` and `Trait::method(obj, args)`), address-of `&x` and deref `*p` (to current EDN sugar).
- Patterns in match: `_`, identifier binding, tuple-like sum constructors: `Option::Some(x)`, `Option::None`, `Result::Ok(v)`, `Result::Err(e)`.

# Rustlite PEGTL parser notes
Out of scope (v1):
- Borrow checker/lifetimes/ownership semantics; references are parsed but only for lowering convenience.
- Full generics/type inference; allow explicit type annotations where required; minimal generic instantiation demo only.
- Macros (Rust-style) beyond our own internal lowering.

Deliverable contract

## Architecture

- PEGTL grammar (header-only): top-down recursive-descent with prioritized rules.
- AST: a thin Rustlite-specific AST (C++ structs) that is easy to lower to our macro EDN.
- Lowering: AST -> Rustlite macro EDN using existing forms in `languages/rustlite/src/expand.cpp`.

## Tokens and lexing rules

- Whitespace/comments: skip spaces, tabs, newlines; line comments `// ...\n`; block comments `/* ... */` with nesting not required (v1: non-nested).
- Identifiers: `[A-Za-z_][A-Za-z0-9_]*` (no Unicode escapes in v1).
- Keywords: `fn`, `extern`, `"C"`, `let`, `mut`, `if`, `else`, `while`, `loop`, `break`, `continue`, `return`, `match`, `true`, `false`.
- Literals: integer (decimal, optional `-`), float (simple `123.45`), bool, character and string (v1 support string for `rcstr`).
- Symbols: `::`, `->`, `=>`, `==`, `!=`, `<=`, `>=`, `&&`, `||`, `&`, `*`, `=`, `:`, `,`, `;`, `.`, `(`, `)`, `{`, `}`, `[`, `]`.

## Grammar sketch (PEGTL-style)

- module := ws item* eof
- item := function | extern_fn | struct_decl? | enum_decl? | trait_decl? | impl_block?
  - call := `(` arg_list? `)`
  - field := `.` ident
  - index := `[` expr `]`
- primary := literal | ident_or_path | `(` expr `)`
- ident_or_path := ident ( `::` ident )*
- loop_expr := `loop` block
- match_expr := `match` expr `{` ( pattern `=>` expr_or_block `,` )* `}`
- pattern := `_` | ident | path `(` pattern_list? `)` | path

## Type grammar (minimal)
- ty := simple_path | `i32`|`i64`|`u32`|`u64`|`f32`|`f64`|`bool`| pointer_ty | array_ty
- pointer_ty := `*mut` ty | `*const` ty | `&` ty (semantic sugar only)
- array_ty := `[` ty `;` int `]`

## Lowering to Rustlite macros / EDN core

- Module: emit `(module ...items...)`.
- Functions: `fn name(args)->ret { body }` → `(rfn :name "name" :params [...] :ret Ty :body [ ...lower(body)... ])` (or direct `fn` if preferred).
- Extern C fn: `extern "C" fn name(args)->ret;` → `(rextern-fn :name "name" :ret Ty :params [...])`.
- Let/mut: `let x: T = e;` → `(rlet T %x %e :body [...])` (block-scoped; if inside a block, lower to `as` form). `let mut` → `rmut`.
- Assign: `x = e` → `(rassign %x %e)` or `as` on existing SSA name per our macro convention.
- If/else: `if c { ... } else { ... }` → `rif/relse` sugar over core `if`.
- While/loop/break/continue: map to `rwhile` / `rloop` / `rbreak` / `rcontinue`.
- Return: `return e;` → `(rret Ty %e)` or core `ret` with explicit type.
- Calls: `f(a,b)` → `(rcall %dst RetTy f [ %a %b ])` when context expects a value; inline temporaries as needed.
- Method call sugar:
  - `obj.method(a)` → `(rdot %dst RetTy method %obj %a)` (free-fn-with-receiver form) or trait form if qualified.
  - `Trait::method(obj, a)` or `obj.Trait::method(a)` → `(rdot %dst RetTy Trait.method %obj %a)`.
- Field/index:
  - `obj.field` → `(rget %dst Ty %obj field)`
  - `obj.field = v` → `(rset %obj field %v)`
  - `arr[i]` → `(rindex %dst ElemTy %arr %i)` etc.
- Unary `&e`/`*p`: map to `raddr`/`rderef` as appropriate.
- Short-circuit `&&`/`||`: lower to `rand`/`ror` (SSA-safe form already implemented).
- Match and patterns:
  - `match e { Option::Some(x) => t, Option::None => u }` → `rmatch` with `sum-is`/`sum-get` combo or `rif-let` when single positive arm binds then yields.
  - `Result::Ok(v)` / `Err(e)` similarly.
  - `_` arm → default.

## Error handling & diagnostics

- Every parse error includes: file, line, column, expected token/rule, and a short message.
- Strategy: fail-fast inside expressions; recover at `;` or `}` for statements/blocks; for items, recover at top-level `fn`/`extern`/`}` boundaries.
- Map selected semantic checks (e.g., duplicate param names) during lowering; otherwise let the EDN type checker produce rich diagnostics (we pass through locations when possible).

## Implementation plan & milestones

M0: Skeleton and harness
- Create `languages/rustlite/parser` library with basic PEGTL scaffolding and position tracking.
- Add minimal driver (CLI optional) and unit test that parses an empty module and one empty function.

M1: Expressions (core)
- literals, identifiers/paths, calls, precedence climbing, parens; produce AST and EDN for expression statements returning constants.

M2: Statements & blocks
- let/mut, expr statements, return, blocks; lower to `rlet/rmut` and `rret`.

M3: Control flow
- if/else, while, loop/break/continue; lower to `rif/relse`, `rwhile`, `rloop`.

M4: Match & patterns (Option/Result)
- Parse `match` with `_`, `Some(x)`, `None`, `Ok(v)`, `Err(e)` patterns; lower to `rmatch` or `rif-let`.

M5: Method calls, field/index sugar
- `obj.method(...)`, `Trait::method(obj, ...)`, `a.b`, `a[i]`; lower to `rdot`, `rget/rset`, `rindex-*`.

M6: Extern and interop literals
- `extern "C" fn …;` to `rextern-fn`. Strings/bytes to `rcstr`/`rbytes` once those helpers are added.

M7: Decls (optional v1)
- Minimal `struct`, `enum`, `trait` stubs parsed and lowered to `rstruct/renum/rtrait` where useful, or ignored if only needed for name resolution sugar.

## Testing strategy

- Unit-style parser tests under `tests/` (Phase 5) or `languages/rustlite/tools`:
  - For each milestone, add positive/negative cases: parse -> lower -> type-check -> (optionally) emit and scan IR for key substrings.
- Golden EDN comparisons for a few representative inputs to make diffs readable.
- Keep tests fast; avoid JIT runs here.

## Integration & build

- Dependencies: PEGTL already added via vcpkg (`pegtl`); include `tao/pegtl.hpp`.
- CMake: create target `rustlite_parser` (OBJECT or STATIC) linked in `languages/rustlite`; add a small test target (gated by EDN_BUILD_TESTS) for parser unit tests.
- CLI (optional): `rustlitec` that reads a `.rl.rs` file and prints EDN to stdout.

## Data structures

- AST nodes: Module, Item(Function, ExternFn), Block, Stmt (Let, ExprStmt, Return, Break, Continue), Expr (Call, Path, Binary, Unary, Field, Index, If, While, Loop, Match, Lit, Assign), Pattern (Wildcard, Bind, Path, TupleLike(path, elems)).
- Types: SimplePath, Builtins, Pointer, Array.

## Open questions / risks

- Type annotations: how strict initially? Proposal: require types at fn params and returns; make local `let` types optional when init is a literal.
- Name resolution: we’ll keep it syntactic; canonicalize paths for sums/traits; semantic checks deferred to EDN type checker.
- Strings and bytes: depend on `rcstr`/`rbytes` helpers; plan to add them early if needed.

## Exit criteria (ready to wire a frontend)

- Parser supports the subset used by existing Rustlite drivers (traits, generics mono demo, extern-fn, panic, rdot, while-let essentials).
- Lowering produces EDN that expands and type-checks with no changes to core.
- Tests: green across Debug/Release on Windows; a handful of golden EDN files.
