# Coroutines (experimental)

Status: experimental, gated behind the EDN_ENABLE_CORO environment flag.

This project emits minimal LLVM coroutine intrinsics for switched-resume coroutines. The goal is to generate valid IR that the LLVM coroutine passes can lower, while keeping the surface area small and testable.

## Emission summary

When EDN_ENABLE_CORO=1, the following IR forms are emitted:

- Begin:
  - token %id = call token @llvm.coro.id(i32 0, ptr null, ptr null, ptr null)
  - ptr %h = call ptr @llvm.coro.begin(token %id, ptr null)

- Save (optional distinct savepoint):
  - token %tok = call token @llvm.coro.save(ptr %h)

- Suspend (non-final):
  - i8 %st = call i8 @llvm.coro.suspend(token %tok_or_none, i1 false)

- Final suspend:
  - i8 %st2 = call i8 @llvm.coro.suspend(token %tok_or_none, i1 true)

- Promise pointer:
  - ptr %p = call ptr @llvm.coro.promise(ptr %h, i32 0, i1 false)

- Control helpers:
  - call void @llvm.coro.resume(ptr %h)
  - call void @llvm.coro.destroy(ptr %h)
  - i1 %dn = call i1 @llvm.coro.done(ptr %h)

- End (non-unwind path):
  - %unused = call i1 @llvm.coro.end(ptr %h, i1 false, token none)

Notes:
- The coroutine handle is the pointer returned by coro.begin.
- For suspend, you may pass a token from llvm.coro.save or use token none (the emitter accepts either).
- Final suspend uses the same intrinsic with the isFinal flag set to true.
- The type checker currently tracks tokens with an i8 placeholder for SSA bookkeeping; the LLVM IR uses proper token types.

### Minimal example

```
(module :id "m_coro"
  (fn :name "c" :ret i32 :params [] :body [
    (coro-begin %h)
    (coro-save %tok %h)
    (coro-promise %p %h)
  (coro-size %sz)
  (coro-alloc %need %cid)
    (coro-suspend %st %tok)
    (coro-final-suspend %fst %tok)
    (coro-resume %h)
    (coro-destroy %h)
  (coro-free %mem %cid %h)
    (coro-done %dn %h)
    (coro-end %h)
    (const %z i32 0)
    (ret i32 %z)
  ]))
```

## Tests

The golden IR test `tests/phase4_coro_ir_golden_test.cpp` includes:
- Minimal begin/suspend/end sequence using token none for suspend.
- A variant that saves a token with llvm.coro.save, gets a promise pointer, performs a final suspend, resumes/destroys, checks done, and ends.

## Lowering and passes

These intrinsics represent the switched-resume coroutine form. To execute coroutines, LLVM’s coroutine passes (e.g., coro-early, coro-split, coro-cleanup) must be run to lower intrinsics into regular control flow and helpers. In this repository, tests focus on IR shape (golden/smoke). Execution/JIT can be added once a pass pipeline that includes coroutine lowering is enabled.

### ABI and attributes

EDN uses the Switch-Resumed ABI. LLVM’s CoroEarly pass requires coroutines to be marked as "presplit" to select this ABI. The emitter tags every function when `EDN_ENABLE_CORO=1` with:

- the built-in function attribute kind: `PresplitCoroutine`
- the string attribute: `"presplitcoroutine"`

This ensures `F.isPresplitCoroutine()` is true in LLVM 18+, avoiding CoroEarly assertions and enabling the expected lowering path.

### Pass pipeline

The smoke test drives the new pass manager via `PassBuilder::parsePassPipeline` with:

- `coro-early,coro-split,coro-cleanup`

`coro-elide` is optional for this minimal setup and isn’t required for the smoke.

You can run the tests on Windows PowerShell with:

```pwsh
# Build (choose Debug or Release to match your workflow)
cmake --build build --config Debug

# Run all tests (includes the coroutine IR goldens and the lowering smoke test)
& "build\tests\Debug\edn_tests.exe"
```

## Next steps

- Consider exposing additional manipulation intrinsics (resume/destroy/done/promise) if needed by frontends.
- Explore building a tiny runtime shim and a JIT smoke test once lowering passes are integrated into examples.

### Follow-up demo: safe coroutine invocation (planned)

Goal: run a lowered coroutine safely (no null-based frame stores) by allocating a frame before `coro.begin` and cleaning it up.

Planned approach:
- Emit size/allocation sequence prior to lowering:
  - `(coro-id %cid)` and `(coro-size %sz)`; choose stack (`alloca i8 %sz`) or heap (`extern malloc` then `(call %mem (ptr i8) malloc %sz)`).
  - If using heap, record `%need` via `(coro-alloc %need %cid)` and later `(coro-free %mem %cid %h)` to allow `coro.clean` paths to free.
- Plumb the allocated pointer into `coro.begin` so the lowered IR writes to a valid frame base rather than `null`.
  - Today, the emitter always passes `null` as the memory pointer to `llvm.coro.begin`. The follow-up will extend EDN to accept an optional memory argument, e.g. `(coro-begin %h %mem)`, and wire it to `coro.begin`’s second parameter.
- Run passes: `coro-early,coro-split,coro-cleanup` (and optionally `coro-elide`).
- JIT and call the function; if heap-alloc was used, ensure destroy/free paths run; verify it returns the expected value.

Notes:
- Without passing a non-null memory pointer (or running an elision strategy that replaces it), lowered code may contain GEP/stores off `null`. That’s why the current JIT smoke is lookup-only.
- This demo remains optional and will land once the `(coro-begin %h %mem)` form is added.
