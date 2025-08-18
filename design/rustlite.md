# Rustlite – a tiny Rust-like prototype on EDN (Phase 5)

Status: In progress (2025-08-18)

Rustlite is a minimal Rust-inspired surface that lowers to EDN IR using EDN’s macro/transform system. It validates Phase 4/5 platform features: sum types + match (including result-mode), simple locals, and core pass/verification toggles.

This doc records scope, lowerings, and how to run the demo so it’s easy to pick up work on another machine.

## Rustlite frontend (PEGTL) parser status

We added a tiny Rust-like parser and CLI under `languages/rustlite/` to drive end-to-end smoke via source text. This is intentionally small and independent of the macro expander.

Locations
- Parser: `languages/rustlite/parser/parser.cpp`
- CLI: `languages/rustlite/tools/rustlitec.cpp`
- Samples: `languages/rustlite/samples/*.rl.rs`

Implemented grammar subset
- Items: `fn ident(params) (-> RetTy)? { ... }`
- Blocks: `{ ... }`, including nested blocks as standalone statements
- Statements
  - let: `let x = 1;` and `let x: i32 = (1);` (RHS supports general expressions)
  - return: `return;`, `return 123;`
  - break, continue
  - assignments: `x = <expr>;`
  - expression statements: signed ints, identifiers, calls (empty or with args)
- Control flow: `if`/`else`/`else if`, `while`, `loop`
- Booleans and comparisons yield `i1`; `&&`/`||` short-circuit via `rif` and temporaries
- Parenthesized expressions as primaries: `(1)`, `(x)`, `(foo())` usable in conditions, RHS of let/assign, and call args

CLI test coverage (all passing as of 2025-08-18)
- rustlite.cli_smoke → `empty_main.rl.rs`
- rustlite.cli_return → `return.rl.rs`
- rustlite.cli_let → `let_stmt.rl.rs`
- rustlite.cli_let_typed → `let_typed.rl.rs`
- rustlite.cli_unary_minus → `unary_minus.rl.rs`
- rustlite.cli_ident_stmt → `ident_stmt.rl.rs`
- rustlite.cli_empty_call → `empty_call.rl.rs`
- rustlite.cli_call_args → `call_args.rl.rs`
- rustlite.cli_break_continue → `break_continue.rl.rs`
- rustlite.cli_if_stmt → `if_stmt.rl.rs`
- rustlite.cli_if_else → `if_else.rl.rs`
- rustlite.cli_else_if_chain → `else_if_chain.rl.rs`
- rustlite.cli_while_stmt → `while_stmt.rl.rs`
- rustlite.cli_loop_stmt → `loop_stmt.rl.rs`
- rustlite.cli_nested_blocks → `nested_blocks.rl.rs`
- rustlite.cli_assign_stmt → `assign_stmt.rl.rs`
- rustlite.cli_paren_exprs → `paren_exprs.rl.rs`
- rustlite.cli_logical_ops → `logical_ops.rl.rs`
- rustlite.cli_unary_not → `unary_not.rl.rs`
- rustlite.cli_mixed_precedence → `mixed_precedence.rl.rs`
- rustlite.cli_fn_params → `fn_params.rl.rs`
- rustlite.cli_let_mut → `let_mut.rl.rs`

## Phase 5 implementation placement rules
- If it is a multi-language common macro function or intrinsic, implement it in the main EDN library (core), not in a specific language folder.
- If it is a single-language feature that can be a macro function, implement it in that language’s folder (e.g., `languages/<lang>/...`).
- If the feature cannot be implemented as a macro and is specific to the language, as a last resort implement it as an intrinsic in that language layer (do not promote to core).

Guidance: Prefer macros over intrinsics. If a language-specific macro later proves generally useful, promote it to the main library and keep a thin compatibility shim in the language layer.

