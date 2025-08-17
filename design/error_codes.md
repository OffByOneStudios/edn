# EDN Error Codes Reference

Status: Initial extraction on 2025-08-14. Keep this file in sync when adding / renaming diagnostics.

## Conventions
- Format: Code | Short Title | When Emitted | Typical Hint / Notes
- Short Title mirrors the internal `error_code` short message (2nd argument) for traceability.
- Unused / reserved codes inside an allocated block are listed as **(reserved)** to prevent accidental reuse with unintended semantics.
- Suggestions: Some errors (`E0803` unknown struct field, `E0804` base undefined, `E0407` unknown arg var, `E0902` unknown global) may append suggestion notes (edit-distance ranked). Suggestions are ENABLED by default; set environment variable `EDN_SUGGEST=0` to suppress them. Any other non-`0` value (or unset) keeps them on.
- Deprecations: Legacy integer comparison ops (`eq ne lt gt le ge`) still emit `E010x` errors; a warning may be optionally produced when `EDN_WARN_DEPRECATED=1`.

## Ranges Overview
| Range    | Category                               |
|----------|----------------------------------------|
| E000x    | Integer binary arithmetic (add/sub/…)   |
| E010x    | Legacy integer comparisons              |
| E011x    | `icmp` predicate form                   |
| E012x    | `fcmp` predicate form                   |
| E020x    | `load`                                  |
| E021x    | `store`                                 |
| E030x    | `phi`                                   |
| E040x    | `call`                                  |
| E050x    | Casts                                   |
| E060x    | Bit / logical & shifts                  |
| E070x    | Float binary arithmetic                 |
| E080x    | Struct member access (`member`)         |
| E081x    | Struct member address (`member-addr`)   |
| E082x    | Array indexing (`index`)                |
| E090x    | Global loads / stores (`gload/gstore`)  |
| E130x    | Pointer arithmetic (`ptr-add/sub/diff`) |
| E131x    | Address-of / deref (`addr` / `deref`)   |
| E132x    | Function pointers / indirect call       |
| E133x    | Typedef aliases                         |
| E134x    | Enum declarations & constants           |
| E135x    | Union declarations & access             |
| E136x    | Variadics & vararg intrinsics           |
| E137x    | For loop construct                      |
| E138x    | Continue statement                      |
| E139x    | Switch construct                        |
| E13Ax    | Cast sugar `(as ...)`                   |
| E143x    | Closures & captures `(closure ...)`      |

| E145x    | Try/Catch construct                       |

---

## Detailed Codes

### E000x – Integer Binary Arithmetic
| Code  | Title                        | When / Condition                                                                                   | Hint |
|-------|------------------------------|----------------------------------------------------------------------------------------------------|------|
| E0001 | binop arity                  | Wrong number of list elements for integer binop                                                    | expected form `(op %dst <int-type> %a %b)` |
| E0002 | binop symbol expected        | Missing `%` or empty symbols for dst / operands                                                    | Use `%` prefixes |
| E0003 | binop type must be integer   | Annotated type not an integer kind                                                                 | Choose i1/i8/i16/i32/i64/u8/u16/u32/u64 |
| E0004 | lhs type mismatch            | LHS var type differs from annotated type                                                           | Match annotated type |
| E0005 | rhs type mismatch            | RHS var type differs from annotated type                                                           | Match annotated type |
| E0006 | redefinition of variable     | Destination SSA name already defined                                                               | Use fresh `%name` |
| E0007 | dest must be %var            | Destination missing `%` prefix                                                                     | Prepend `%` |

### E010x – Legacy Integer Comparisons
| Code  | Title                         | Condition                                                                                          | Hint |
|-------|-------------------------------|-----------------------------------------------------------------------------------------------------|------|
| E0100 | cmp arity                     | Wrong number of elements                                                                            | `(cmp %dst <int-type> %a %b)` form |
| E0101 | cmp symbol expected           | Missing symbols / `%` prefixes                                                                     | Use `%dst %lhs %rhs` |
| E0102 | cmp operand type must be int  | Annotated comparison type not integer                                                              | Use integer base type |
| E0103 | cmp operand type mismatch     | Operand variable type differs from annotated                                                       | Make operands same type |
| E0104 | operand must be %var          | Operand literal / missing `%`                                                                      | Prefix with `%` |
| E0105 | redefinition of variable      | Destination already defined                                                                        | Rename dest |
| E0106 | dest must be %var             | Destination missing `%`                                                                            | Prefix with `%` |

