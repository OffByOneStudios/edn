# edn

Header-only EDN parser & experimental LLVM-oriented IR (+ type checker, diagnostics, and Phase 3/4/5 features). Includes a tiny Rust-like frontend (Rustlite) to drive end-to-end demos.

## Features (current)
- Header-only: single include `#include <edn/edn.hpp>`
- Rich EDN data model: nil, booleans, integers, floats, strings, keywords, symbols, lists, vectors, maps, sets, tagged values
- Per-node metadata (line/col span) auto-populated; accessible for diagnostics
- Macro / transformer system for syntactic extension & metadata enrichment
- LLVM IR emission for a custom SSA instruction subset
- Multi-phase language growth (Phases 1–3, plus experimental Phase 4 and Phase 5 prototypes) with stable error codes & structured notes
- JSON diagnostics export (`EDN_DIAG_JSON=1`)

### IR Instruction Set (Phases 1–3)

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

Additional Phase 2 / 3 instructions & forms:
- PHI nodes: `(phi %dst <type> [ (%val %predBlock) ... ])`
- Float arithmetic: `fadd`, `fsub`, `fmul`, `fdiv`
- Floating comparisons: `(fcmp %dst <f-type> :pred <pred> %a %b)`
- Pointer arithmetic: `(ptr-add)`, `(ptr-sub)`, element difference `(ptr-diff)`
- Address-of & dereference sugar: `(addr %p (ptr <T>) %v)`, `(deref %v <T> %p)`
- Function pointers & indirect calls: `(fnptr ...)`, `(call-indirect %dst <ret> %fptr %args...)`
- Typedef aliases: `(typedef :name Alias :type <type>)`
- Enums: `(enum :name E :underlying i32 :values [ (eval :name A :value 0) ... ])`
- Unions: `(union :name U :fields [ (ufield :name a :type i32) ... ])` + `(union-member %v U %ptr field)`
- Variadic functions: `:vararg true` on `fn` + runtime vararg intrinsic support
- For loops & continue: `(for :init [...] :cond %c :step [...] :body [...])`, `(continue)`
- Switch: `(switch %expr :cases [ (case <const> [ ... ]) ... ] :default [ ... ])`
- Cast sugar: `(as %dst <to-type> %src)` desugared to concrete cast ops (zext, trunc, uitofp, etc.)
- Structured global initializers: `(global :name G :type (struct-ref S) :init [ ... ] :const true)` with validation

Phase 3 feature sample programs: see `edn/phase3/` for one `.edn` file per new construct plus a globals mismatch diagnostics example. Use the `phase3_driver` tool to JIT-run them, e.g.:
```pwsh
cmake --build build --config Debug
./build/Debug/phase3_driver.exe edn/phase3/cast_sugar.edn cast_demo
```

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

### Diagnostics
Structured diagnostics with stable error codes, per-error hints, and optional note list (`expected` vs `found`). JSON mode enabled by setting `EDN_DIAG_JSON=1` before running tests / emitter.

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

Current (Phase 1–3) code ranges:
- E0001–E0007 integer arithmetic
- E0100–E0106 legacy deprecated cmp wrappers (eq/ne/lt/...)
- E0110–E0118 icmp predicates
- E0120–E0128 fcmp predicates
- E0200–E0214 load / store
- E0300–E0309 phi (with structured incoming mismatch notes on E0309)
- E0400–E0408 direct function call
- E0500–E0508 explicit cast op family
- E0600–E0606 bit / logical ops
- E0700–E0706 float arithmetic
- E0800–E0818 struct member / member-addr
- E0820–E0827 index (array element access)
- E0900–E0905 global load
- E0910–E0915 global store
- E1000–E1007 if / while / break
- E1010–E1012 return
- E1100–E1110 const / assign / alloca
- E1200–E1209 struct literal
- E1210–E1218 array literal
- E1220–E1228 global const & initializer validation (E1220/E1223/E1225 emit expected/found notes)
- E1300–E1309 pointer arithmetic
- E1310–E1319 address-of / deref
- E1320–E1329 function pointer + indirect call
- E1330–E1334 typedef
- E1340–E1349 enum
- E1350–E1359 union
- E1360–E1369 variadic functions
- E1370–E1379 for loop
- E1380 misuse of continue (reserved range)
- E1390–E1399 switch
- E13A0–E13A5 cast sugar dispatcher `(as ...)`

Notes use the `TypeNote` vector; mismatch helpers add two notes: one `expected <T>`, one `found <T>`.

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

## Phase 3 Out-of-Scope / Deferred
The following originally proposed Phase 3 items are now explicitly deferred:
- Source span mapping for non-S-expression surface (would require alternate parser)
Optimization pass pipeline (`EDN_ENABLE_PASSES`) with presets via `EDN_OPT_LEVEL` (O0/O1/O2)
- Union write/store convenience op (current API only supports reads via `union-member`)
- Active union variant tracking / safety tagging
- Additional suggestion coverage for typedef / enum / union fields (basic may be present; advanced fuzzy ranking deferred)
- Constant-folding control toggles (current cast materialization workaround accepted for tests)

General future roadmap (unrelated to Phase 3 closure):
Optimization presets (Phase 5):
 - Set `EDN_ENABLE_PASSES=1` to enable the opt pipeline; select level with `EDN_OPT_LEVEL=0|1|2|3` (defaults to 1). `0` preserves IR (no passes), `1`/`2`/`3` use LLVM's default O1/O2/O3 module pipelines.
 - Advanced: provide a custom textual LLVM pipeline via `EDN_PASS_PIPELINE` (e.g., `default<O2>` or `module(ipsccp,globalopt)`). When set, this overrides `EDN_OPT_LEVEL`.
 - Debugging: set `EDN_VERIFY_IR=1` to run LLVM's IR verifier before and after the pass pipeline (custom or preset). Verification errors will be printed to stderr but will not abort emission.

