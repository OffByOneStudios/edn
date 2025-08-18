# Rustlite Quickstart

Rustlite is a tiny Rust-inspired frontend that lowers to EDN IR. It lives under `languages/rustlite` and can be driven either from the CLI (`rustlitec`) or via the node-based `rustlite::Builder`.

## Build and run tests

```powershell
cmake -S . -B build -DEDN_BUILD_TESTS=ON
cmake --build build --config Release --target rustlitec rustlite_e2e_driver
ctest --test-dir build/languages/rustlite -C Release -R ^rustlite\.
```

## CLI samples

Sample sources live under `languages/rustlite/samples/*.rl.rs`.

Run the CLI on a sample:

```powershell
./build/languages/rustlite/Release/rustlitec.exe languages/rustlite/samples/return.rl.rs
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

```powershell
./build/languages/rustlite/Release/rustlite_e2e_driver.exe languages/rustlite/samples/logical_ops_e2e.rl.rs --dump
```

This prints both the high-level EDN and the final IR.

## Grammar subset (current)
- Items: `fn ident(params) (-> RetTy)? { ... }`
- Blocks: `{ ... }`
- Statements: `let`/`let mut`, `return`, `break`, `continue`, assignment, expression statements (ident, calls)
- Control flow: `if`/`else if`/`else`, `while`, `loop`
- Expressions: integers, identifiers, calls, `!` not, unary `-`, binary ops with precedence, comparisons, `&&`/`||` with short-circuit; parentheses allowed as primaries

## Lowering overview
- Locals: `(rlet ...)` and `(rmut ...)` with `:body [ ]` stubs
- Assign: `(assign %name %val)`
- Return: `(ret <fn-ret-ty> %val)`; bare `return;` casts zero to the function’s declared return type
- Control flow: `(rif ...)`, `(rwhile ...)`, `(rloop ...)`, `(rbreak)`, `(rcontinue)`
- Calls: `(rcall %dst <op-ty or ret-ty> callee args...)` with intrinsic rewrites

See `design/rustlite.md` for background and roadmap.