### E011x – `icmp`
| Code  | Title                         | Condition                                                                                          | Hint |
|-------|-------------------------------|-----------------------------------------------------------------------------------------------------|------|
| E0110 | icmp arity                    | Wrong arity                                                                                         | `(icmp %dst <int-type> :pred <pred> %a %b)` |
| E0111 | icmp dst must be %var         | Destination missing `%`                                                                            | Prefix destination |
| E0112 | icmp operand type must be int | Annotated type not integer                                                                         | Choose integer type |
| E0113 | icmp expects :pred keyword    | Missing `:pred` keyword                                                                             | Insert `:pred` before predicate |
| E0114 | unknown icmp predicate        | Predicate not in allowed set                                                                        | Use eq/ne/slt/sgt/sle/sge/ult/ugt/ule/uge |
| E0115 | icmp operands required        | Missing operand symbols                                                                             | Supply `%lhs %rhs` |
| E0116 | icmp operand type mismatch    | Operand variable type differs from annotated                                                       | Match annotated type |
| E0117 | icmp operand must be %var     | Operand missing `%` prefix                                                                         | Prefix operand |
| E0118 | redefinition of variable      | Destination previously defined                                                                     | Rename dest |

### E012x – `fcmp`
| Code  | Title                          | Condition                                                                                          | Hint |
|-------|--------------------------------|-----------------------------------------------------------------------------------------------------|------|
| E0120 | fcmp arity                     | Wrong arity                                                                                         | `(fcmp %dst <float-type> :pred <pred> %a %b)` |
| E0121 | fcmp dst must be %var          | Destination missing `%`                                                                            | Prefix destination |
| E0122 | fcmp operand type must be float| Annotated type not f32/f64                                                                         | Use f32 or f64 |
| E0123 | fcmp expects :pred keyword     | Missing `:pred`                                                                                     | Insert `:pred` |
| E0124 | unknown fcmp predicate         | Predicate not recognized                                                                            | Use oeq/one/olt/ogt/ole/oge/ord/uno/ueq/une/ult/ugt/ule/uge |
| E0125 | fcmp operands required         | Missing `%lhs` or `%rhs`                                                                           | Supply both operands |
| E0126 | fcmp operand type mismatch     | Operand variable type differs from annotated                                                       | Match annotated type |
| E0127 | fcmp operand must be %var      | Operand lacks `%`                                                                                  | Prefix operand |
| E0128 | redefinition of variable       | Destination already defined                                                                        | Rename dest |

### E020x – load
| Code  | Title                 | Condition                                                       | Hint |
|-------|-----------------------|------------------------------------------------------------------|------|
| E0200 | load arity            | Wrong arity                                                      | `(load %dst <type> %ptr)` |
| E0201 | load symbols          | Missing `%dst` or `%ptr`                                         | Provide both |
| E0202 | ptr undefined         | Pointer var not defined                                          | Define earlier |
| E0203 | ptr not pointer to type| Pointer not matching `(ptr <type>)`                              | Use correct pointer type |
| E0204 | load dst must be %var | Destination missing `%`                                          | Prefix destination |

### E021x – store
| Code  | Title                     | Condition                                                      | Hint |
|-------|---------------------------|----------------------------------------------------------------|------|
| E0210 | store arity               | Wrong arity                                                    | `(store <type> %ptr %val)` |
| E0211 | store symbols             | Missing pointer or value symbol                               | Provide both |
| E0212 | ptr undefined             | Pointer var not defined                                        | Define earlier |
| E0213 | store ptr type mismatch   | Pointer pointee != annotated type                              | Match `<type>` |
| E0214 | store value type mismatch | Value var type != annotated type                               | Match `<type>` |

### E030x – phi
| Code  | Title                          | Condition                                                     | Hint |
|-------|--------------------------------|---------------------------------------------------------------|------|
| E0300 | phi arity                      | Wrong arity                                                   | `(phi %dst <type> [ (%v %label) ... ])` |
| E0301 | phi dst must be %var           | Destination missing `%`                                       | Prefix destination |
| E0302 | redefinition of variable       | Destination already defined                                   | Rename phi dest |
| E0303 | phi incoming list must be vector| Third element not a vector                                  | Wrap pairs in `[...]` |
| E0304 | phi requires at least two incoming values | Fewer than 2 incoming pairs                       | Provide >=2 pairs |
| E0305 | phi incoming entry must be list| Inner element not a list                                     | Use `(%val %label)` |
| E0306 | phi incoming entry arity       | Not exactly 2 elements                                        | Fix form |
| E0307 | phi incoming expects (%val %label)| Types in pair not two symbols                           | Provide `%val %label` |
| E0308 | phi incoming value must be %var| Value missing `%`                                            | Prefix value |
| E0309 | phi incoming value type mismatch| Value type inconsistent                                      | Match phi type |