## Goals
- Exercise ADTs (Rust enums with payload) via EDN sum types and `match`.
- Provide tiny sugar for locals: immutable `let` and mutable `mut`.
- Provide a result-oriented match helper (`rif-let`) that binds a payload and yields a value.
- Stay macro-driven (no parser) to focus on IR mappings.

Non-goals (now)
- Borrow checker/lifetimes/ownership beyond EDN’s typing.
- Full generics/traits surface (we can reuse Phase 4 expanders when needed).

## Surface forms and lowerings
Rustlite uses EDN list forms with an expander in `languages/rustlite/src/expand.cpp`.

- rlet: immutable binding
  Form: `(rlet <type> %name %init :body [ ... ])`
  Lowers to: `(block :body [ (as %name <type> %init) ...body... ])`

- rmut: mutable binding (mutation via SSA aliasing)
  Form: `(rmut <type> %name %init :body [ ... ])`
  Lowers to: `(block :body [ (as %name <type> %init) ...body... ])`
  Mutate by emitting `(assign %name %new)` inside the block/body later.

- rif-let: result-mode match with bind
  Form: `(rif-let %dst <ret-type> SumType Variant %ptr :bind %x :then [ ... :value %then ] :else [ ... :value %else ])`
  Lowers to: a `(match ...)` with one `case` and a `default`, using `:value %sym` markers. The checker extracts these to build a PHI with well-typed incomings.

- block: expander emits `(block :body [ ... ])`. The emitter walks `:body`. `:locals` is reserved for future stack materialization.

Correctness notes
- Result-mode `match` must not yield PHIs with `undef` incoming values. The driver scans LLVM IR and errors if found.
- Macros always return a single list-form instruction (never a raw vector), so the checker sees valid instruction nodes.
 - Short-circuit `rand`/`ror` must not redefine the destination SSA name in branches. Define the destination once with `as` before the branch and use `assign` inside the `then`/`else` blocks.

## Demo driver
Module contents:
- Sum type: `OptionI32` with `None` and `Some(i32)`
- `use_option`: core `match` form
- `use_option_rif`: `rif-let` result-mode match
- `let_demo`: `rlet` binding
- `mut_demo`: `rmut` binding + `(assign ...)`

Additional drivers (regressions and focused coverage):
- `rustlite_logicdriver` – validates `rand`/`ror` short-circuit semantics and asserts results.
- `rustlite_rwhilelet_driver` – exercises `rwhile-let` lowering in a simple single-iteration loop.
- `rustlite_rdot_driver` – validates `rdot` in trait and free-function forms; checks IR for indirect and direct calls.
- `rustlite_rdot_negdriver` – negative smoke for `rdot` (unknown method and bad arity).

Files
- Expander: `languages/rustlite/src/expand.cpp`
- Driver: `languages/rustlite/tools/rustlite_driver.cpp`
- Headers: `languages/rustlite/include/rustlite/*.hpp`

Driver pipeline
- Build EDN as nodes via `rustlite::Builder` (uses `edn::node_list` and operator<<), serialize to text
- Run trait/generic expanders (if used), then rustlite expansion
- Type check, emit LLVM IR, verify no PHI has `undef` incoming
- `--dump` prints high-level EDN and lowered IR

Environment flags (from EDN core)
- `EDN_ENABLE_PASSES=0|1` – enable LLVM pass pipeline
- `EDN_OPT_LEVEL=0|1|2|3` – O0..O3 presets (used when passes enabled)
- `EDN_PASS_PIPELINE` – custom textual pipeline (overrides presets)
- `EDN_VERIFY_IR=0|1` – IR verifier
- `EDN_ENABLE_EH=0|1`, `EDN_PANIC=abort|unwind` – exception/panic model
- `EDN_TARGET_TRIPLE=...` – cross-compile target triple

## Examples (conceptual)
- Let/Mut
  `[ (const %init i32 41) (rlet i32 %a %init :body [ ]) (ret i32 %a) ]`
  `[ (const %init i32 1) (rmut i32 %b %init :body [ (const %one i32 1) (add %tmp i32 %b %one) (assign %b %tmp) ]) (ret i32 %b) ]`

