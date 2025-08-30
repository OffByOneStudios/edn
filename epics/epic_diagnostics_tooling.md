# Epic: Diagnostics & Tooling

## Goal
Deliver deterministic, structured diagnostics and developer tooling that work across all surface languages and the core IR.

## Why It Matters
- Consistent error reporting and debugging experience, regardless of language.
- Enables advanced editor integration, CI, and automated refactoring.
- Makes the system approachable for new contributors and users.

## Success Criteria
- All errors have stable codes, hints, and optional notes.
- Diagnostics can be exported as JSON for editor/CI integration.
- Tooling (formatters, linters, macro debuggers) works across surfaces.

## Key Steps
1. Expand and document error code registry.
2. Build negative test matrix for diagnostics.
3. Prototype editor/CI integration (JSON output, code actions).
4. Develop macro debugging and visualization tools.