### E040x – call
| Code  | Title                      | Condition                                                      | Hint |
|-------|----------------------------|----------------------------------------------------------------|------|
| E0400 | call arity                 | Too few elements                                               | `(call %dst <ret-type> name %args...)` |
| E0401 | call dst must be %var      | Destination missing `%`                                        | Prefix destination |
| E0402 | call callee symbol required| Callee name missing                                            | Provide function name |
| E0403 | unknown callee             | Function not found                                             | Define function first |
| E0404 | call return type mismatch  | Annotated ret type != callee decl                              | Use declared return type |
| E0405 | call arg count mismatch    | Provided arg count != expected                                 | Match arity |
| E0406 | call arg must be %var      | Argument missing `%`                                           | Prefix each arg |
| E0407 | unknown arg var            | Argument SSA var undefined                                     | Define earlier |
| E0408 | call arg type mismatch     | Argument type != parameter type                                | Match declared param |

### E050x – Casts
Codes cover multiple cast ops (zext, sext, trunc, bitcast, sitofp, uitofp, fptosi, fptoui, ptrtoint, inttoptr) in common handler.
| Code  | Title                        | Condition                                                     | Hint |
|-------|------------------------------|---------------------------------------------------------------|------|
| E0500 | <op> arity                   | Wrong number of elements                                      | `(op %dst <to-type> %src)` |
| E0501 | <op> src must be %var        | Source missing `%`                                            | Prefix source |
| E0502 | unknown cast source var      | Source var undefined                                          | Define source earlier |
| E0503 | (reserved)                   | Not used yet                                                  | — |
| E0504 | cast dst must be %var        | Destination missing `%`                                       | Prefix destination |
| E0505 | redefinition of variable     | Destination already defined                                   | Rename destination |
| E0506 | (reserved)                   | Not used yet                                                  | — |
| E0507 | (reserved)                   | Not used yet                                                  | — |
| E0508 | invalid cast combination     | Helper `bad(...)` for invalid (from,to) semantics             | Adjust cast path |

### E060x – Bit / Logical / Shift
| Code  | Title                          | Condition                                                    | Hint |
|-------|--------------------------------|--------------------------------------------------------------|------|
| E0600 | bit/logical op arity           | Wrong arity                                                  | `(op %dst <int-type> %a %b)` |
| E0601 | bit/logical expects symbols    | Missing dst or operands                                      | Provide `%dst %lhs %rhs` |
| E0602 | bit/logical op type must be integer | Annotated type not integer                               | Use integer base type |
| E0603 | operand type mismatch          | Operand var type mismatch                                    | Match annotated type |
| E0604 | operand must be %var           | Operand missing `%`                                          | Prefix operand |
| E0605 | redefinition of variable       | Destination already defined                                  | Rename destination |
| E0606 | dest must be %var              | Destination missing `%`                                      | Prefix destination |

### E070x – Float Binary Arithmetic
| Code  | Title                       | Condition                                                     | Hint |
|-------|-----------------------------|---------------------------------------------------------------|------|
| E0700 | fbinop arity                | Wrong arity                                                   | `(fadd %dst <float-type> %a %b)` etc |
| E0701 | fbinop symbol expected       | Missing `%` or symbols                                       | Use `%` prefixes |
| E0702 | fbinop type must be f32/f64  | Annotated type not f32/f64                                   | Choose f32 or f64 |
| E0703 | lhs type mismatch            | LHS var type differs                                         | Match annotated type |
| E0704 | rhs type mismatch            | RHS var type differs                                         | Match annotated type |
| E0705 | redefinition of variable     | Destination already defined                                  | Rename destination |
| E0706 | dest must be %var            | Destination missing `%`                                      | Prefix destination |

