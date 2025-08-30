# Epic: Macro System & Surface Language Growth

## Goal
Design and evolve a macro system that allows new high-level language surfaces to be added by writing macro expansions, not by changing the core IR or backend.

## Why It Matters
- Macro-driven growth means new languages and features can be prototyped quickly.
- Keeps the core IR small, stable, and easy to reason about.
- Enables rapid iteration and experimentation with language features.

## Success Criteria
- Surface constructs (match, await, error union, etc.) are implemented as macros.
- Adding a new language surface requires only macro and parser work, not IR changes.
- Macro expansion is deterministic and testable (golden tests).

## Key Steps
1. Document macro authoring patterns and best practices.
2. Expand macro coverage for Rustlite (ranges, closures, error handling, etc.).
3. Prototype a new surface language using only macros.
4. Build a macro test suite (positive and negative cases).
5. Enable macro debugging and diagnostics for contributors.