- Result-mode match
  `[ (rif-let %ret i32 OptionI32 Some %p :bind %x :then [ :value %x ] :else [ (const %z i32 0) :value %z ]) (ret i32 %ret) ]`

## Status and next steps
Done
- Field/index/pointer sugar demo and negative diagnostics.
- Trait-object IR-shape demo.
- “Generics” monomorphization demo.
- Extern "C" call smoke.
- Panic-model toggle driver (abort/unwind) and tests.
- Fix for `rand`/`ror` SSA redefinition; added logic regression test.
- Comprehensive smoke passing.

Next
- `EGEN: instruction must be list`: a macro likely returned a raw vector; wrap in `(block :body [ ... ])`.
- If `let_demo`/`mut_demo` return `0`, ensure the emitter handles `block` and the `ret` is outside the block.

## Macro feature set (pre-frontend)
This is the minimum Rustlite macro surface we should implement before investing in a full frontend. Each item lists intent, EDN anchor ops, and a sketch of desugaring. Priorities: M = must-have, S = should-have, L = later.

- Locals and blocks [M]
  - rlet/rmut: already implemented; block-scoped typed binding via `as` into `(block :body [...])`.
  - rassign: mutation sugar inside a block.
    - Form: `(rassign %name %value)` → `(as %name <ty-of-%name> %value)` or dedicated `(assign ...)` if present.

- Conditionals and loops [M]
  - rif/relse: simple if/else sugar over core `(if %cond [..then..] [..else..])`.
  - rloop/rbreak/rcontinue: sugar over core `(for ...)` or `(loop ...)`/`(break)`/`(continue)` if present; otherwise emulate `loop { .. }` with `for`’s `:cond %true`.
  - rwhile: `(rwhile %cond :body [ ... ])` → core `(while %cond [ ... ])`.
  - rfor: minimal counted/iter-like loop → core `(for :init [...] :cond %c :step [...] :body [...])`.

- Pattern matching and ADTs [M]
  - rmatch: multi-arm sugar over core `(match ...)` with `:value` result-mode, ensuring no PHI `undef` incomings.
  - rif-let: already implemented option-like bind-and-yield helper.
  - Option/Result helpers: `rsome`, `rnone`, `rok`, `rerr` to build common sums; `rwhile-let` (S) as loop + match sugar.

- Functions and calls [M]
  - rfn: function definition sugar → core `(fn :name ... :ret ... :params [...] :body [...])` with optional attributes.
  - rcall: positional call sugar → core `(call %dst RetTy callee [args...])` (or existing call form).
  - rextern-fn: extern decl sugar → `(fn ... :external true)` and optional calling-conv attrs.
  - rdot: method-call sugar
    - Trait call: `(rdot %dst RetTy Trait %obj method arg...)` → `trait-call`.
    - Trait dot-path: `(rdot %dst RetTy Trait.method %obj arg...)` → split to `Trait` and `method`, then `trait-call`.
    - Free fn with receiver: `(rdot %dst RetTy callee %obj arg...)` → `call` with receiver first.

- Closures [S]
  - rclosure: lambda literal with capture list.
    - Form: `(rclosure (params [...]) (captures [...]) (ret Ty) :body [...])` → `make-closure` + synthesized callee using env-first calling convention.
  - rcall-closure: sugar → core `call-closure`.

- Types and data [S]
  - rstruct: structural type + ctor/accessor sugar → core `struct` + `get`/`set` ops.
  - renum: sum type definition sugar → core sum type + `sum-new`/`sum-get`.
  - rtypedef: type alias → core `typedef`.

- Traits and impls [S]
  - rtrait/rimpl: dictionary/vtable construction sugar mapping to Phase 4 trait machinery.
  - rmethod/rdot: method-call sugar resolving to direct or trait-object calls.

