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

- Minimal printing (best-effort, non-canonical formatting)

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