### E080x – Struct Member Access (`member`)
| Code  | Title                             | Condition                                                    | Hint / Notes |
|-------|-----------------------------------|--------------------------------------------------------------|--------------|
| E0800 | member arity                      | Wrong arity                                                  | `(member %dst Struct %base %field)` |
| E0801 | member expects symbols            | Missing one or more symbols                                  | Supply all symbols |
| E0802 | unknown struct in member          | Struct not declared                                          | Declare struct first |
| E0803 | unknown field                     | Field name not found (suggestions may be added)              | Check field spelling |
| E0804 | base undefined                    | Base var not defined                                         | Define base earlier |
| E0805 | base not struct or pointer to struct | Base type invalid                                          | Use struct value or pointer |
| E0806 | base must be %var                 | Base missing `%`                                             | Prefix base |
| E0807 | member dst must be %var           | Destination missing `%`                                      | Prefix destination |
| E0808 | redefinition of variable          | Destination already defined                                  | Rename destination |

### E081x – Struct Member Address (`member-addr`)
| Code  | Title                                   | Condition                                                   | Hint |
|-------|-----------------------------------------|-------------------------------------------------------------|------|
| E0810 | member-addr arity                       | Wrong arity                                                 | `(member-addr %dst Struct %base %field)` |
| E0811 | member-addr expects symbols             | Missing symbols                                             | Provide all |
| E0812 | unknown struct in member-addr           | Struct not declared                                         | Declare struct first |
| E0813 | unknown field                           | Field name not in struct                                    | Check field name |
| E0814 | base undefined                          | Base var not defined                                        | Define base earlier |
| E0815 | member-addr base must be pointer to struct | Base not pointer-to-struct                              | Pass pointer (e.g. param (ptr (struct-ref S))) |
| E0816 | base must be %var                       | Base missing `%`                                            | Prefix base |
| E0817 | member-addr dst must be %var            | Destination missing `%`                                     | Prefix destination |
| E0818 | redefinition of variable                | Destination already defined                                 | Rename destination |

### E082x – Array Index (`index`)
| Code  | Title                               | Condition                                                   | Hint |
|-------|-------------------------------------|-------------------------------------------------------------|------|
| E0820 | index arity                         | Wrong arity                                                 | `(index %dst <elem-type> %basePtr %idx)` |
| E0821 | index symbols                       | Missing `%dst/%base/%idx`                                   | Supply all |
| E0822 | base undefined                      | Base var not defined                                        | Define base earlier |
| E0823 | index base not pointer              | Base not a pointer type                                     | Use pointer to array |
| E0824 | base not pointer to array elem type | Pointer pointee not `(array :elem <elem-type>)` or elem mismatch | Ensure pointer matches array elem type |
| E0825 | index must be int                   | Index var type not integer                                  | Use integer index |
| E0826 | index dst must be %var              | Destination missing `%`                                     | Prefix destination |
| E0827 | redefinition of variable            | Destination already defined                                 | Rename destination |

### E090x – Globals (`gload` / `gstore`)
| Code  | Title                            | Condition                                                   | Hint / Notes |
|-------|----------------------------------|-------------------------------------------------------------|--------------|
| E0900 | gload arity                      | Wrong arity                                                 | `(gload %dst <type> GlobalName)` |
| E0901 | gload symbols                    | Missing `%dst` or global name                               | Provide both |
| E0902 | unknown global                   | Global not declared (suggestions may be added)              | Declare `(global :name ...)` first |
| E0903 | gload type mismatch              | Annotated type != declared global type                      | Use global's declared type |
| E0904 | gload dst must be %var           | Destination missing `%`                                     | Prefix destination |
| E0905 | redefinition of variable         | Destination already defined                                 | Rename destination |
| E0910 | gstore arity                     | Wrong arity                                                 | `(gstore <type> GlobalName %val)` |
| E0911 | gstore symbols                   | Missing global name or value var                            | Provide both |
| E0912 | unknown global                   | Global not declared                                         | Declare first |
| E0913 | gstore type mismatch             | Annotated type != declared global type                      | Match global declaration |
| E0914 | gstore value type mismatch       | Value type != annotated type                                | Match `<type>` |
| E0915 | gstore value must be %var        | Value missing `%`                                           | Prefix value |

### E130x – Pointer Arithmetic
Forms:
1. `(ptr-add %dst (ptr <T>) %base %offset)`
2. `(ptr-sub %dst (ptr <T>) %base %offset)`
3. `(ptr-diff %dst <int-type> %a %b)` -> number of elements (signed division by sizeof(T)).

