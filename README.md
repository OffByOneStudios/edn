# edn

Header-only EDN parser library with metadata + transformation layer.

## Features (current)
- Header-only: single include `#include <edn/edn.hpp>`
- EDN data types: nil, booleans, integers, floats, strings, keywords, symbols, lists, vectors, maps, sets, tagged values
- Per-node metadata map (string -> node) automatically populated with `line/col/end-line/end-col`
- Transformer system (macro expansion + visitor traversal) for building higher-level IR
- Example LLVM-like IR emitter (see `examples/llvm_ir_emitter.cpp`)
### IR Emitter Subset (Phase 1)

The embedded experimental IR supports these instruction forms (EDN list syntax):

Arithmetic / Bitwise / Compare
- `(add %dst <type> %a %b)` similarly `sub`, `mul`, `sdiv`
- `(and %dst <int-type> %a %b)` similarly `or`, `xor`, `shl`, `lshr`, `ashr`
- `(eq %dst <int-type> %a %b)` also `ne`, `lt`, `gt`, `le`, `ge` produce `i1`

Constants & Assignment
- `(const %dst <type> <literal>)` (ints / floats only currently)
- `(assign %dst %src)` aliasing a previous SSA value name (non-phi merge convenience)

Memory & Data
- `(alloca %dst <type>)` returns pointer
- `(load %dst <type> %ptr)`
- `(store <type> %ptr %val)`
- Arrays: `(index %dst <elem-type> %arrayPtr %idx)` yields pointer to element
- Struct member: `(member %dst StructName %basePtr fieldName)` loads field value
- Struct member address: `(member-addr %dst StructName %basePtr fieldName)` yields pointer to field

Globals
- Module-level `(global :name G :type i32 :init 7)` (init optional -> zero)
- `(gload %dst <type> G)` / `(gstore <type> G %val)`

Control Flow
- `(if %cond [ ...then... ] [ ...else... ])` else block optional
- `(while %cond [ ...body... ])` with `(break)` to exit early
- `(ret <type> %val)`

Functions & Calls
- `(fn :name "add" :ret i32 :params [ (param i32 %a) (param i32 %b) ] :body [ ...instructions... ])`
- `(call %dst <ret-type> calleeName %arg1 %arg2 ...)`

Types are provided inline using forms like `i32`, `i1`, `(ptr i32)`, `(array :elem i32 :size 4)`, `(struct-ref MyS)`.

This is intentionally minimal: no PHI nodes (merges use temporary stack slots), no floating-point arithmetic yet, and structs are declared with `(struct :name MyS :fields [ (field :name a :type i32) ... ])`.

Phase 2 adds explicit phi node support:
`(phi %dst <type> [ (%val %predBlockName) ... ])`
Example:
```
(const %t i1 1)
(if %t [ (const %a i32 1) ] [ (const %b i32 2) ])
(phi %m i32 [ (%a if.then.0) (%b if.else.1) ])
(ret i32 %m)
```
Block names correspond to the auto-generated structured control flow labels (`if.then.N`, `if.else.N`, etc.).

- Minimal printing (best-effort, non-canonical formatting)

### Diagnostics (Phase 2)
The type checker now emits structured diagnostics:

Format (driver output):
```
error[E0003]: binop type must be integer (line 12:5)
	hint: choose one of i1/i8/i16/i32/i64/u8/u16/u32/u64
```

Each `TypeError` / `TypeWarning` contains:
- code: stable identifier (`E####` / `W####` or category code like EMOD1)
- message: short human description
- hint: optional quick-fix guidance
- notes: optional related messages (future use)

Current code ranges (subject to expansion):
- E0001–E0007 integer arithmetic
- E0100–E0106 legacy (deprecated) simple cmp ops
- E0110–E0118 icmp (typed integer comparisons)
- E0120–E0128 fcmp (typed float comparisons)
- E0200–E0214 load / store
- E0300–E0309 phi construction
- E0400–E0408 function call
- E0500–E0508 cast family (arity/src/dst/redefinition + validation)
- E0600–E0606 bit / logical (and/or/xor/shifts)
- E0700–E0706 floating arithmetic (fadd/fsub/fmul/fdiv)
- E0800–E0818 struct member / member-addr
- E0820–E0827 index (array element access)
- E0900–E0905 global load
- E0910–E0915 global store
- E1000–E1007 control flow if/while/break
- E1010–E1012 return
- E1100–E1110 const / assign / alloca + related
- E1200–E1209 struct-lit
- E1210–E1218 array-lit
- E1220–E1228 global const/initializer validation (scalar/array/struct + const store rejection)

General fallback uses `EGEN` (generic) until specialized code coverage is completed for all instructions.

### New Aggregate & Global Features (Phase 2 Milestones M4/M5)

Instructions:
- `(struct-lit %dst StructName [ field1 %v1 field2 %v2 ... ])` -> allocates stack struct and stores field values (result is pointer to struct)
- `(array-lit %dst <elem-type> <size> [ %e0 %e1 ... ])` -> allocates stack array and stores elements (result is pointer to array)

Const Globals:
```
(global :name G :type i32 :init 7 :const true)
(global :name ARR :type (array :elem i32 :size 3) :init [1 2 3] :const true)
(struct :name P :fields [ (field :name x :type i32) (field :name y :type f32) ])
(global :name S :type (struct-ref P) :init [1 2.0] :const true)
```
Diagnostics ensure initializer shapes and literal types match declared global types and disallow storing to a `:const` global (E1226).

## Not yet implemented / roadmap
- Big integers / ratios
- Character literals
- Namespaces for keywords & symbols
- Tagged literal dispatch / user extension registry
- Proper hash-based set & map preserving insertion order (currently vector-backed)
- Streaming / incremental parser
- Performance tuning & benchmarking harness

## Build & Test
```pwsh
cmake -S . -B build -DEDN_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## Using with CMake (after installation or via vcpkg)
```cmake
find_package(edn CONFIG REQUIRED)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE edn::edn)
```

## vcpkg Port (local overlay)
Add the `vcpkg/ports/edn` folder to an overlay, then install:
```pwsh
vcpkg install edn --overlay-ports=path/to/edn/vcpkg/ports
```

Example usage (CMake):
```cmake
find_package(edn CONFIG REQUIRED)
add_executable(demo main.cpp)
target_link_libraries(demo PRIVATE edn::edn)
```

Minimal parse example:
```cpp
#include <edn/edn.hpp>
int main(){
	auto ast = edn::parse("(add 1 2)");
	// positions
	int line = edn::line(*ast); // 1
}
```

## License
MIT (see LICENSE)

## Contributing
See [CONTRIBUTING.md](CONTRIBUTING.md). Please add tests for new features.

## Changelog
See [CHANGELOG.md](CHANGELOG.md) for upcoming and past changes.
