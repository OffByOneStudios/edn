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
### Added (Phase 2 Progress)
- Unsigned integer types (u8/u16/u32/u64) and associated arithmetic/division and comparison predicates.
- Canonical `(icmp ...)` and `(fcmp ...)` predicate-based comparison instructions.
- Floating point arithmetic instructions: fadd/fsub/fmul/fdiv.
- Cast instruction family: zext/sext/trunc/bitcast/sitofp/uitofp/fptosi/fptoui/ptrtoint/inttoptr with validation diagnostics.
- Explicit phi nodes `(phi %dst <type> [ (%v %label) ... ])`.
- Aggregate literals: `(struct-lit ...)` and `(array-lit ...)` producing pointer results.
- Const global data with scalar, array, and struct initializers via `:const true` and `:init` (supports base scalar elements/fields).
- Extended structured diagnostics with dedicated code ranges for new instruction families and global initializer validation (E1200+ and E1220+).

### Changed
- README diagnostics section expanded with aggregate literal and const global codes.

### Fixed
- Added validation preventing mutation of const globals.
