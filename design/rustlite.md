# Rustlite – a tiny Rust-like prototype on EDN (Phase 5)

Status: In progress (2025-08-17)

Rustlite is a minimal Rust-inspired surface that lowers to EDN IR using EDN’s macro/transform system. It validates Phase 4/5 platform features: sum types + match (including result-mode), simple locals, and core pass/verification toggles.

This doc records scope, lowerings, and how to run the demo so it’s easy to pick up work on another machine.

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

## Demo driver
Module contents:
- Sum type: `OptionI32` with `None` and `Some(i32)`
- `use_option`: core `match` form
- `use_option_rif`: `rif-let` result-mode match
- `let_demo`: `rlet` binding
- `mut_demo`: `rmut` binding + `(assign ...)`

Files
- Expander: `languages/rustlite/src/expand.cpp`
- Driver: `languages/rustlite/tools/rustlite_driver.cpp`
- Headers: `languages/rustlite/include/rustlite/*.hpp`

Driver pipeline
- Build EDN text via `rustlite::Builder`, parse to AST
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
- rlet/rmut lowering to `block :body` with typed `as` binding
- Emitter supports `block :body`
- `rif-let` lowering; driver PHI validator for `undef` incomings

Next
- Optional `:locals` materialization for stack lifetime
- Reuse/add generics/traits sugar for a richer Rust subset
- Phase 5 tests for rlet/rmut and rif-let

## Troubleshooting
- `EGEN: instruction must be list`: a macro likely returned a raw vector; wrap in `(block :body [ ... ])`.
- If `let_demo`/`mut_demo` return `0`, ensure the emitter handles `block` and the `ret` is outside the block.
