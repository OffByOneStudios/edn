# EDN ABI Specification (v0.1)

Status: Draft (2025-08-17)

This document freezes the initial set of Application Binary Interface (ABI) conventions used by EDN‑generated LLVM IR so multiple front‑ends can interoperate and we can stabilize cross‑version behavior. Additive changes are permitted; breaking changes require bumping the ABI version.

ABI version: 0.1

## 1. Conventions
- Target triples and DataLayout determine sizes/alignments. EDN follows LLVM DataLayout for all aggregates.
- Default calling convention is C (cdecl); platform‑specific CCs may be selected per function.
- Varargs follow the platform C ABI (already supported).
- Large aggregate returns may use sret when mandated by the platform ABI.

## 2. Closures

### 2.1 Value Representation
A closure value is a pair consisting of:
- invoke: function pointer with the signature: R (Env*, P1, P2, ...)
- env: pointer to an environment struct (heap or stack allocated depending on escape analysis)

Layout (conceptual C):
struct __edn_closure {
    void* invoke;   // function pointer to trampoline/thunk;
    void* env;      // pointer to environment struct
};

Notes:
- The actual LLVM IR may use the concrete function pointer type in vtables/records; the conceptual pair is stable.
- The invoke function always receives Env* as its first argument, followed by original params.

### 2.2 Environment Struct
- Name: "struct.__edn.closure.<fn>" (implementation may append a uniquing suffix).
- Fields: one field per captured variable in source order.
- Size/Align: computed from DataLayout; no implicit padding beyond what LLVM inserts for alignment.

Stability:
- Capture field order is FROZEN as source order.
- Passing convention (Env* first) is FROZEN.

## 3. Trait Objects and VTables

### 3.1 Fat Pointer Representation
A trait object (interface value) is a fat pointer with two fields:
- data: i8* pointing to the underlying object storage (or concrete value address)
- vtable: pointer to the trait vtable for the concrete type

Conceptual C:
struct __edn_trait_obj {
    void* data;
    void* vtable; // pointer to struct __edn_vtable_<Trait>
};

### 3.2 VTable Layout
- Name: "struct.__edn.vtable.<Trait>" (implementation may append uniquing suffix for generics).
- Field order: method pointers in trait declaration order.
- Method pointer signature: matches the EDN function signature, with the receiver/data pointer (i8* or typed*) as the first parameter when required by the dispatch strategy.

Stability:
- VTable method order is FROZEN as trait declaration order.
- Fat pointer field order (data, then vtable) is FROZEN.

## 4. Sum/Union Types (Tagged Unions / ADTs)

### 4.1 Representation
A sum type value is represented as a struct containing:
- tag: 32‑bit discriminant (variant ordinal starting at 0, declaration order)
- payload: byte array large enough to hold the largest variant payload, aligned to the maximum alignment among variants

Conceptual C:
struct __edn_sum_T {
    uint32_t tag;
    // padding to align payload to max_align
    unsigned char payload[PAYLOAD_SIZE];
};

Notes:
- Variants with no payload occupy zero bytes in the payload region.
- PAYLOAD_SIZE and alignment are computed using LLVM StructLayout across variants.

Stability:
- Tag is 32‑bit, variant order is declaration order (FROZEN).
- Payload layout uses max‑size + max‑align among variants (FROZEN for v0.1).

## 5. Slices / Fat Pointers (Reserved)
For future use (not required in Phase 4): slices will use { T* ptr, usize len } with platform usize.

## 6. Calling Conventions & Attributes
- Default CC: C. Others (fastcall/vectorcall/thiscall) may be specified explicitly.
- sret/byval/byref attributes are applied per target ABI rules.
- Varargs follow the C ABI.

## 7. Exceptions and Panic
- Itanium EH: personality __gxx_personality_v0; SEH on Windows uses __C_specific_handler; selection is controlled via environment flags.
- Panic modes: abort or unwind. Unwind lowers to platform‑specific throw/raise routines.

## 8. Names and Mangling
- Identified structs use the prefixes:
  - struct.__edn.closure.<fn>
  - struct.__edn.vtable.<Trait>
  - struct.__edn.sum.<Type> (recommended; implementation detail may vary)
- Function and symbol mangling remains internal; external C interop symbols should be declared extern "C" to avoid mangling.

## 9. Validation (Golden Tests)
- Closure: assert env struct exists with expected field count/order; invoke signature starts with Env*.
- Trait/vtable: assert vtable struct exists and method pointer count/order matches trait definition.
- Sum/union: assert 32‑bit tag and payload size >= each variant payload; alignment matches max among variants.

## 10. Versioning
- ABI v0.1 is the initial frozen set for Phase 5 prototypes.
- Additive changes (new vtable entries appended, new traits) are allowed; reordering or representation changes are breaking and require a version bump.
