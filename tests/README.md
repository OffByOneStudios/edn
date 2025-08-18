# EDN Tests

This folder contains split test runners to speed up iteration. Each target builds only a subset of tests:

- edn_tests_core: core/unit tests (types, type checker, IR emitter, casts, globals, diagnostics)
- edn_tests_phase2: Phase 2 feature tests
- edn_tests_phase3: Phase 3 feature tests
- edn_tests_phase4: Phase 4 feature tests (sums/match, generics, traits, closures, EH, coro, DI)
- edn_tests_phase5: Phase 5 tests (optimization presets, pipeline override/fallback, verify IR; plus prototype smoke tests where applicable)

## Building

- Configure once:
  - cmake -S . -B build
- Build only a suite:
  - cmake --build build --config Release --target edn_tests_phase4

## Running

- With CTest filters (recommended):
  - ctest --test-dir build -C Release -R "edn\.(core|phase2|phase3|phase4|phase5)$" --output-on-failure
- Or run the binaries directly from build/tests/<Config>/

## Notes

- Tests rely on EDN_SOURCE_DIR defined by CMake so they can find example .edn files.
- If compile times grow again, consider further splitting Phase 4 (e.g., di/coro/eh) into separate runners.
