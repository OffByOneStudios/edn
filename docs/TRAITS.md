# Traits (Experimental Phase 4)

This document explains the experimental traits feature: how to define a trait, construct a vtable and an object, and invoke methods. Traits are currently implemented as a macro expansion pass over EDN S-expressions that lowers to the core typed IR supported by the type checker and LLVM emitter.

Status: experimental. Shapes and error codes may change. Field names must be symbols to satisfy the type checker.

## Overview

- Define a trait with one or more methods.
- The expander generates two structs per trait:
  - `<Trait>VT`: the vtable type; one field per method (field names are symbols)
  - `<Trait>Obj`: `{ data: (ptr i8), vtable: (ptr <Trait>VT) }`
- Construct a trait object from a data pointer and a vtable pointer.
- Invoke a trait method via `trait-call`, which expands into `member-addr`, `load`, and `call-indirect` on a function pointer with an implicit first `ctx` argument taken from the `data` field of the object (type `(ptr i8)`).

## Syntax

Trait declaration:
```
(trait :name Show
       :methods [ (method :name print
                          :type (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32))) ])
```

Lowered types (produced by the expander):
```
(struct :name "ShowVT"
        :fields [ (field :name print :type (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32))) ])
(struct :name "ShowObj"
        :fields [ (field :name data   :type (ptr i8))
                  (field :name vtable :type (ptr ShowVT)) ])
```

Constructing an object and calling a method:
```
; Build a vtable instance and store a function pointer into its print slot
(fnptr %fp (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) print_i32)
(alloca %vt ShowVT)
(member-addr %p ShowVT %vt print)
(store (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) %p %fp)

; Stack-allocate the object payload and make a trait object
(alloca %obj ShowObj)
(bitcast %vtp (ptr ShowVT) %vt)
(make-trait-obj %o Show %obj %vtp)

; Call through the trait
(trait-call %rv i32 Show %o print %x)
```

Expansion of the last two forms (conceptually):
```
; make-trait-obj -> struct-lit with i8* data and vtable pointer
(bitcast %data.i8 (ptr i8) %obj)
(struct-lit %o ShowObj [ data %data.i8 vtable %vtp ])

; trait-call -> member-addr/load chain + call-indirect
(member-addr %vt.addr ShowObj %o vtable)
(load %vt.ptr (ptr ShowVT) %vt.addr)
(member-addr %fn.addr ShowVT %vt.ptr print)
(load %fn.ptr (ptr (fn-type ...)) %fn.addr)
(member-addr %data.addr ShowObj %o data)
(load %ctx (ptr i8) %data.addr)
(call-indirect %rv i32 %fn.ptr %ctx %x)
```

## Type Checker Constraints

- Struct field `:name` must be a symbol; struct `:name` and function `:name` must be strings.
- `ret`, `load`, `store`, and `call-indirect` require explicit types.
- For `member-addr`, the base must be a pointer to the struct, and the field name is a symbol.
- Avoid taking an address of an address: `alloca` yields a pointer already.

## Example

A minimal example is provided at `examples/traits_example.cpp` and built as target `edn_traits_example`.

It parses a module that defines `Show`, builds a vtable with `print_i32`, constructs a `ShowObj`, dispatches `Show.print`, type-checks, emits LLVM IR, and verifies the module.

## Diagnostics

If the expander or your input violates the checker rules, you may see errors like:
- E1315: addr source type mismatch
- E0814: struct member base undefined or wrong type
- EGEN: unknown instruction (if macro forms aren’t recognized)

Use `EDN_DIAG_JSON=1` to get machine-readable diagnostics for tooling.
