# edn

An experimental polyglot compilation substrate:
- A fast EDN parser + rich, typed core IR
- A minimal set of well-specified intrinsics (LLVM backed)
- A macro system to lower multiple high-level surface languages into a single IR
- JIT & AOT emission paths sharing one semantic core

Ultimate vision: Seamless interop across modern language surfaces (Rust↔Python↔Zig↔TypeScript, etc.) in one process without FFI glue, fragmentation, or per-language runtimes. Multiple languages become syntactic veneers that macro‑lower into the same strongly‑typed EDN/IR graph, enabling mixed-module optimization, unified diagnostics, and shared tooling.


## Quickstart:
* Install cmake
* Install and configure: [vcpkg](https://github.com/Microsoft/vcpkg)
* Clone this project:
```sh
git clone https://github.com/OffByOneStudios/edn
```
* Configure and build project:
```sh
cmake --preset default
cmake --build build
```


## Why
Today polyglot means: brittle FFI boundaries, duplicated runtime stacks, impedance mismatches in types, and separate debuggers/toolchains. We want:
1. Single canonical IR + type system with zero-copy value passing where possible.
2. Macro driven surface growth: add a new language by teaching a macro expander, not by forking the backend.
3. Deterministic, structured diagnostics (stable error codes) across all surfaces.
4. Unified optimization + JIT pipeline (LLVM) applied after surface lowering, so cross-language inlining/AA/opts "just happen".
5. Opt-in AOT for deployment; same artifacts, same semantics.

## Core Concepts
- EDN Parse Layer: S-expression + tagged literals + metadata (line/col) becomes the lossless syntax tree feeding macros.
- Macro Expansion: Declarative / procedural transformers enrich & rewrite EDN into a typed Core IR form. Each surface construct (e.g. Rust-like `match`, Python-style `await`, Zig-style error union) is a macro pattern lowering to canonical primitives (blocks, phi, struct, sum, call, trait object, etc.).
- Typed Core IR: SSA-ish instruction set (arithmetic, control flow, memory, aggregates, traits, sums, closures). Stable, small, deliberately opinionated; no surface sugar.
- Intrinsics: Minimal set (alloca, load/store, pointer math, coroutine/eh hooks, trait dispatch, closure thunking). Everything else is macros.
- Emission: LLVM IR builder + optional pass pipeline -> JIT (MCJIT/ORC) or AOT object/library generation.
- Diagnostics: Phase-tagged with codes (E####) + optional JSON stream; macro layers can add high-level context notes while still mapping to core errors.

## Roadmap Snapshot (High-Level)
Near Term:
- Surface macro coverage expansion (ranges, richer match payloads, closure capture inference, error handling sugar)
- Cross-language value ABI spec (layout + ownership rules) & first interop demo (Rustlite ↔ pseudo-Python macro layer)
- Deterministic snapshot tests for macro lowering graphs

Mid Term:
- Pluggable language frontends (Rustlite, PyLite, Ziglet, TSLite) coexisting in one module
- Mixed-module inlining & trait/closure interop across surfaces
- Incremental / streaming parser variant

Long Term:
- Profile-guided multi-surface optimization
- Deterministic caching of expanded IR (build graph integration)
- On-the-fly REPL attaching new surface modules into a live JIT session

## Status (2025-08)
Implemented:
- Rich EDN data forms + per-node metadata
- Macro system & phased feature growth (traits, generics, sums, closures prototypes)
- Core IR: arithmetic, control flow (if/while/for/switch), phi, memory, structs, arrays, enums, unions, sum types, traits (vtable objects), closures (thunk + record forms), varargs, pointer & cast suite, global consts, basic coroutine & EH hooks
- Rustlite prototype surface with expanding coverage (arrays, indexing, matches, rtry, loops, enums, sums, partial pattern payload binding)
- JSON diagnostics export & stable error code registry
- LLVM emission with optional verification / opt pipeline controls

In Progress / Next:
- Additional Rustlite surface constructs (range forms, compound shifts, richer diagnostics)
- Negative test matrix build-out for parser & macro errors
- First cross-language shim demo (Rustlite + skeletal PyLite macro set)
- ABI formalization doc & test harness

## Architecture (Simplified Flow)
```
   surface.rs / surface.py / surface.ziglike / surface.ts
          | (tokenize/parse to EDN forms or direct EDN literals)
          v
      EDN AST  --(macro expansion passes)-->  Typed Core IR  --(LLVM emission)-->  JIT / Obj / Wasm (future)
                              ^
                              | diagnostics (codes, notes, JSON)
```

## Adding a New Surface Language (Conceptual Contract)
1. Parse / translate source into EDN S-expressions enriched with metadata.
2. Provide macro set: pattern match surface forms -> core IR constructs (possibly staged: desugar high-level to mid-level macros -> primitive IR).
3. Register any intrinsic needs (if genuinely non-expressible via existing primitives) or layer them as further macros.
4. Extend test suite: (a) surface sample -> golden normalized expanded IR, (b) negative samples (expected diagnostics), (c) interop sample with another surface.
5. Document: grammar sketch + lowering table.

## Diagnostics Philosophy
- Every error: stable code, terse message, optional hint, structured notes.
- Macro layers may wrap errors adding context (e.g., "while lowering match arm"), but underlying core code remains traceable.
- JSON stream mode for editor integration.

## Example (Toy Surface Desugaring Sketch)
Surface snippet (Rustlite-like):
```
match opt {
  Some(x) => x + 1,
  None => 0,
}
```
Macro-expanded EDN (illustrative):
```
(ematch %__m i32 %opt :cases [
  (arm Some :binds [x] :body [ (add %t i32 %x (const %c i32 1)) (ret i32 %t) ])
  (arm None :body [ (ret i32 (const %z i32 0)) ])
])
```
Further lowered (phi, blocks) inside the core IR builder prior to LLVM emission.

## Build & Test (CMake)
```
cmake -S . -B build -DEDN_BUILD_TESTS=ON
cmake --build build -j 8
ctest --test-dir build --output-on-failure
```

## Embedding / Linking
```
find_package(edn CONFIG REQUIRED)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE edn::edn)
```

## Minimal Parse Snippet (C++)
```cpp
#include <edn/edn.hpp>
int main(){
    auto ast = edn::parse("(add 1 2)");
    if(ast) { /* inspect */ }
}
```

## License
MIT

## Contributing
Please add focused tests for each new surface construct or macro pass. Negative tests are first-class.

## FAQ (Early)
Q: Why not just use existing IRs (MLIR, WASM) directly?
A: We constrain scope intentionally: a small, opinionated IR tightly aligned with macro-driven multi-surface lowering keeps cognitive + implementation overhead low while still leveraging LLVM for heavy lifting.

Q: Why EDN syntax instead of inventing a new one?
A: EDN is readable, data-centric, composable, and already encodes rich literals + metadata pathways. Great for macro authoring and golden tests.

Q: How will Python / TS integrate (dynamic types)?
A: Frontends encode dynamic values as sum / boxed structural forms with tagged union patterns; macro passes inject runtime type tests that still collapse to static IR paths when types stabilize.