| Code  | Title                               | Condition                                                     | Hint |
|-------|-------------------------------------|----------------------------------------------------------------|------|
| E1300 | ptr-add/ptr-sub arity               | Wrong arity for add/sub pointer op                            | Use `(ptr-add %dst (ptr <T>) %base %offset)` |
| E1301 | ptr-add/ptr-sub dst must be %var    | Destination missing `%`                                       | Prefix destination |
| E1302 | ptr base must be %var               | Base symbol missing `%`                                       | Prefix base |
| E1303 | ptr base type mismatch              | Base undefined or not same pointer type as annotation         | Ensure `%base` has type `(ptr <T>)` |
| E1304 | ptr offset must be %var int         | Offset missing `%` or not integer variable                    | Provide integer offset var |
| E1305 | redefinition of variable            | Destination already defined                                   | Rename destination |
| E1306 | ptr-diff arity                      | Wrong arity for `(ptr-diff ...)`                              | Use `(ptr-diff %dst <int-type> %a %b)` |
| E1307 | ptr-diff dst must be %var           | Destination missing `%`                                       | Prefix destination |
| E1308 | ptr-diff result/operands invalid    | Result type not integer OR operands missing `%`               | Use integer result and `%a %b` |
| E1309 | ptr-diff pointer type mismatch      | Operands not same pointer type                                | Use two pointers to same element type |

### E131x – Address-of / Deref
| Code  | Title                            | Condition                                                    | Hint |
|-------|----------------------------------|--------------------------------------------------------------|------|
| E1310 | addr arity                      | Wrong arity                                                 | `(addr %dst (ptr <T>) %src)` |
| E1311 | addr dst must be %var           | Destination missing `%`                                     | Prefix destination |
| E1312 | addr annotation must be pointer | Second operand not pointer type annotation                  | Use `(ptr <elem>)` |
| E1313 | addr source must be %var        | Source missing `%`                                          | Prefix source |
| E1314 | addr source undefined           | Source variable not defined                                 | Define source earlier |
| E1315 | addr type mismatch              | Annotation pointee != source value type                     | Match pointee to source type |
| E1316 | redefinition of variable        | Destination already defined                                 | Rename destination |
| E1317 | deref arity                     | Wrong arity                                                 | `(deref %dst <type> %ptr)` |
| E1318 | deref dst must be %var          | Destination missing `%`                                     | Prefix destination |
| E1319 | deref ptr type mismatch         | Pointer var undefined / not pointer to annotated type       | Ensure pointer has `(ptr <type>)` |

### E132x – Function Pointers / Indirect Call
| Code  | Title                               | Condition                                                    | Hint |
|-------|-------------------------------------|--------------------------------------------------------------|------|
| E1320 | call-indirect arity                 | Too few elements                                             | `(call-indirect %dst <ret-type> %fptr %args...)` |
| E1321 | call-indirect dst must be %var      | Destination missing `%`                                      | Prefix destination |
| E1322 | call-indirect fptr must be %var     | Function pointer symbol missing `%`                          | Prefix pointer |
| E1323 | call-indirect fptr undefined/not fn | Pointer var undefined or not pointer to function             | Define fn pointer earlier / use (ptr (fn-type ...)) |
| E1324 | call-indirect return type mismatch  | Annotated return type differs from function pointer type     | Match function pointer ret type |
| E1325 | call-indirect arg count mismatch    | Provided arg count != expected                               | Match parameter count |
| E1326 | call-indirect arg must be %var      | Argument missing `%`                                         | Prefix each arg |
| E1327 | call-indirect arg type mismatch     | Argument type != parameter type                              | Match param type |
| E1328 | redefinition of variable            | Destination already defined                                  | Rename destination |
| E1329 | fnptr arity / annotation issue      | Wrong arity or malformed annotation                          | `(fnptr %dst (ptr (fn-type ...)) Name)` |

### E133x – Typedef Aliases
Form: `(typedef :name Alias :type <type-form>)`

| Code  | Title                            | Condition                                                    | Hint |
|-------|----------------------------------|--------------------------------------------------------------|------|
| E1330 | typedef missing :name            | :name keyword absent or Alias not symbol/string              | provide `(typedef :name Alias :type <type>)` |
| E1331 | typedef missing :type            | :type keyword absent                                          | add `:type <type-form>` |
| E1332 | typedef redefinition             | Alias already defined (typedef or struct)                    | choose unique alias name |
| E1333 | typedef type form invalid        | Underlying type parse error                                  | fix underlying type form |

### E134x – Enums
Form: `(enum :name Color :underlying i32 :values [ (eval :name RED :value 0) (eval :name BLUE :value 1) ])`
Each `(eval ...)` entry requires :name and :value (integer literal) and becomes a constant symbol of underlying type.

