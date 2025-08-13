# EDN High-Level IR Phase 1 Plan

Goal: Establish a minimal, language-neutral EDN-based intermediate representation for C-like languages plus a lowering path toward LLVM IR (text or in-memory Module) using the existing generic macro + visitor framework.

## Scope (Phase 1 MVP)
Provide constructs sufficient to model (status in brackets):
- Modules, functions, parameters, locals (function list form present; richer block form pending)
- Primitive numeric + boolean types (implemented)
- Struct types & field access (implemented: struct collection + member instruction)
- Arrays & element access (implemented: array type + index instruction)
- Integer arithmetic (+,-,*,/) (implemented: add/sub/mul/sdiv instructions)
- Control flow: block, if, while (planned)
- Variables: definition (locals), assignment, return (return implemented; locals/assign pending)
- Loads/stores (implemented: load/store instructions)
- Pointer & array element address (index validated; future GEP multi-index planned)
- Calls (direct only) (planned)
- Metadata attachment for inferred instruction result types (:type-id) (implemented)

Out of scope for Phase 1 (defer):
- Generics / templates / monomorphization
- Virtual dispatch / interfaces / inheritance
- Exceptions / unwinding
- Move semantics / lifetime mgmt beyond trivial
- Vector types, SIMD, atomics
- Optimization passes
- Advanced type qualifiers (const/volatile/restrict) and alignment control beyond metadata placeholders

## IR Layering
1. HL-EDN (language flavored) – not required for MVP; we'll author directly in Core forms initially.
2. Core-EDN (canonical forms we define here)
3. Mid-EDN (SSA-ready; evaluation order explicit, temporaries introduced) – partial prototype later; not mandatory for first module emission.
4. LLVM Bridge – either textual pseudo IR (current example) or real LLVM objects via a pluggable backend.

## Core-EDN Forms (Phase 1 Subset)
```
(module :name <string>
  :decls [ <decl>* ])

; Declarations
(struct  :name <sym> :fields [ (:name <sym> :type <type>)* ])
(func    :name <sym>
         :type (fn-type :params [<type>*] :ret <type>)
         :params [ (:name <sym> :type <type>)* ]
         :body (block ...))
(global  :name <sym> :type <type> :init <expr>? :mutable? <bool>? :linkage <kw>? )  ; (maybe later)

; Types (nouns)
<iN> | f32 | f64 | void
(ptr :to <type>)
(array :elem <type> :size <int>)
(struct-ref <sym>) ; reference existing named struct
(fn-type :params [<type>*] :ret <type>)

; Statements / Expressions
(block :locals [(:name <sym> :type <type>)*] :stmts [ <stmt>* ])
(if    :cond <expr> :then (block ...) :else (block ...)? )
(while :cond <expr> :body (block ...))
(return :value <expr>?)
(assign :lhs <lvalue> :rhs <expr>)
(call   :fn <expr> :args [<expr>*])
(load   :addr <expr>)
(store  :addr <expr> :value <expr>)
(member :base <expr> :field <sym>) ; implemented (current instruction form)
(index  :base <expr> :idx <expr>)  ; implemented (pointer-to-array element)
(cast   :to <type> :expr <expr> :kind <sym>) (optional early, maybe just bitcast/inttoptr)
(binop  :op <sym> :lhs <expr> :rhs <expr>) ; arithmetic / comparisons
(unop   :op <sym> :arg <expr>) ; neg, not (optional)
(var    :name <sym>) ; variable reference
(lit    :type <type> :value <scalar>) ; typed literal
(addr-of :expr <lvalue>) ; produce pointer
```

### Value Categories
- lvalue forms: `var`, `member`, `index` (when used as :lhs or nested under addr-of). Distinction enforced by type-checker.
- rvalue forms: `lit`, `binop`, `unop`, `call`, `load`, etc.

## Passes (Phase 1)
1. Parse EDN -> raw AST (already available).
2. Structural validation & symbol table build
   - Ensure declared names unique per category (types, functions, globals) within module.
   - Attach metadata references: `:sym-id` or similar.
3. Type resolution
   - Map type forms to internal canonical `TypeId` (interned structure).
   - Annotate each expression node with `:type` metadata for later passes.
4. (Optional) Simple lvalue/rvalue check & lowering of convenience forms (e.g., implicit load for `var` when used in rvalue context) – might keep explicit for clarity.
5. Emission
   - Provide backend interface that the example uses to either:
     a) Produce textual pseudo LLVM IR (current), or
     b) Build real `llvm::Module` (once LLVM dependency is present) or JIT with ORC.

## Backend Abstraction Design
Introduce a lightweight IR emission context interface to decouple traversal from textual output:

```
struct IRBuilderBackend {
    virtual ~IRBuilderBackend() = default;
    virtual void beginModule(const ModuleInfo&) = 0;
    virtual void endModule() = 0;
    virtual void declareFunction(const FunctionDecl&) = 0; // optional separate decl
    virtual void beginFunction(const FunctionInfo&) = 0;
    virtual void emitInstruction(const Instruction&) = 0;  // generic instruction descriptor
    virtual void endFunction() = 0;
};
```

