# edn

Header-only EDN parser library with metadata + transformation layer.

## Features (current)
- Header-only: single include `#include <edn/edn.hpp>`
- EDN data types: nil, booleans, integers, floats, strings, keywords, symbols, lists, vectors, maps, sets, tagged values
- Per-node metadata map (string -> node) automatically populated with `line/col/end-line/end-col`
- Transformer system (macro expansion + visitor traversal) for building higher-level IR
- Example LLVM-like IR emitter (see `examples/llvm_ir_emitter.cpp`)
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
