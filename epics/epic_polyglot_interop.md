# Epic: Polyglot Interop & Unified Runtime

## Goal
Enable seamless, zero-glue interop between multiple high-level languages (Rust, Python, Zig, TypeScript, etc.) in a single process, all compiled to a shared IR and runtime, without FFI boundaries or duplicated runtimes.

## Why It Matters
- Modern software is polyglot, but FFI is fragile and slow.
- A unified IR and runtime enables cross-language inlining, optimization, and tooling.
- Developers can mix and match language features and libraries without friction.

## Success Criteria
- Two or more surface languages (e.g., Rustlite + PyLite) can call each other's functions and pass values without serialization or glue code.
- Type and ownership rules are respected across boundaries.
- Diagnostics and stack traces are unified.

## Key Steps
1. Define a cross-language ABI (layout, ownership, calling convention).
2. Implement a second surface language (PyLite or Ziglet) with macro lowering.
3. Build interop samples and tests (cross-calls, value passing, error propagation).
4. Integrate diagnostics and stack traces across surfaces.
5. Document the ABI and interop patterns for contributors.
