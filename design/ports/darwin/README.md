# Darwin (macOS arm64) Port Plan

Status: Not building on Darwin arm64. Goal is to reach feature parity with current Phase 5 (Windows baseline) without regressing Windows. Linux will follow after macOS is green.

This plan is organized by phases with concrete, verifiable, small steps. Each step includes: scope, acceptance criteria, platform notes, CMake impacts, and tests to flip on. We keep Windows CI green at every step and gate macOS work behind feature flags or targeted conditionals.

## Global strategy
- Keep the Windows feature set intact. Prefer additive changes and platform probes over wide refactors.
- Validate in narrow slices: get core library `edn` building, then examples, then tests, then language drivers.
- Use CMake presets and component detection to select the right LLVM libs for `APPLE && arm64`.
- Land per‑phase compilation targets to allow independent builds: `tests/phase{N}_*` should compile even if later phases are skipped.
- Add a macOS AArch64 job to CI last; iterate locally first.

## Environment assumptions
- Toolchain: Apple Clang with LLVM from vcpkg (LLVM 18+).
- Target triple: `arm64-apple-macosx` (Darwin). Itanium EH model.
- JIT: ORC v2 supported on Apple Silicon.

## CMake and build system adjustments (immediate)
1) LLVM components: ensure `AArch64` backend is linked on Apple arm64, X86 otherwise. Keep Support/Core/IRReader/OrcJit.
   - Current CMake does this via:
     - `if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64") list(APPEND LLVM_COMPONENTS AArch64)`
   - Verify: `llvm_map_components_to_libnames(llvm_libs ${LLVM_COMPONENTS})` resolves on macOS.
2) Header search: honor `${LLVM_INCLUDE_DIRS}` only if LLVM is found.
3) fmt/cxxopts/pegtl linkage: already guarded with `if(TARGET ...)`. Keep.
4) Split examples/tests by phase targets already present. Ensure each compiles on macOS with minimal stubs when advanced features are gated off.
5) Add an option: `EDN_PORT_DARWIN=ON` to enable Darwin-specific workarounds.

Tracking issues to open if needed:
- Toolchain flags for coroutine & EH intrinsics on Apple Clang.
- Sanitizers mismatches (if used) on macOS.

---

# Phase-by-Phase bring-up

Each phase has:
- Objective: what we want compiling and running
- Work items: code/CMake edits
- Tests: which `tests/` files to enable/expect green
- Acceptance: compile/run criteria
- Windows safety: how we avoid regressions

## Phase 1 – Core IR parse/typecheck/emit skeleton
Objective
- Build `edn` static lib and minimal IR emission to a Module; no JIT yet.

Work items
- Verify `src/edn.cpp`, `src/type_check.cpp`, `src/diagnostics_json.cpp` compile with Apple Clang.
- Fix any MSVC-specific code: replace `_stricmp`, `_umul128`, `_BitScanForward` etc. with portable equivalents or `#ifdef _MSC_VER` guards.
- Ensure no Windows‑only headers/pragma are included.

Tests
- Enable: `tests/core_main.cpp`, `tests/diagnostics_test.cpp`, early phase tests under `tests/phase2_*` that don’t require JIT.

Acceptance
- `ctest -R core|diagnostics` green on macOS.

Windows safety
- Use `#if defined(_MSC_VER)` blocks; no behavior change on Windows.

## Phase 2 – Unsigned, floats, casts, comparisons
Objective
- Type checker and emitter for unsigned ints, float ops, casts build on macOS; IR verifies.

Work items
- Audit any uses of `size_t` → 64‑bit assumptions; Darwin arm64 pointers are 64‑bit.
- Ensure intrinsic and predicate enums map correctly with Apple LLVM 18.
- Add portable helpers for bit width queries; avoid compiler intrinsics.

Tests
- Enable: `tests/cast_test.cpp`, `tests/phase2_main.cpp`, float and icmp tests.

Acceptance
- All Phase 2 tests pass on macOS; IR verifies via LLVM verifier.

Windows safety
- All code is portable; no platform branches expected.

## Phase 3 – C-like semantics (ptr arith, enums/unions, loops, switch, variadics)
Objective
- All Phase 3 features compile and pass on macOS. Variadics use Itanium calling conv.

Work items
- Verify function pointer and indirect call lowering produces valid call conv attrs on Darwin.
- Check variadic intrinsics `(va-start/va-arg/va-end)` use the correct target data layout.
- Unions/packed layout: assert size/align with `DataLayout` on Darwin.

Tests
- Enable: `tests/phase3_*` including `fnptr`, `union`, `variadic_runtime`, control flow tests.

Acceptance
- Phase 3 suite green on macOS; a couple of examples in `examples/` JIT‑run as smoke (without EH/coro/traits).

Windows safety
- Keep existing code paths; add platform checks only for callconv differences if needed.

## Phase 4 – Modern platform features (sums, generics, traits, closures, EH, coroutines)
Objective
- Re‑enable all Phase 4 IR forms and JIT on macOS Itanium model; EH=Itanium, Coroutines enabled.

