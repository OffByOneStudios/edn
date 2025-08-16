## Sum types and match (Phase 4)

Sum types are tagged unions with helper ops and a match form.
- sum: declares the type and its variants
- sum-new: construct a value of a variant
- sum-is: test a value's tag
- sum-get: read a field from a specific variant
- match: branch on the tag; optional result-as-value form that yields a value

The emitter represents a sum S as: struct S { i32 tag; [N x i8] payload; } with adequate payload size for the largest variant.

### Syntax

- Declaration
  (sum :name S :variants [ (variant :name A :fields [ i32 i64 ]) (variant :name B :fields [ i8 ]) ])

- Construct and get
  (sum-new %s S A [ %x %y ])
  (sum-is %ok S %s A)
  (sum-get %g0 S %s A 0)

- Match (statement-like)
  (match S %p
    :cases [ (case A [ ...body... ]) (case B [ ...body... ]) ]
    :default [ ... ])

- Match (result-as-value)
  (match %dst <T> S %p
    :cases [ (case A :binds [ (bind %x 0) ] :body [ ... ] :value %x)
             (case B :body [ ... ] :value %y) ]
    :default [ :body [ ... ] :value %z ])

### Constraints and notes
- Names in :name and :variants use symbols; :fields are types.
- sum-new takes a vector of values matching variant field types/arity.
- sum-get index must be in range for the variant; result type is that field.
- match requires exhaustiveness or a :default. Result form builds a PHI.

See tests in tests/phase4_sum_types_test.cpp for precise error codes and shapes.
