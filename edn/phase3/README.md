# Phase 3 Feature Samples

Files:
- `pointer_arith.edn` – pointer add/sub/diff forms.
- `addr_deref.edn` – address-of and dereference sugar.
- `fnptr_indirect.edn` – function pointer acquisition and indirect call.
- `typedef_enum.edn` – typedef alias and enum usage.
- `union_access.edn` – basic union declaration and member read.
- `variadic.edn` – variadic function with vararg intrinsics.
- `for_continue.edn` – for-loop with continue/break skeleton.
- `switch.edn` – switch construct with cases and default.
- `cast_sugar.edn` – generic `(as ...)` cast dispatcher.
- `globals_const_notes.edn` – global const initializers including mismatch examples producing structured notes.

To exercise diagnostics notes for globals run the driver against `globals_const_notes.edn` and observe errors for BAD1, ARR, S.

Environment helpers:
- `EDN_DIAG_JSON=1` emits JSON diagnostics.
- `EDN_SUGGEST=0` disables symbol suggestions.
