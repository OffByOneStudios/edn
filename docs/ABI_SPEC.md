# ABI Specification (Draft v0)

Status: DRAFT (unstable). This document will evolve as interop and additional surface languages are implemented.

## Scope
Defines calling conventions, type layout, value categories (by-value, by-ref, opaque), ownership & lifetime rules, and runtime helper contracts for all code compiled through the EDN macro & IR pipeline.

## Goals
- Stable, language-neutral bridge for cross-surface calls.
- Zero / minimal copy for common value categories (scalars, small structs).
- Explicit ownership verbs (copy, move, drop) where destruction or resource transfer occurs.
- Deterministic layout enabling mixed-module inlining and debug mapping.

## Non-Goals (v0)
- Full exception interop across all surfaces (only panic/unwind hooks prototyped).
- Full async/await state machine ABI.
- Distributed / remote ABI.

## Value Categories
| Category | Examples | Passing | Returning | Notes |
|----------|----------|---------|-----------|-------|
| Scalar | i1/i8/i16/i32/i64/f32/f64 | in register | in register | Sign/zero extend per LLVM default |
| Aggregate (Plain) | struct w/ POD fields | by value if <= 2 regs else by pointer | same rule | Packed = disallowed in v0 |
| Sum / Enum | tagged union / variant | pointer to payload struct | pointer | Tag is first machine word |
| Trait Object | { data ptr, vtable ptr } | by value (two pointers) | by value | 2 pointer words |
| Closure Record | { fn ptr, env (POD) } | by value if small else pointer | same rule | Env may spill threshold > 2 regs |
| Large Array | >2 machine words | pointer | pointer | Caller retains ownership |
| Opaque Handle | runtime-managed resource | pointer | pointer | Must use helper verbs |

## Ownership Verbs
- copy(T*): duplicate value (bitwise or custom)
- move(T* src, T* dst): transfer; src becomes logically invalid
- drop(T*): release resources / recursively drop fields

Runtime will provide symbol naming convention: `__edn_copy_<mangledType>` etc. If absent, bitwise copy is assumed for POD.

## Layout Rules
1. Struct field order = declared order, natural alignment.
2. Sum (variant) layout: first word = tag (u32), followed by payload union aligned to max member alignment.
3. Trait object: data ptr (void*), then vtable ptr (void*).
4. Closure thunk record: fn ptr (void*), env inline storage (struct); large env spills to heap block referenced by pointer (future optimization hint only).

## Calling Convention
- Default: C ABI as provided by underlying platform (LLVM uses target default) for exported functions.
- Internal cross-surface calls may inline; still adhere to layout/ownership semantics.
- Variadic not stabilized for polyglot boundary (discouraged for inter-surface in v0).

## Runtime Helpers (Initial Set)
| Helper | Purpose |
|--------|---------|
| edn_alloc(size_t) -> void* | Allocate raw memory (aligned) |
| edn_dealloc(void*) | Free raw memory |
| edn_panic(const char* msg) [[noreturn]] | Abort with message |
| edn_tag_of(void* sumPtr) -> u32 | Read tag word |
| edn_sum_payload(void* sumPtr) -> void* | Access payload base |
| edn_trait_call(vtable*, index, dataPtr, args...) | Generic dispatch (optional) |

## Error Handling
- Panic/unwind prototype only; cross-language catch not guaranteed.
- Future: typed error channels via sum return (Result<T, E>) canonicalization.

## Conformance Tests (Planned)
- Layout checksum tests (compute SHA of field offsets + sizes) to detect drift.
- Cross-language call passing each category, verifying round-trip fidelity.
- Ownership stress (copy vs move vs double-drop detection under ASAN/UBSAN).

## Evolution Process
1. Propose change via PR modifying this doc (add DRAFT section).
2. Implement behind feature flag (e.g., EDN_ABI_EXPERIMENT_FOO).
3. Add/extend conformance tests.
4. Graduate to STABLE section after two releases with no incompatible changes.

## Open Questions
- Should we canonicalize small struct passing (<3 words) always by registers irrespective of platform defaults? (TBD)
- Inline string (short string optimization) policy? (TBD)
- Async frame layout & cancellation hooks? (TBD)

---
This is a living document; expect updates as interop proof-of-concepts land.
