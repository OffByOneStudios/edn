# Changelog

All notable changes will be documented here once releases begin.

## [Unreleased]
- Initial node-based AST with metadata (line/column)
- Transformer (macro expansion + visitors)
- Example LLVM-like IR emitter (Phase 1 subset):
	- Types: base ints/floats, void, pointer, array, struct, function
	- Struct declarations & field access / address
	- Arithmetic: add/sub/mul/sdiv
	- Bitwise/logical: and/or/xor/shl/lshr/ashr
	- Comparisons: eq/ne/lt/gt/le/ge (integers)
	- Memory: alloca/load/store, array index (GEP)
	- Globals: definition + gload/gstore
	- Control flow: if / if-else / while / break / ret
	- Calls with prototype pre-pass
	- Member vs member-addr pointer distinction
	- Negative type checking tests
- vcpkg port scaffold