Work items
- Exceptions:
  - Default `EDN_EH_MODEL=itanium` on Apple; ensure functions are tagged `uwtable` and personality `__gxx_personality_v0`.
  - Ensure `(panic)` and `(try)` lower to `invoke/landingpad` on Darwin when `EDN_ENABLE_EH=1`.
- Coroutines:
  - Ensure `coro.*` intrinsics are declared and valid with AppleClang’s LLVM.
  - Verify pass pipeline: CoroEarly → CoroSplit → CoroCleanup runs.
- Traits/generics/closures:
  - Reuse Windows implementation; check for alignment and pointer cast warnings on AppleClang and fix as needed.

CMake
- Add an option to toggle EH/coro easily during bring‑up: `-DEDN_ENABLE_EH=ON -DEDN_ENABLE_CORO=ON` (env or cache vars already used by code).

Tests
- Enable: `tests/phase4_*` except any Windows‑specific SEH tests; run Itanium EH tests and coroutines goldens.

Acceptance
- All Phase 4 tests that are cross‑platform pass on macOS; examples under `examples/` build and a coroutine JIT smoke runs.

Windows safety
- Keep SEH paths under `_WIN32` and select Itanium on APPLE/UNIX; do not alter existing Windows behavior.

## Phase 5 – Language prototypes (Rustlite subset)
Objective
- Build and run Rustlite drivers on macOS; IR verify and JIT a couple of demos.

Work items
- Ensure Rustlite macros expansions don’t assume Windows path separators or `\r\n` newlines.
- Fix any name‑mangling that assumes MSVC; use portable mangling already defined in Phase 4.
- Validate panic model on Darwin (abort + unwind) and trait object calls.

Tests
- Enable: `tests/phase4_*_rustlite*` if present; otherwise run `languages/rustlite/*driver` examples as ctest.

Acceptance
- Rustlite demos run to completion on macOS; output matches Windows.

Windows safety
- No changes to Windows‑specific SEH paths; trait/vtable/closure ABI remains identical.

---

## Concrete task backlog (checklist)
- [ ] Verify CMake LLVM components on macOS arm64; add AArch64 if missing (already in top‑level CMakeLists).
- [ ] Compile `edn` on macOS; fix compiler errors with portable replacements or guards.
- [ ] Turn on Phase 1/2 tests on macOS; fix any DataLayout or predicate mapping issues.
- [ ] Audit varargs lowering on Darwin; add a focused variadic runtime test runner.
- [ ] Wire Itanium EH and run try/catch golden tests on macOS.
- [ ] Enable coroutines and run split/cleanup pipeline; add a minimal JIT smoke on macOS.
- [ ] Build examples: `edn_traits_example`, `edn_generics_example`, `edn_sum_example` and run IR verifier.
- [ ] Bring up Rustlite drivers; fix any string/FS or name unicode issues.
- [ ] Add a GitHub Actions macOS arm64 runner matrix once local is green.

## Known risk areas on macOS
- Differences in default visibility; may need `CMAKE_CXX_VISIBILITY_PRESET` and `VISIBILITY_INLINES_HIDDEN` set conservatively.
- libunwind/libc++ link symbols for EH when actually running unwinding (linker availability via vcpkg).
- ORC JIT permissions on Apple Silicon (W^X); rely on ORC resolver to allocate with correct permissions.
- Coroutines pass availability mismatches if LLVM in vcpkg lags behind AppleClang; prefer vcpkg LLVM for consistency.

## CMake knobs and flags (reference)
- `-DEDN_BUILD_TESTS=ON`
- `-DEDN_ENABLE_EH=ON` (via env)
- `-DEDN_ENABLE_CORO=ON` (via env)
- `-DEDN_TARGET_TRIPLE=arm64-apple-darwin` (env) for cross‑target IR tests
- `-DCMAKE_OSX_ARCHITECTURES=arm64`

## Phase gating in tests
- Keep per‑phase test entrypoints compilable independently. If a phase needs to be temporarily skipped on macOS, use `add_test(NAME ... CONFIGURATIONS ...)` gating or label filters (e.g., `LABELS windows-only`).

---

## Appendix: Quick triage guide for common Darwin build breaks
- Missing headers: replace `<Windows.h>`/`<intrin.h>` uses with portable C++ or `#ifdef _MSC_VER` guarded code.
- Function name case‑insensitivity helpers: `_stricmp` → `strcasecmp` when available; or implement a small portable wrapper in a utility header.
- Bit operations: `_BitScan*` → `__builtin_ctz*`/`__builtin_clz*` or std::countr_zero in C++20.
- 128‑bit math: use builtin `__int128` on Clang/GCC; provide MSVC path separately.
- Alignment/packing: use `alignas()` and `#pragma pack(push,1)` with guards only where needed; prefer no packing.
- `dllexport/dllimport`: keep Windows attributes under `_WIN32`; default visibility controls on others.

