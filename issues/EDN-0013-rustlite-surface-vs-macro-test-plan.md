# EDN-0013: Rustlite Surface vs Macro Test Plan (Condensed)

Status: In Progress (Surface coverage pass)
Date: 2025-08-29 (condensed update)

## Completed Since Last Condense
- Surface harness + normalization tool
- Arrays + indexing (read/write)
- Enum constructors
- ematch exhaustive + non-exhaustive diagnostic
- ematch payload/binding variants (placeholder bind inits)
- Shift operators (<< >>) baseline
- rtry (Result/Option) success path
- rwhile-let basic loop form
- Added samples & goldens: shift_ops, rtry_result, rtry_option, rwhile_let_basic, ematch_payloads
- Total samples: 46 (4 negative) ~84% of initial target (~55)

## Remaining Work
1. rfor-range tuple form tests (exclusive, inclusive, degenerate) + malformed range negatives
2. rtry negative wrong sum (E1603) and error-path sample
3. Shift compound assignments (<<=, >>=) test
4. Bounds index load flag samples (in-bounds + OOB negative)
5. Closure syntax & capture inference flag (on/off) + disabled negative
6. Tuple index OOB negative (E1601)
7. Additional syntax negatives: malformed range (0..), (..5), bad for header, unterminated block comment, keyword as identifier, missing semicolon, malformed while-let
8. Inclusive for-in semantics follow-up & identifier-based inclusive ranges (a..=b)
9. Docs update (RUSTLITE.md) with new features + contribution guidance
10. CI pipeline note/verification for surface layer

## Progress Metric
46 / ~55 (≈84%). Remaining items emphasize diagnostics, range/loop variants, feature toggles (closures, bounds), and compound shifts. Expected final count 55–58.

## Changelog Note (mirrors CHANGELOG.md additions)
Added Rustlite surface parsing features: ematch payload binds, shift ops, rtry, rwhile-let, expanded sample suite; updated test plan progress to 46/55.

