# Phase 2 Feature Samples

Files:
- `floats.edn` – float arithmetic and mixed operations.
- `unsigned.edn` – unsigned integer types.
- `casts.edn` – representative chain of integer/bit-width casts.
- `phi.edn` – control flow with conditional path (phi implied by later SSA usage).
- `suggestions_invalid.edn` – triggers diagnostic `E0902` with suggestion output (misspelled global `GLOB1` as `GLOB`).

These complement Phase 1 basics under `../phase1` (arith, arrays, control, structs, globals).

Environment flags:
- `EDN_SUGGEST=0` will suppress suggestion notes for the invalid sample.