- Operators and logic [M]
  - Arithmetic/comparison: infix-y sugar mapping to existing EDN ops (add/sub/mul, eq/ne/lt/le/gt/ge).
  - Short-circuit `and`/`or`: `rand`/`ror` desugar via `if` to preserve short-circuit semantics.

- Error handling [M]
  - rpanic: → core `panic`; configurable behavior via env (`EDN_PANIC`).
  - rassert[_eq/_ne/...]: emit branch and `rpanic` on failure.

- Interop [S]
  - rextern-const/global: extern global/const sugar if needed.
  - rcstr/rbytes: literal helpers to build host-compatible data when interoping.

Acceptance criteria
- Each macro expands to valid core EDN forms the checker already supports (type-correct; terminator rules respected).
- Result-mode `rmatch`/`rif-let` expansions never yield PHIs with `undef` incoming; validated by the driver’s IR scan.
- Minimal tests for each macro: expand → type-check → emit; run where applicable; cover 1-2 edge cases (empty arms, mismatched types, early `break`).

Initial implementation order
1) rif/relse, rloop/rbreak/rcontinue, rwhile, rfor
2) rmatch, Option/Result helpers
3) rfn/rcall/rextern-fn
4) rclosure/rcall-closure
5) rstruct/renum/rtypedef
6) rtrait/rimpl/rmethod

## Getting started on a new machine (Windows)

Prerequisites
- Visual Studio 2022 (Desktop development with C++)
- CMake 3.20+ available on PATH
- vcpkg installed; set the `VCPKG_ROOT` environment variable

Configure and build (uses CMakePresets)

```powershell
cmake --preset msbuild
cmake --build build --config Debug
```

Quick verify

```powershell
# Core Rustlite smoke
ctest --test-dir build -C Debug -R ^rustlite\.smoke$ -V

# Parser CLI suite
ctest --test-dir build -C Debug -R ^rustlite\.cli_ -j6 --output-on-failure

# Run CLI manually on a sample
./build/languages/rustlite/Debug/rustlitec.exe languages/rustlite/samples/empty_main.rl.rs
```

Notes
- vcpkg dependencies (LLVM, PEGTL, fmt, cxxopts) are pulled automatically via the VCPKG toolchain.
- Visual Studio solution is generated at `build/edn.sln` if you prefer working in the IDE.

### Workflow: add a small parser feature and test
1) Edit grammar in `languages/rustlite/parser/parser.cpp`.
2) Add a sample in `languages/rustlite/samples/<name>.rl.rs` (keep it minimal; one-liners are ideal).
3) Register a test in `languages/rustlite/CMakeLists.txt`:
  `add_test(NAME rustlite.cli_<name> COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/<name>.rl.rs)`
4) Build the CLI:

```powershell
cmake --build build --config Debug --target rustlitec
```

5) Run just your test:

```powershell
ctest --test-dir build -C Debug -R rustlite\.cli_<name> --output-on-failure
```

6) Run the whole CLI suite to catch regressions:

```powershell
ctest --test-dir build -C Debug -R ^rustlite\.cli_ -j6 --output-on-failure
```

Troubleshooting
- PEGTL on MSVC: use fully-qualified `tao::pegtl::...` (local aliases previously confused MSVC name lookup).
- Forward-declare grammar rules that reference each other (e.g., `struct block_rule;`).
- Let RHS must accept general expressions (we route to `cond_expr`) so `(1)`, idents, and calls parse.
- Keep `assign_stmt` before `expr_stmt` in `sor<>` to avoid ambiguity on `ident = ...` vs `ident;`.
- CLI diagnostics: `rustlitec` prints `filename:line:column` on `parse_error`—use this to jump straight to the failure.

Suggested next parser steps
- Function signatures: parameters and optional `-> RetTy` (syntax only for now).
- Binary operators with precedence (start with `* /` > `+ -`, left-assoc).
- Booleans `true`/`false` and basic comparisons for `if` conditions.
- Strings (quoted) to enable simple I/O demos.
- Basic recovery across `;`/`}` to improve multi-error reporting.
