# EDN IR Emitter Architecture

This directory contains a modularized version of the IR emitter that previously lived in a single `src/edn.cpp` file. The goals are:

- Strong separation of concerns (types, collection, control flow, EH, sums, DI, etc.).
- Clear documentation for each path the compiler walks.
- Safer DataLayout usage (only on sized types) and deterministic ordering.
- Easier testing and future refactors (Darwin/Linux/Windows specifics isolated).

Key modules:
- context.[hpp|cpp]: Orchestration of emission, environment flags, LLVM context/module.
- types.[hpp|cpp]: Type mapping from EDN types to LLVM types; struct caches; utilities.
- collect.[hpp|cpp]: Reader-macro expansion entry, prepass to register structs/unions/sums/globals. Ensures struct bodies are set before DataLayout queries.
- core_ops.cpp: Scalars, arithmetic/bitwise, ptr math.
- memory.cpp: addrs/deref/load/store/globals/allocas; var slots.
- control.cpp: if/while/for/switch/match scaffolding and PHIs.
- sums.cpp: sum-new/sum-is/sum-get, match bindings; uses a single SumLayout helper.
- eh.cpp: Itanium and SEH lowering, panic paths; common gating by environment variables.
- coro.cpp: Coroutine intrinsics and attributes when enabled.
- di.cpp: Debug info (types, locals, struct members) guarded by EDN_ENABLE_DEBUG.
- passes.cpp: Pass pipeline configuration; verifier hooks.

Conventions:
- Never call DataLayout on opaque/unsized types; resolve struct bodies first in collect.
- All control-flow blocks should end with a terminator; helper functions return both value and insertion block where needed.
- Sums are represented as `{ i32 tag, [N x i8] payload }` with N = max payload size among variants on the target DL.
- Windows SEH funclets use three-argument catchpad; Itanium uses landingpad with catch-all for our simple try/catch.
- Panic=abort uses llvm.trap; panic=unwind uses __cxa_throw (Itanium) or RaiseException (SEH).

Testing:
- Phase 4 tests exercise sums, match, EH, panic, DI, and pass pipelines. Use env helpers in `tests/test_env.hpp` for platform differences.