| Code  | Title                         | Condition                                         | Hint |
|-------|-------------------------------|---------------------------------------------------|------|
| E1340 | enum missing :name            | No :name or invalid enum name                     | provide `(enum :name E ...)` |
| E1341 | enum missing :underlying      | No :underlying keyword                            | add `:underlying <int-type>` |
| E1342 | enum underlying not integer   | Underlying type not integer base                  | choose integer base type |
| E1343 | enum missing :values          | No :values vector                                 | add `:values [ (eval ...) ]` |
| E1344 | enum value entry malformed    | `(eval ...)` entry missing :name or :value        | ensure each entry has both |
| E1345 | enum duplicate constant       | Repeated constant name within enum or other enum  | rename constant |
| E1346 | enum constant value not int   | :value not integer literal                        | use integer literal |
| E1347 | enum redefinition             | Enum name already used (enum/struct/typedef)      | choose unique enum name |
| E1348 | enum constant global clash    | Enum constant collides with existing global/var   | rename constant |
| E1349 | unknown enum constant         | Use of enum constant symbol not declared         | check constant spelling (suggestions may appear) |

### E135x – Unions
Form: `(union :name U :fields [ (ufield :name a :type i32) (ufield :name b :type (ptr i8)) ])`

| Code  | Title                        | Condition                                         | Hint |
|-------|------------------------------|---------------------------------------------------|------|
| E1350 | union missing :name          | No :name keyword / invalid name                   | provide `(union :name U ...)` |
| E1351 | union missing :fields        | No :fields vector                                 | add `:fields [ (ufield :name a :type i32) ... ]` |
| E1352 | union field malformed        | Field entry missing name/type or wrong shape      | use `(ufield :name a :type i32)` |
| E1353 | union field type invalid     | Field :type parse error                           | fix field :type form |
| E1354 | union field type unsupported | Non-base / non-pointer field (aggregate)          | restrict to base or pointer types initially |
| E1355 | union duplicate field        | Repeated field name                               | rename field |
| E1356 | union redefinition           | Union name already used                           | choose unique union name |
| E1357 | union-member arity           | `(union-member ...)` malformed / wrong arity      | use `(union-member %dst Union %ptr field)` |
| E1358 | unknown union                | Referenced union not declared / base var undefined| declare union / define base variable earlier |
| E1359 | unknown union field          | Field name not in union                           | check field spelling |

Reserved: future active-member tracking diagnostics.

### E136x – Variadic Functions & Intrinsics
Forms:
`(fn :name "f" :ret <type> :params [ (param <type> %a) ... ] :vararg true :body [ ... ])`

Calls use existing `(call %dst <ret-type> name %arg...)` form. Fixed parameters are validated for count & type; extra variadic arguments are only required to be previously defined `%` vars (future work may add default promotions / classification).

Variadic intrinsics (current lightweight model – `va_list` represented as `i8*`):
- `(va-start %ap)` defines a new `%ap` of type `i8*` (must appear inside a variadic function body).
- `(va-arg %dst <type> %ap)` claims the next variadic argument as `<type>` and defines `%dst`. (Emitter currently produces `undef` placeholder until real lowering is implemented.)
- `(va-end %ap)` marks the end of traversal (no-op now; kept for parity / future safety checks).

| Code  | Title                          | Condition / When Emitted                               | Hint |
|-------|--------------------------------|--------------------------------------------------------|------|
| E1360 | variadic missing required args | Provided args < declared fixed param count             | supply all fixed params first |
| E1361 | variadic arg must be %var      | Extra variadic arg not a `%` symbol                    | prefix each variadic arg with % |
| E1362 | variadic arg undefined         | Variadic arg variable not defined earlier              | define argument value earlier |
| E1363 | va-start arity                 | Wrong form / missing `%ap` / duplicate destination     | expected `(va-start %ap)` |
| E1364 | va-start only in variadic fn   | `(va-start ...)` used in non-variadic function         | declare function with `:vararg true` |
| E1365 | va-arg arity                   | Wrong form / `%dst` missing / `%dst` already defined   | expected `(va-arg %dst <type> %ap)` |
| E1366 | va-arg only in variadic fn     | `(va-arg ...)` used in non-variadic function           | declare function with `:vararg true` |
| E1367 | va-arg ap must be %var         | `%ap` missing `%`, undefined, or not of type `i8*`     | pass previously defined `%ap` (type i8*) |
| E1368 | va-end arity                   | Wrong form / `%ap` missing `%` / `%ap` undefined       | expected `(va-end %ap)` |
| E1369 | va-end only in variadic fn     | `(va-end ...)` used in non-variadic function           | declare function with `:vararg true` |