Phase 1 will implement `TextLLVMBackend` which internally mirrors current pseudo-text emission but through structured calls. Later `LLVMOrcBackend` will implement same interface using real LLVM IRBuilder / Module / ExecutionEngine.

### Data Carriers
Define small POD structs in a new header `edn/ir.hpp` (or `backend.hpp`) representing high-level events:
```
struct ModuleInfo { std::string id; std::string source; std::string triple; std::string dataLayout; };
struct ParamInfo { std::string name; std::string type; };
struct FunctionInfo { std::string name; std::string returnType; std::vector<ParamInfo> params; };
struct Instruction { std::string opcode; std::vector<std::string> operands; }; // simple for Phase 1
```

### Visitor Adaptation
Refactor `emit_module` pipeline: Instead of directly concatenating strings inside a lambda for "module", the visitor constructs `ModuleInfo`, calls `backend.beginModule(...)`, iterates functions, builds `FunctionInfo`, then each instruction becomes an `Instruction` passed to `backend.emitInstruction(...)`.

Backend then owns assembly of final text (or LLVM objects). For text backend, collect into `std::string` accessible via `str()`.

### JIT / ORC Readiness
- The backend interface isolates IR-building; swapping to real LLVM requires only implementing translation within `emitInstruction` mapping `(add %dst ty %a %b)` to IRBuilder operations.
- Ensure each instruction has sufficient typed information. Might extend `Instruction` to include `resultName`, `type`, and `kind` fields:
```
struct Instruction { std::string opcode; std::string result; std::string type; std::vector<std::string> args; };
```
- For constants, represent opcode "const" with type + literal, or treat them as separate instructions replaced by actual LLVM constants during emission.

## Type System Internal Representation
Define `enum class BaseType { I1,I8,I16,I32,I64,F32,F64,Void };` plus a `struct Type { enum Kind { Base, Pointer, Struct, Function, Array } ... }` (implemented). Supported parse forms: base, pointer, struct-ref, fn-type, array.

Provide `TypeId` alias (e.g., `uint32_t`) to store in metadata: store as `:type-name` string or embed numeric id encoded as int node for now.

## Validation Rules (Phase 1)
- Function parameter names unique within function.
- Local variable names unique within block (no shadowing initially to simplify).
- All referenced struct names must be declared before use (single pass or gather then resolve).
- Binary op operands must have identical primitive type (enforced for add/sub/mul/sdiv).
- load/store pointer pointee type must match value type.
- member base must be struct or pointer-to-struct matching field.
- index base must be pointer-to-array of elem type; index operand integer.
- Return statement value type must match function return type (unless void).

## Testing Strategy
Add tests under `tests/`:
1. Parse + to_string roundtrip for a minimal module form. (implicit via existing parser usage)
2. Type checker success: arithmetic, struct member, load/store, index. (implemented)
3. Type checker failure: missing :name, invalid member, invalid index/base, etc. (implemented)
4. Backend text emission equals expected (golden test pending)
5. Future: call instruction, control flow, assignment/lvalue tests.

## Directory Additions
- `include/edn/ir.hpp` (Phase 1 structs & backend interface) – header-only
- `examples/llvm_ir_emitter.cpp` refactor -> `examples/llvm_ir_emitter.cpp` uses new backend.
- Future: `src/` could be added if implementations grow beyond headers (currently header-only).

## Incremental Implementation Order
1. Add `ir.hpp` with backend interfaces + simple `TextLLVMBackend` implementation. (done)
2. Refactor existing example to use backend. (done)
3. Basic type system + arrays + interning. (done)
4. Type checker: structs, function headers, binops, const, ret, member, load/store, index, :type-id metadata. (done)
5. Backend emission golden test. (todo)
6. Add block/if/while + locals + assign. (todo)
7. Implement calls + fn-type argument checking. (todo)
8. Extend index to multi-dimensional / pointer arithmetic (future).
9. Propagate :type-id to any future expression nodes beyond current instruction subset. (ongoing)

## Example Usage After Refactor
```
TextLLVMBackend backend;
Transformer tr;
register_module_visitor(tr, backend); // sets up visitor(s) capturing module
auto ast = parse(edn_text);
tr.traverse(ast);
std::cout << backend.str();
```

## Open Questions / Future Work
- Decide whether to embed types as structured EDN nodes or attach canonical strings in metadata.
- Introduce SSA or rely on LLVM to fix up (naming collisions) – probably Phase 2.
- Macro system to facilitate syntactic sugar (e.g., while -> loop + branch) – design patterns upcoming.

---
Phase 1 delivers a pluggable backend path and stable core forms to build upon. This document should evolve with concrete type checker APIs as they are implemented.
