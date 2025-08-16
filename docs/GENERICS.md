## Generics (Phase 4)

This project supports simple generic functions via a reader-macro pair:
- gfn: declares a generic template with type parameters
- gcall: requests a concrete instantiation and rewrites to a normal call

The compiler pipeline expands generics before type checking and IR emission, so examples typically just parse and emit.

### Syntax

- Generic function template
  (gfn :name "id" :generics [ T ] :ret T :params [ (param T %x) ] :body [ (ret T %x) ])

- Generic call (instantiation site)
  (gcall %dst <RetType> id :types [ <T-args> ] %args...)

The expander will:
- Clone the gfn body, substitute type parameters with the provided concrete types.
- Synthesize a mangled name for the specialization (e.g., id$T=i32).
- Emit a concrete (fn ...) at the template site and rewrite gcall → call using the mangled name.
- Deduplicate identical instantiations.

### Constraints and notes
- Type parameter identifiers are symbols inside :generics (e.g., T, U).
- All types in :types must be concrete EDN types the type checker understands.
- After expansion, normal call rules apply (explicit return type on call form, argument arity/typing, varargs as declared).
- You don’t need to call the expander manually; IREmitter runs it for you.

### Minimal example

(module
  (gfn :name "id" :generics [ T ] :ret T :params [ (param T %x) ]
       :body [ (ret T %x) ])
  (fn :name "main" :ret i32 :params [ (param i32 %a) ]
      :body [ (gcall %r i32 id :types [ i32 ] %a) (ret i32 %r) ]))

After expansion, this behaves like calling a specialized function id$T=i32 with signature (i32)->i32.