Notes:
- Current implementation does not yet materialize a true argument cursor; `(va-arg ...)` yields an `undef` of the requested type in IR (placeholder). Future implementation will load promoted values from the underlying ABI argument area and advance `%ap`.
- Suggestion logic (edit-distance) applies to `E1362` similar to other unknown var errors when enabled via `EDN_SUGGEST`.

### E13Ax – Cast Sugar `(as ...)`
Form: `(as %dst <to-type> %src)` chooses an underlying concrete cast opcode (zext, sext, trunc, bitcast, sitofp, uitofp, fptosi, fptoui, ptrtoint, inttoptr) based on source/target types.

| Code   | Title                    | Condition / When Emitted                        | Hint |
|--------|--------------------------|-------------------------------------------------|------|
| E13A0  | as arity                 | Wrong number of elements                        | use `(as %dst <to-type> %src)` |
| E13A1  | as dst must be %var      | Destination missing `%`                         | prefix destination with % |
| E13A2  | as src must be %var      | Source missing `%`                              | prefix source with % |
| E13A3  | as unknown src var       | Source variable undefined                       | define source earlier |
| E13A4  | as unsupported conversion| No valid canonical cast between the two types   | adjust source/target types or insert explicit form |
| E13A5  | redefinition of variable | Destination already defined                     | rename destination |

### E137x – For Loop
Form: `(for :init [ ... ] :cond %c :step [ ... ] :body [ ... ])` Keywords may appear in any order; all are required.

| Code  | Title                  | Condition                                   | Hint |
|-------|------------------------|---------------------------------------------|------|
| E1370 | (reserved)             | —                                           | — |
| E1371 | for missing :init      | Missing `:init` section                     | add `:init []` (can be empty) |
| E1372 | for missing :cond      | Missing `:cond %var` or symbol lacks `%`    | provide condition `%var` of type i1 |
| E1373 | for cond must be i1    | Condition variable not boolean i1          | ensure condition var has type i1 |
| E1374 | for missing :body      | Missing `:body [ ... ]` block               | add body vector |
| E1375 | for missing :step      | Missing `:step [ ... ]` block               | add step vector (can be empty) |
| E1376 | (reserved)             | —                                           | — |
| E1377 | (reserved)             | —                                           | — |
| E1378 | (reserved)             | —                                           | — |
| E1379 | (reserved)             | —                                           | — |

### E138x – Continue
Form: `(continue)` – valid only inside `while` or `for` bodies.

| Code  | Title                       | Condition                             | Hint |
|-------|-----------------------------|---------------------------------------|------|
| E1380 | continue outside loop       | Used with no active loop context      | place inside while/for body |
| E1381 | continue takes no operands  | Extra list elements after `continue`  | remove operands |
| E1382 | (reserved)                  | —                                     | — |
| E1383 | (reserved)                  | —                                     | — |
| E1384 | (reserved)                  | —                                     | — |
| E1385 | (reserved)                  | —                                     | — |
| E1386 | (reserved)                  | —                                     | — |
| E1387 | (reserved)                  | —                                     | — |
| E1388 | (reserved)                  | —                                     | — |
| E1389 | (reserved)                  | —                                     | — |

### E139x – Switch
Form: `(switch %expr :cases [ (case <int> [ ... ])* ] :default [ ... ])`

| Code  | Title                     | Condition                                      | Hint |
|-------|---------------------------|------------------------------------------------|------|
| E1390 | switch missing expr       | No `%expr` symbol after opcode                 | supply `(switch %var ...)` |
| E1391 | switch expr must be %var  | Expression not a `%` variable                  | prefix variable with `%` |
| E1392 | switch expr must be int   | Expression variable not integer base type      | use integer base type |
| E1393 | switch missing :cases     | Omitted `:cases` section                       | add `:cases [ (case <int> [ ... ])* ]` |
| E1394 | switch cases must be vector | `:cases` value not a vector                  | wrap cases in `[ ]` |
| E1395 | switch case malformed     | Case entry not `(case <int> [ ... ])`          | fix case form |
| E1396 | switch duplicate case     | Repeated integer constant across cases         | remove duplicate |
| E1397 | switch missing :default   | No `:default` section or malformed vector      | add `:default [ ... ]` |
| E1398 | (reserved)                | —                                              | — |
| E1399 | (reserved)                | —                                              | — |

