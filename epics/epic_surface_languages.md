# Epic: Surface Language Expansion

## Goal
Support multiple high-level language surfaces (Rustlite, PyLite, Ziglet, TSLite, etc.) that lower to the same core IR, enabling users to choose their preferred syntax and features.

## Why It Matters
- Developers can use the best tool for each job, mixing languages as needed.
- Surface languages can evolve independently while sharing a common backend.
- Encourages community contributions and experimentation.

## Success Criteria
- At least two surface languages with meaningful feature coverage.
- Shared test suite for surface-to-IR lowering.
- Interop samples demonstrating mixed-language modules.

## Key Steps
1. Define minimal viable feature sets for each new surface language.
2. Implement parsers and macro sets for each surface.
3. Build cross-language test and demo programs.
4. Document language-specific quirks and lowering patterns.
