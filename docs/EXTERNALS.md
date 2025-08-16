# External functions and JIT symbol resolution

Status: Minimal interop in place (2025-08-15)

## Declaring externs
Use `:external true` on a function to emit a declaration without a body:

Example:

```
(fn :name "malloc" :ret (ptr i8) :params [ (param i64 %size) ] :external true)
```

- The type checker records `external=true` and skips body validation for externs.
- The emitter creates an external declaration in the LLVM module.

## Calling externs at runtime
The `phase3_driver` installs a symbol resolver that first queries the JIT dylibs and then the host process. This allows calling common C runtime functions like `malloc` from EDN code when running via the driver.

Relevant snippet (tools/phase3_driver.cpp):
- Registers `DynamicLibrarySearchGenerator::GetForCurrentProcess` with the JIT main dylib.

Example programs:
- `edn/phase3/extern_malloc.edn` – extern declaration sample.
- `edn/phase3/extern_malloc_call.edn` – demonstrates calling `malloc`.

Notes:
- To resolve additional host symbols, ensure they are available in the host process or loaded libraries.
- Name mangling: EDN uses the provided string `:name` verbatim for externs; ensure it matches the host ABI symbol.