---

## Maintenance Process
1. When introducing a new diagnostic:
   - Allocate next code in the appropriate range (or create a new range block in increments of 100 if a new category emerges).
   - Add an entry here with: Code, Title (short message), When / Condition, Hint.
   - If adding suggestion logic, note it in the Hint / Notes column.
2. If deprecating a code, keep the line and append `(deprecated)` to its Title; do NOT reuse the code.
3. If reserving codes for future expansion, list them with `(reserved)`.
4. Keep ordering numeric for easy scanning.

## Environment Flags
| Variable        | Effect |
|-----------------|--------|
| `EDN_SUGGEST`   | If set to `0`, suppresses suggestion notes for eligible errors (`E0407`, `E0803`, `E0804`, `E0902`). Any other value or unset => suggestions emitted. |
| `EDN_WARN_DEPRECATED` | If set to `1`, emits a warning note for use of legacy integer comparison op forms. |

## Planned Enhancements
- Add secondary notes for type mismatches (expected vs actual) – codes unaffected, only richer notes.
- Potential JSON export script (future) to enable editor integrations / LSP.

---

## Structured Notes
Some errors emit additional note entries providing expected vs found types to aid tooling (JSON mode includes these in a `notes` array):
- E0309 (phi incoming value type mismatch) – emits two notes: `expected <phi-type>` and `found <incoming-type>`.
- E1220 / E1223 / E1225 (global const initializer mismatch cases) – emit expected/found notes detailing the declared global type vs the initializer element or nested element type.

Notes appear only when a mismatch occurs; they do not change the primary error code semantics.

Out-of-Scope (Phase 3) – Optimization pass pipeline & extended suggestion coverage are deferred and may introduce new ranges later.

_Last updated: 2025-08-15_

---

### E143x – Closures & Captures
Form: `(closure %dst (ptr <fn-type>) %fn [ %captures... ])` – currently supports a minimal slice with a single `%env` capture for non‑escaping closures.

| Code  | Title                         | Condition / When Emitted                               | Hint |
|-------|-------------------------------|---------------------------------------------------------|------|
| E1430 | closure arity                 | Wrong arity / malformed operands                        | use `(closure %dst (ptr <fn-type>) %fn [ %env ])` |
| E1431 | closure dst must be %var      | Destination missing `%`                                 | prefix destination |
| E1432 | closure annotation must be fnptr | Second operand not `(ptr (fn-type ...))`             | annotate with function pointer type |
| E1433 | closure unknown callee        | `%fn` not a known function symbol                       | define target function earlier |
| E1434 | closure signature mismatch    | Annotated thunk signature disagrees with target (ret/params/vararg) | align closure fn type with target |
| E1435 | closure capture type mismatch | Provided `%env` type disagrees with callee's first param | pass matching env value type |

Record-based closures:
- `(make-closure %dst Callee [ %env ])`
- `(call-closure %dst <ret> %clos %args...)`

| Code  | Title                              | Condition / When Emitted                                   | Hint |
|-------|------------------------------------|-------------------------------------------------------------|------|
| E1436 | make-closure arity                 | Wrong arity (missing closure pieces like capture vector)    | use `(make-closure %dst Callee [ %env ])` |
| E1437 | call-closure validation failure    | Arg count/type mismatch, bad closure value, or ret mismatch | ensure `%clos` is a closure, args exclude env, and `<ret>` matches |

Notes:
- Current implementation lowers closures to a per‑site thunk with a private global storing `%env`. Future iterations will construct an explicit closure record `{ fnptr, env }` and support multiple captures and escaping semantics.

---

### E145x – Try/Catch
Form: `(try :body [ ... ] :catch [ ... ])` – minimal catch‑all support initially. The type checker validates structure; semantics are lowered per EH model (Itanium landingpads; SEH funclets).

| Code  | Title                        | Condition / When Emitted                                 | Hint |
|-------|------------------------------|-----------------------------------------------------------|------|
| E1450 | try missing :body            | `:body` section absent                                    | add `:body [ ... ]` (can be empty) |
| E1451 | try :body must be vector     | `:body` present but not a vector                          | wrap body ops in `[ ... ]` |
| E1452 | try :catch must be vector    | `:catch` present but not a vector                         | wrap catch ops in `[ ... ]` |
| E1453 | try missing :catch           | `:catch` section absent                                   | add `:catch [ ... ]` |

Reachability and lints:
- The lints analyzer treats try/catch as reachable if either body or catch can reach a terminator.