Micro-benchmarks:
 - Build `edn_bench` and run the CSV-emitting microbench harness:
 ```pwsh
 cmake --build build --config Release --target edn_bench
 ctest --test-dir build -C Release -R "edn\.bench\.basic$" --output-on-failure
 ```
 You can experiment with `EDN_ENABLE_PASSES` and `EDN_OPT_LEVEL` to observe IR size/time changes.
- Big integers / ratios
- Character literals
- Namespaces for keywords & symbols
- Tagged literal dispatch / user extension registry
- Streaming / incremental parser
- Performance tuning & benchmarking harness

## Build & Test
```pwsh
cmake -S . -B build -DEDN_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

### Examples
Build the examples and run the traits demo:
```pwsh
cmake --build build --config Release --target edn_traits_example
./build/examples/Release/edn_traits_example.exe
```

More Phase 4 demos:
```pwsh
cmake --build build --config Release --target edn_generics_example edn_sum_example
./build/examples/Release/edn_generics_example.exe
./build/examples/Release/edn_sum_example.exe
```

Docs:
- docs/TRAITS.md
- docs/GENERICS.md
- docs/SUMS.md
 - docs/EXTERNALS.md
 - docs/EH.md
 - docs/COROUTINES.md
 - docs/RUSTLITE.md
 - docs/EDN_BUILDER.md

Note on IR printing: LLVM quotes symbol names that contain special characters. For example, generic instances are mangled like `id@i32`, which will appear in IR as `@"id@i32"`. Tests and string matches should account for the quotes.

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

## JSON Diagnostics Example
Set the environment variable before running your tool/tests:
```pwsh
$env:EDN_DIAG_JSON=1; .\build\tests\Debug\edn_tests.exe
```
Sample output snippet:
```json
{"success":false,"errors":[{"code":"E1220","message":"global scalar initializer type mismatch","hint":"match literal to declared type","line":3,"col":1,"notes":[{"message":"expected i32","line":3,"col":1},{"message":"found f64","line":3,"col":1}]}],"warnings":[]}
```

## Changelog
See [CHANGELOG.md](CHANGELOG.md) for detailed changes.

## Phase 3 Driver & Examples
`phase3_driver` mirrors the original `phase1_driver` but supports the extended instruction set and structured JSON diagnostics. Examples live under `edn/phase3`. A failing diagnostics showcase (`globals_const_notes.edn`) intentionally triggers errors with expected/found notes; run it for JSON output:
```pwsh
$env:EDN_DIAG_JSON=1; ./build/Debug/phase3_driver edn/phase3/globals_const_notes.edn use
```

## Phase 4 (experimental): Traits, Generics, Sum Types, Closures

Phase 4 adds experimental surface macros for traits, generics, and sum types with lowering into the core typed IR. These are exercised by tests and a small example.

Highlights for Traits:
- Define a trait and its methods:
	`(trait :name Show :methods [ (method :name print :type (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32))) ])`
- The expander synthesizes two structs:
	- `ShowVT`: vtable with one field per method (field names are symbols)
	- `ShowObj`: object wrapper `{ data: (ptr i8), vtable: (ptr ShowVT) }`
- Construct an object and call a method:
	- `(make-trait-obj %o Show %dataPtr %vtPtr)` produces a `ShowObj` struct literal
	- `(trait-call %dst i32 Show %o print %x)` expands to `member-addr`/`load` and `call-indirect`

Important constraints from the type checker:
- Struct field `:name` values must be symbols (not strings); function `:name` must be a string.
- `ret`, `load`, `store`, `call-indirect` require explicit types.
- Don’t take the address of an alloca result twice: pass `%obj` directly to `(make-trait-obj ...)`.

See the new example target `edn_traits_example` in `examples/` and the detailed notes in `docs/TRAITS.md`.

### Closures
- Minimal thunk form: `(closure %dst (ptr <fn-type>) %fn [ %env ])`
	- Non-escaping, single-capture. Type checks in E1430–E1435. Lowers to a private thunk + private env global; `%dst` is the thunk fnptr.
- Record form: `(make-closure %dst Callee [ %env ])` builds a closure record and `(call-closure %dst <ret> %clos %args...)` invokes it.
	- Layout: `struct __edn.closure.<Callee> { i8* fn; <EnvType> env; }`
	- Call semantics: the stored `fn` is called indirectly with `env` as the first argument followed by user args; return type must match `<ret>`.
	- Validation:
		- `make-closure` arity and capture checks (E1436 for arity; E1433/E1434 for capture vector/typing).
		- `call-closure` return/arg checks and closure type checks (E1437 family).
- Tests: `tests/phase4_closures_min_test.cpp`, `tests/phase4_closures_record_test.cpp`, `tests/phase4_closures_negative_test.cpp`.

## Phase 5: Prototypes and Optimization Presets

Phase 5 focuses on tiny language prototypes and opt/pipeline controls. A Rust-like prototype (Rustlite) lives under `languages/rustlite/` and demonstrates:
- Sum types and result-mode match via a `rif-let` macro.
- Simple `let`/`mut` sugar via macros lowering to EDN `block` bodies.
- Driver-level PHI validation to ensure result-mode lowering never yields `undef` incoming values.

Docs: see `design/phase5_plan.md`, `design/rustlite.md`, and the quickstart in `docs/RUSTLITE.md`.
