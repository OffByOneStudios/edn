# Enums and Exhaustive Matching (Rustlite Phase B)

Status: draft (E1600 diagnostic implemented)

This document describes the Rustlite surface for declaring enums (sum types), constructing variant values, and performing exhaustive matches with diagnostics.

## Declaring Enums

Macro: (enum :name Name :variants [ (Var) (Var TyA TyB ...) ... ])
Lowers to core (renum ...) which lowers to (sum ... (variant ...)). Each variant may list zero or more field types.

Example:
  (enum :name Opt2 :variants [ (None) (Some i32) ])

## Constructing Variants

Surface form (Type::Variant %dst [ payload* ]) is rewritten during a pre-walk to:
  (enum-ctor %dst Type Variant payload...)
Then lowered to core:
  (sum-new %dst Type Variant [ payload* ])

Manual constructor macro also available:
  (enum-ctor %dst Type Variant a b c)

Helpers for common shapes (Option / Result style):
  (rnone %dst Opt2) ; -> None
  (rsome %dst Opt2 %val)
  (rok %dst Res %val)
  (rerr %dst Res %err)

## Matching Variants

Core form (match ...) already existed for sum types. New macro (ematch ...) provides an exhaustive-oriented surface:
  (ematch %dst RetTy EnumType %value :arms [ (arm Variant :binds [ %a %b ] :body [ ... :value %ret ]) ... ])

Behavior:
- Expands to core (match %dst RetTy EnumType %value :cases [ (case ...) ... ])
- If every declared variant of EnumType is covered exactly once, no :default is emitted.
- If one or more variants are missing, the macro still emits the core match without :default and tags metadata { ematch: true }.

## Non-Exhaustive Diagnostic (E1600)

During type checking, a (match ...) tagged with metadata key "ematch" that lacks both a :default and total variant coverage triggers diagnostic E1600:
  code: E1600
  message: non-exhaustive enum match
  help: add missing variant arms or provide :default

This replaces the generic E1415 (match missing :default) for ematch-generated forms, enabling clearer guidance.

## Exhaustiveness Rules

A match is considered exhaustive when the set of distinct variant names appearing in its cases equals the total variant count recorded at enum declaration. Duplicate variants inside the same match produce E1414 before exhaustiveness is evaluated.

## Variant Field Binding

Within (arm Variant ...) you can list :binds [ %x %y ] to bind each payload field positionally. Expansion converts this list to core (bind %x index) entries associated with the case.

Example with payload binding:

```
(enum :name PairOrUnit :variants [ (Unit) (Pair i32 i32) ])
(ematch %sum i32 PairOrUnit %val :arms [
  (arm Unit :body [ (const %z i32 0) :value %z ])
  (arm Pair :binds [ %a %b ] :body [ (add %tmp i32 %a %b) :value %tmp ])
])
```
The second arm binds the two i32 payload fields to %a and %b, then produces their sum.

## Result Mode

If %dst and a RetTy appear as the first two operands, match operates in result mode, requiring each case body to include ":value %var" specifying the produced value. Type and presence are validated (E1421..E1423).

## Examples

Exhaustive:
  (enum :name Tri :variants [ (A) (B) (C) ])
  (ematch %out i32 Tri %t :arms [
    (arm A :body [ (const %z i32 0) :value %z ])
    (arm B :body [ (const %o i32 1) :value %o ])
    (arm C :body [ (const %t2 i32 2) :value %t2 ]) ])

Non-exhaustive (produces E1600):
  (enum :name Opt2 :variants [ (None) (Some i32) ])
  (ematch %out i32 Opt2 %o :arms [ (arm None :body [ (const %z i32 0) :value %z ]) ])

Add a default instead of covering all variants:
  (ematch %out i32 Opt2 %o :arms [ (arm None :body [ :value %zero ]) ] :default (default :body [ (const %d i32 1) :value %d ]))

## Legacy Core Matches

Plain (match ...) forms produced directly (not via ematch) continue to use E1415 when missing a :default and not exhaustive.

## ematch vs rmatch Summary

| Aspect | ematch | rmatch |
|--------|--------|--------|
| Intent | Enforce exhaustiveness for enums | General sum match with explicit else |
| Default handling | Omitted when exhaustive; omitted + non-exhaustive => E1600 | Requires :else (vector) always; missing/empty else leads to E1415 / related value diagnostics |
| Diagnostic on non-exhaustive | E1600 (enum focused) | E1415 (generic missing default) |
| Metadata tag | ematch (+ ematch-exhaustive when complete) | none |
| Variant coverage check | Based on renum-declared variant count | Not performed (relies on else) |
| Payload binds | :binds [ %sym* ] same semantics | :binds [ %sym* ] |
| Use when | You want compiler help listing missing variants | You want a catch-all or partial handling with default |

## Future Work
- Payload pattern destructuring sugar (nested matches / guard conditions)
- Auto-synthesized variant value constructors with type inference
- IDE hints listing missing variants

