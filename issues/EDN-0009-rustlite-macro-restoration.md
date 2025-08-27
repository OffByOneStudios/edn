# EDN-0009: Rustlite Macro Restoration & Phantom Source Anomaly

## Summary
`languages/rustlite/src/expand.cpp` was intentionally slimmed to a minimal stub for maintainability. During and after this reduction we observed the compiler (Ninja/CMake build) apparently compiling a "phantom" prior version of the file: build errors referenced symbols and line numbers **not present** in the on‑disk stub. We halted functional macro development to (a) preserve a reproducible diagnostic trail and (b) safely recover the full macro lowering layer from history.

This ticket enumerates what existed at the "complex lowering" baseline commit, what actions have occurred since, what was lost, confirmed findings about the phantom source behavior, and the proposed recovery / hardening plan.

## Reference Commit ("Complex Lowering")
Assumed commit: `70c05fc` (contains: full macro suite + post-pass alias rewrite of initializer consts → variable symbols).

Key elements present in that commit:
* Helper constructors: `make_sym`, `make_kw`, `make_i64`, `gensym`
* Macro registrations (`tx.add_macro`) for (non-exhaustive, recovered complete list):
  - Variable & binding forms: `rlet`, `rmut`, `ras`, `rassign`
  - Control flow: `rif`, `relse`, `rif-let`, `rwhile`, `rwhile-let`, `rloop`, `rfor`, `rbreak`, `rcontinue`, `rmatch` (pattern + cases sugar)
  - Sum / option / result constructors: `rsum`, `rnone`, `rsome`, `rok`, `rerr`
  - Closure forms: `rclosure`, `rcall-closure`
  - Types / declarations: `rstruct`, `renum`, `rtypedef`, `rfn`, `rextern-fn`, `rtrait`, `rfnptr`
  - Trait / dynamic dispatch: `rmake-trait-obj`, `rtrait-call`, `rimpl`, `rmethod`, `rdot`
  - Struct & array sugar: `rget`, `rset`, `rindex-addr`, `rindex`, `rindex-load`, `rindex-store`
  - Pointer ops: `raddr`, `rderef`
  - Calls & intrinsics: `rcall` (with intrinsic rewriting: `add, sub, mul, div, eq, ne, lt, le, gt, ge, not`)
  - Boolean short-circuit: `rand`, `ror`
  - Assertions & panic: `rpanic`, `rassert`, `rassert-eq`, `rassert-ne`, `rassert-lt`, `rassert-le`, `rassert-gt`, `rassert-ge`
* Post-pass symbol remapping: rewrites uses of temporary const initializer symbols back to the declared variable symbol (environment tracking across vectors / lists).

## Actions Performed Since That Commit
1. Replaced `expand.cpp` with a minimal stub returning the input AST unchanged.
2. Added / removed instrumentation: `#error` sentinels, numbered comment padders to force unmistakable diagnostics.
3. Observed intermittent compiler diagnostics referencing:
   * Line numbers far exceeding stub length.
   * Symbols (`tx.add_macro`, helper creators) absent from the current file.
4. Verified only one physical `languages/rustlite/src/expand.cpp` in repository; no alternative path in `CMakeLists.txt` (explicit source list, no globbing, no generation step).
5. Deleted stale build artifacts; attempted rebuilds (plain, reconfigure, renamed source file, substitution with alternative filename) — phantom content persisted intermittently.
6. Retrieved historical file content from commit `70c05fc` (full macro implementation recovered and archived in session output; not yet reinstated on disk).
7. Confirmed repo-wide `grep` for certain phantom tokens returned none while errors still mentioned them (supporting mismatch between file opened by compiler and working tree file content at compile time).

## Current State
* Live `expand.cpp`: minimal pass-through (macros unavailable; dependent higher-level Rustlite surface forms currently non-functional).
* Historical macro implementation: extracted (needs clean re-application once phantom anomaly is addressed).
* Tests depending on macro sugar likely failing or exercising reduced behavior (not yet re-run post-truncation in an isolated clean build directory).

## Impact
* Loss of macro sugar blocks language feature progression (control flow, pattern matching, trait usage, structured aggregate helpers).
* Risk of misdiagnosis persists until we prove the compiler consumes the on-disk file bytes deterministically.
* Potential underlying causes (enumerated; none yet confirmed):
  - Stale or shadow object compilation unit via alternate build dir / stale dep graph.
  - Editor / tooling overlay (e.g., generated or cached file served to compiler wrapper path).
  - Accidental include of a different translation unit pre-rename (unlikely given explicit source list).
  - Filesystem race or caching anomaly (less likely but not ruled out).

## Recovery & Hardening Plan (Proposed)
Priority tiers with acceptance criteria.

### Tier 0: Safety / Repro Isolation
1. Create brand new build directory (`build_phantom_isolation/`), run a full configure & build; confirm stub diagnostics (no phantom macros) — capture compile command line.
2. Replace current `expand.cpp` content with a single `#error EXPAND_SENTINEL` line; rebuild expected failure; verify emitted diagnostic shows sentinel line 1.
3. If mismatch still occurs, wrap the C++ compiler via a shim script that:
   * Logs argv.
   * Hashes the file path argument (`sha256`) and records inode + size before invoking real compiler.
   * Stores log under `diagnostics/compile_log.txt`.
4. Analyze log: ensure only expected path is compiled; compare file hash to working tree hash.

### Tier 1: Restoration
5. Once isolation proves deterministic consumption, checkout historical `expand.cpp` from `70c05fc` into current working tree (preserve original as `expand_min_stub.cpp` if desired for diff clarity).
6. Build & run tests; capture failing tests referring to macro layer and triage.
7. Add lightweight checksum banner comment at top of file: includes short hash of body; CI step (future) can verify drift vs committed hash.

### Tier 2: Incremental Improvement / Cleanup
8. Segment macro definitions into thematic static helper functions or nested scopes to reduce single-function sprawl (optional refactor for readability — defer until green).
9. Add targeted unit tests for:
   * Control flow (rif / rif-let / rwhile / rand / ror)
   * Pattern matching (rmatch arms + else)
   * Data structures (rstruct / renum + sum constructors)
   * Trait dispatch (rimpl + rdot variants)
10. Add fuzz / randomized symbol collision test for `gensym` uniqueness across expansions.

### Tier 3: Post-Pass Robustness
11. Expand alias post-pass to capture transitive chains (currently only single mapping observed) and verify against nested block scopes.
12. Performance profiling: measure expansion time on representative large module to ensure O(n) traversal remains.

## Open Questions
* Exact commit hash for "complex lowering"—assumed `70c05fc`; confirm via git log annotation.
* Were any macros added after `70c05fc` but before truncation? (Need diff of file across subsequent commits to verify no lost deltas.)
* Which tests explicitly exercise macro sugar today vs relying on desugared core forms?

## Required Artifacts To Generate (Future)
* `diagnostics/` directory with compile shim logs (Tier 0 Step 3).
* Macro layer unit test sources under `tests/` (Tier 2 Step 9).
* CI script snippet verifying checksum banner (Tier 1 Step 7).

## Definition of Done
1. Rebuilt from clean directory with restored macro file — no phantom content mismatch.
2. All pre-existing tests pass or are updated to reflect intended macro semantics.
3. New targeted macro unit tests pass (minimum: rif, rwhile, rmatch, rimpl, rand/ror, rindex + rset/rget).
4. Logged evidence (hash + inode) proving compiler consumed restored source bytes.
5. Ticket updated with commit hash containing restoration.

---
Prepared: (auto-generated) — adjust if the reference commit differs.
