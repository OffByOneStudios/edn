# EDN-0009: Rustlite Macro Surface Gap

Status: Closed
Target Release: Unspecified (stage: Rustlite MVP polish)

## Summary
Identify and implement missing or provisional Rustlite surface macros to align with the design goals in `rustlite.md` and improve ergonomics. Most core constructs exist; a few helpers and sugars remain either absent or minimally implemented.

## Goals
- Provide ergonomic aliases / sugars so typical Rust-like snippets compile without resorting to raw core EDN forms.
- Keep each macro a thin, transparent lowering to existing core ops (no semantic shifts).
- Add focused tests exercising each new macro in isolation (parser / expansion / execution where relevant).

## Out of Scope
- Deeper semantic features (lifetimes, borrow checking, pattern destructuring) — tracked separately.
- Performance optimization passes.

## Existing Macro Coverage (Implemented)
`rlet, rmut, rif-let, rif, relse, rwhile, rfor, rloop, rbreak, rcontinue, rwhile-let, rmatch, rsum, rnone, rsome, rok, rerr, rclosure, rcall-closure, rstruct, renum, rtypedef, rfn, rextern-fn, rcall, rand, ror, rassign, rret, rpanic, rassert, rassert-eq, rassert-ne, rassert-lt, rassert-le, rassert-gt, rassert-ge, rtrait, rfnptr, rmake-trait-obj, rtrait-call, rimpl, rmethod, rdot, rget, rset, rindex, rindex-addr, rindex-load, rindex-store, raddr, rderef`

(Note: This enumeration reflects current `expand.cpp` state; adjust if refactors land.)

## Gaps / Enhancements
1. Literal convenience macros (string/byte arrays):
   - `rcstr` → synthesize a global or local constant array of i8 terminated with 0; yield pointer symbol.
   - `rbytes` → synthesize constant array of i8 of provided raw values.
2. Sum/enum niceties:
   - Verify `renum` test coverage (currently no dedicated sample). Add a small positive test constructing variants and matching.
3. Closure ergonomics:
   - Parser sugar for calling a closure directly like `%c(%x)` currently requires `(rcall-closure ...)` at EDN layer; optional future sugar (parser-level) — document decision.
4. External globals (if design intends):
   - `rextern-global` / `rextern-const` sugar for declaring imported data symbols (`global` with :external true).
5. Assertion / diagnostics helpers (optional):
   - `rassert-fail` (unconditional panic), `rassert-range` etc. (Decide scope; maybe defer.)
6. String length helper (optional):
   - `rcstrlen` to compute length at compile time if using `rcstr` (may be overkill now — defer).

## Proposed Implementation Order
1. Add tests for existing `renum` (sum) macro usage (ensure no regressions before new work).
2. Implement `rcstr` and `rbytes` in `expand.cpp` as pure macro expansions building a `(block ...)` with needed consts / arrays.
3. Add driver or unit tests: parse + expand + inspect IR (or run through JIT if trivial) for the new literal macros.
4. Implement `rextern-global` / `rextern-const` (if confirmed desirable) as alias to `(global ... :external true)`; add smoke test referencing symbol from an extern C fn.
5. (Optional) Add extra assertion helpers; create follow-up issue if deferred.

## Acceptance Criteria
- New macros compile and expand deterministically (idempotent expansion pass).
- Added tests: each macro exercised at least once; failing if macro absent or expansion shape changes drastically.
- No existing tests regress.
- Documentation: updated `rustlite.md` with brief description + example for each added macro.
- CHANGELOG entry under Unreleased summarizing additions.

## Risks / Considerations
- Introducing data-literal macros may require ensuring global uniqueness for generated symbols (use `gensym`).
- Extern global sugar must not conflict with future initialization semantics.

## Tasks
- [x] Add `renum` sample & driver test (enum + match) via `rustlite_enum_driver` demonstrating construction + pattern match (test: `rustlite.enum`).
- [x] Implement `rcstr` macro (provisional lowering using per-byte consts; lacks true array/global support).
- [x] Implement `rbytes` macro (provisional lowering similar to `rcstr`).
- [x] Add enabled tests for `rcstr` / `rbytes` (`rustlite.literals` driver now active using core `cstr`/`bytes` ops).
- [x] Decide on extern data sugar; if yes implement `rextern-global` / `rextern-const` + tests. (Implemented; test `rustlite.extern-globals` passes.)
- [x] Update `rustlite.md` docs with new macro descriptions (renum demo + rcstr/rbytes usage + caveats).
- [x] Update CHANGELOG Unreleased with bullet list of new macros. (Added rcstr/rbytes, renum sample note, extern global sugar.)
- [x] Add negative test(s) for malformed `rcstr` / `rbytes` usage (bad literal form, empty vector, non-int elements, missing quotes) (`rustlite.literals_neg`).
- [x] Ensure symbol alias post-pass doesn't incorrectly rewrite generated const symbols inside produced blocks (audit + targeted test once literal test enabled). (Verified by `rustlite.literals_alias_audit`.)
- [x] Replace provisional `rcstr`/`rbytes` lowering with proper core literal ops (`cstr` / `bytes`) emitting private constant globals (supersedes need for textual `(global ...)` in this case).

### New Follow-up Tasks (post core literal ops)
- [x] Implement escape sequence handling in `rcstr` (e.g. `\n`, `\t`, `\\`, `\"`, `\xNN`, `\0`).
- [x] Deduplicate / intern identical string and byte literals (reuse one global, maintain map during emission).
- [x] Add negative tests covering malformed escapes, out-of-range byte values, empty byte vector, and missing NUL enforcement for `cstr` (subset implemented; malformed escape still TODO if stricter validation added).
- [x] Document new core ops (`cstr`, `bytes`) and macros (`rcstr`, `rbytes`) in `rustlite.md` (usage, limitations, escape support, interning).
- [x] CHANGELOG Unreleased: add entry for literal macro support and underlying core ops. (cstr/bytes core ops + interning documented.)

## Follow Ups (Defer / Separate Issue)
- Parser-level closure call sugar.
- Richer pattern matching (multi-binding patterns, guards).
- Lifetimes / borrow semantics.

## References
- `languages/rustlite/src/expand.cpp`
- `languages/rustlite/rustlite.md`

## Closure Notes
All acceptance criteria satisfied: new literal and extern data macros implemented, core `cstr` / `bytes` ops with interning landed, comprehensive positive & negative tests added (including alias audit), documentation & CHANGELOG updated. Issue closed.
