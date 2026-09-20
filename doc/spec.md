# The SOLID Language Specification

## 1. Scope and conventions

This specification defines the semantics of SOLID, a statically typed, compiled systems programming language: program structure, the type system, declarations, expressions, generics and contracts, compile-time evaluation, conditional compilation, the memory and reference model, run-time semantics, the core library, and the platform layer.

The lexical structure and the grammar of SOLID are defined in *doc/syntax.md*. That document is authoritative for which sequences of characters form which grammatical productions; this specification is authoritative for what those productions mean. The two documents are complementary and together form the complete definition of the language.

In this specification, **shall** expresses a requirement on programs or on implementations. A program that violates a program-side requirement is **ill-formed**; an implementation shall report at least one diagnostic for every ill-formed program. Where this specification does not define a behavior, the behavior is **unspecified**.

The words *diagnostic* and *run-time check* are used as follows:

- A *diagnostic* is a message reported at compile time; it identifies a position in the source and the violated rule.
- A *run-time check* is code emitted by the implementation that tests a condition during execution and terminates the program through the panic mechanism (§14) if the condition holds. Reason codes named in this specification (for example `DIV_ZERO`) are part of the observable panic payload.

## 2. Program structure

### 2.1 Translation units and packages

- A source file is a translation unit. A translation unit consists of an optional namespace declaration, followed by zero or more `using` declarations, followed by zero or more top-level declarations. A translation unit with no declarations is well-formed.
- Files are physical organization only. File names never participate in naming. Declarations of several files may share one namespace; declarations of files without a namespace declaration merge into the same package root.
- A namespace declaration is file-level, optional, at most one per file, and shall appear before all other declarations. It may contain one or more segments separated by `::`. Declarations of a file with no namespace declaration belong to the package root.
- The canonical path of a declaration is `package :: namespace? :: name`. The package's own name is invisible inside that package: a path written inside a package never begins with the package's own name.
- core is implicitly and wholly injected into every package except core itself (the *prelude*). core has zero dependencies and does not reference itself.
- The order of top-level declarations is free; forward references are supported.
- Declarations do not nest. A function body contains statements only; `using` in a body is a statement (§8).

### 2.2 Top-level declarations

The top-level declaration kinds are `let`, `struct`, `enum`, `union`, `contract`, and `func`. Each may carry annotations. A package-root or same-namespace name declared twice is ill-formed.

### 2.3 Visibility

There is no visibility mechanism: every declaration and every field is public. A leading underscore in a name is a convention with no semantics. The closedness of enumerations (§5.3) is guaranteed by the grammar, not by visibility. `@export` (§5.6) operates at the binary symbol level and is orthogonal to source-level visibility.

### 2.4 Packages and manifests

A package is described by a `project.solid.toml` manifest containing:

- `[package]`: `name` and `version`;
- `[build]`: `kind = "lib"` (default) or `kind = "bin"`. Only a package with `kind = "bin"` has a program entry (§15);
- `[features]`: default values of the package's configuration knobs (§12.3);
- `[dependencies]`: the dependency closure.

Rules:

- The dependency graph shall be acyclic.
- core shall not appear in `[dependencies]`; the prelude is part of the platform, not a dependency.
- Package names are unique within a dependency closure.
- A dependency source is exactly one of: a local `path`; a `git` source with exactly one of `tag`, `branch`, `rev` (`branch` defaults to `"main"`); a `version` requirement resolved against the default registry; or a `version` requirement with an explicit `registry`.
- Version requirements use SemVer requirement syntax. Within a closure, requirements on the same package are intersected; an empty intersection is ill-formed.
- A `solid.lock` file records resolved sources. How lock files are produced and consumed is a tooling concern and does not affect the semantics of a compilation.

## 3. Lexical conventions

Lexical structure is defined in *doc/syntax.md*. This section defines its semantic content.

- The grammar's keywords are reserved and are not identifiers. `true` and `false` are not keywords; they are constants declared in core (§12.1) and resolve through ordinary name lookup.
- Integer literal suffixes are `i8 i16 i32 i64 isize i u8 u16 u32 u64 usize u`; floating-point suffixes are `f32 f64 f d`. A suffix denotes the corresponding core type directly; suffix resolution is not subject to name lookup and cannot be shadowed. A name such as `i8` written as an ordinary path resolves through normal lookup and may be shadowed.
- An integer or floating-point literal without a suffix has type `i32` or `f64` respectively. `i` and `u` denote `isize` and `usize`; `f` and `d` denote `f32` and `f64`. These are fixed aliases, not context-dependent.
- There is no address-of operator. `*` is the dereference operator; in the return position of a contract declaration it introduces a wildcard output (§9.3).
- Unary operators are `- + ! ~ *`. Operator precedence and associativity are as defined in *doc/syntax.md*.

## 4. Types

### 4.1 Intrinsic types

The intrinsic types are the integer types `i8 i16 i32 i64 isize u8 u16 u32 u64 usize`, the floating-point types `f32 f64`, the boolean type `bool`, and the array type `Array<T, N: usize>`. `isize` and `usize` are pointer-width and vary with the target. `true` and `false` are the two values of `bool`.

Intrinsic types are declared in core and resolve through ordinary name lookup; they are not compiler-private names.

### 4.2 Core ordinary types

In addition to the intrinsic types, core declares: `Rune` (a `u32` code point), the string family `String8/16/32`, `MutableString8/16/32`, `CString8/16/32`, `MutableCString8/16/32`, `Slice<T>`, and `Opaque` (an empty struct). A string literal has type `String8`; its length is the number of UTF-8 code units. `&Opaque` serves as the untyped opaque pointer.

A *typed opaque handle* is any user-declared zero-sized struct (§5.2). Distinct declarations denote distinct types.

Names of core types may be shadowed by user declarations, like any other names.

### 4.3 Composite types

- *Named type*: `a::b::T<args>`. A generic-argument list applies only to the final segment of the path.
- *Reference type*: `&[readonly | writeonly | noaccess] T`. The four ref-kinds — read-write, readonly, writeonly, noaccess — are four distinct types with the same representation (§11.3).
- *Array type*: `[Expr] T`. It is sugar for `Array<T, N>`; after normalization the two spellings denote the same type, and `&[N]T` denotes the same type as `&Array<T, N>`. The length shall be a constant expression (§10.1) of type `usize`. `[0u]T` is well-formed. The total size of a type is limited by the target's `size_t`; exceeding it is ill-formed.
- *Function type*: `&func(param-types)[callconv][: return-type]`. Function types have no captures.

### 4.4 Typed slots

An integer literal that appears in a position requiring a specific type — an array length, an enum discriminant, a constant generic argument, a `@when` condition, an annotation argument — shall carry the suffix of exactly that type. Unsuffixed literals have only their default types (§3); no position adapts a literal to a slot.

### 4.5 Type equivalence and conversion

- Two types are the same type when they are spelled from the same declaration: type identity is declaration identity. A generic instance is identified by its declaration together with its argument sequence.
- Function types are equal when their parameter types, return type, and callconv are equal item-wise.
- There are no implicit conversions. Explicit conversion is provided by ordinary conversion functions declared in core: numeric widening and narrowing, extraction of an enum discriminant, interconversion of opaque handles, reference-to-`usize` and `usize`-to-reference (pointer integer conversions), and reference re-qualification `as_readonly` / `as_writeonly` (`&T` to `&readonly T` / `&writeonly T`), which reinterprets without generating code.

### 4.6 Recursive types

- *Strong dependencies* — field types, array element types, by-value generic instances — participate in cycle detection. Function types and references (`&T`, in every ref-kind) are weak dependencies and do not participate.
- A strong-dependency cycle is ill-formed (the type would have infinite size).
- There are no forward declarations and no incomplete types. Recursive and mutually indirect structures are expressed with references.
- Generic instantiations are checked after instantiation (§10.6).

### 4.7 Layout

- A struct's fields are laid out in declaration order, each aligned to its ABI alignment; the struct's alignment is the maximum of its members' alignments; its size is rounded up to that alignment. This is the C default layout.
- The numeric layout facts (scalar alignments, pointer width) are those of the target's application binary interface; this specification does not fix them.
- A union's fields share storage; its size is the size of its largest field and its alignment the maximum of its members' alignments. Reading an arm other than the last-written one reinterprets the stored bytes; the responsibility is the programmer's.
- An enum's storage is its behind type (§5.3).
- *Zero-sized types* — an empty struct, an empty union, an empty enum, and `[0u]T` — have size 0 and alignment 1. A zero-sized type has no value, only an address: it cannot be dereferenced, and it shall not appear in any by-value position: as a `let` binding, a parameter type, a return type, a field type, a union arm, an array element type, or a by-value generic argument (checked after generic instantiation). All use of a zero-sized type is through references.

### 4.8 Literal typing

Literals are self-describing; no position infers or absorbs a literal's type.

- A struct literal `Type{ ... }` and an array literal `[N]T{ ... }` carry their types themselves.
- A rune literal has type `Rune`. A string literal has type `String8`.
- `true` and `false` have type `bool`.
- A literal whose value lies outside its type's range is ill-formed.

Every expression is type-self-contained: the type of an expression is determined by its sub-expressions alone, independent of the context in which it appears.

## 5. Declarations

### 5.1 let

- Top-level form: `let NAME [: Type] [= Expression];`. The type may be omitted (the initializer's type is used verbatim).
- A user-declared let shall have an initializer. Initializer-less lets obtain their values externally; exactly three forms exist:
  1. `@intrinsic let` — toolchain-provided constants (core's `true`/`false` and the platform/build flags, §10.3);
  2. `@feature let` — package configuration knobs (§12.3);
  3. `@import` — import of an external global symbol (§5.6).
- A top-level let binds an immutable name with value semantics: binding copies aggregates whole. Global mutable state does not use `let`; it uses a `@static` reference with `set` (§11).
- Every top-level initializer shall be a constant expression (§10.1). There is no run-time global initialization; state that requires run-time computation is given a constant initial value via `@static` and initialized explicitly at the start of `main`.
- Top-level initialization is evaluated in dependency (topological) order; a dependency cycle is ill-formed. Initializer-less lets do not participate.

### 5.2 struct and union

- `{ ... }` is a complete definition; fields are `name: Type`. Two fields with the same name are ill-formed.
- An empty field list `{}` defines a zero-sized type (§4.7). This is the only user-side spelling of an empty type.
- The `;` form (`struct S;`, `union U;`) is well-formed only with `@intrinsic`: the implementation supplies the layout. An initializer-less `;` form without `@intrinsic` is ill-formed; it is not a forward declaration. Redeclaring a name that already has a definition is ill-formed; SOLID has no forward declarations and no completion of declarations.
- A struct literal `Type{ id = value, ... }` requires an explicit type. Omitted fields are zero-filled; `{}` denotes an all-zero value. An unknown field name or a field initialized twice is ill-formed.
- A union literal shall initialize exactly one field; initializing none or more than one is ill-formed. A union field read reinterprets stored bytes (§4.7).

### 5.3 enum

An enum declaration `enum Name [: Type] { members }` is semantically equivalent to a wrapper struct holding the behind value, with each member materialized as a constant of the enum type under the enum's name: `Color::Red` is an ordinary path descent, subject to no member-specific rules. The `;` form is ill-formed for enums — an empty enum is spelled `enum E {}` (§4.7); enums have no `@intrinsic` form.

- A member is a constant of the enum type. Member names live in the namespace descended from the enum's name; two members with the same name are ill-formed.
- A discriminant `name [= Expression]` with the expression omitted takes the previous value plus one (the first member defaults to zero). A discriminant shall be a constant expression (§10.1); an integer literal used as a discriminant shall carry the suffix of the behind type (§4.4). Two members with equal discriminant values are well-formed.
- The behind type is the wrapped representation type. It may be any integer type, including signed types (negative discriminants are then well-formed). If omitted, it is the smallest unsigned type (`u8`, `u16`, `u32`, or `u64`) that contains all discriminant values.
- *Closedness*: an enum type has no literal syntax in the language. No code, including the declaring module, can construct an enum value other than through the declared members: an enum type does not accept a struct literal. Discriminant values are readable by everyone (`Color::Red._e` follows the ordinary field rules) and constructible by no one.
- Synthesized operators: every enum synthesizes `==` and `!=` (comparing discriminant values) by registering implementations of `EqualOp` / `NotEqualOp` (§7.2). An enum annotated `@flag` additionally synthesizes `|`, `&`, `^`, `~` over the behind representation. No other operator is ever synthesized; all other operations require explicit `fulfills` declarations.
- `@flag` enums perform no discriminant-range checking; the interpretation of the bit pattern is the programmer's.

### 5.4 contract and fulfills

- A contract is a pure, possibly generic signature: parameter types and a return position, with no body. The return position may declare a *wildcard output*: `: *Name` names an abstract output type filled in by each implementation's return type (§9.3).
- A function declares that it implements contracts with one or more `fulfills Contract<args>` clauses. The arguments are the contract's generic parameters; the wildcard output is not an argument.
- A generic parameter may carry a constraint that is a contract instance: `F: One<T>`. The constrained name denotes a function value inside the body and may be called directly (`F(...)`).
- A constraint name exists only in generic parameter position. It cannot be used as a parameter type, value type, or field type.

### 5.5 func

- Form: `func Name[<generic-params>](params)[callconv][: return-type] [fulfills Contract<args>, ...] (body | ;`.
- There is no function overloading: two functions with the same name in one namespace are ill-formed. Genericity plus contract search is the only polymorphism.
- Types and values share one namespace: a `struct S` and a `func S` in the same namespace are ill-formed.
- A function without a body shall be annotated `@intrinsic` or `@import` (§5.6).
- A function declaration binds its name to a constant of type `&func(signature)` (§7.4). No decay exists; no conversion occurs.

### 5.6 FFI boundary

- A callconv is legal on any function. Omitted, it denotes SOLID's internal convention, which promises no C compatibility; the implementation is free to choose it.
- A function annotated `@import` or `@export` shall specify a callconv explicitly; omission is ill-formed.
- The predefined callconv names are `cdecl` — the target's default C convention — and `stdcall`. `stdcall` is well-formed only on targets where it is distinct from the default (32-bit x86); elsewhere it is ill-formed. An unknown callconv name is ill-formed. The name list is open; targets may extend it.
- An ordinary (non-FFI) function with an explicit callconv is the C-callback form: the function is called from C but need not export a symbol.
- `@import(LIBRARY, SYMBOL) func f(...)...;` imports an external function (`@import(SYMBOL)` names the symbol only; the library is decided at link time). `@import` on a let imports an external global (§5.1).
- `@export` exports the function under a C symbol for external callers. The symbol name is unmangled and defaults to the declared name; `@export("alias")` renames it. Parameter and return types are unrestricted; cross-boundary type correctness is the programmer's responsibility. `@export` concerns only the binary symbol level.
- Callback type safety: since `&func` equality includes the callconv (§4.5), a function value with the internal convention cannot flow into a `&func(...)[cdecl]` parameter; the ABI of a callback is enforced by the type system.

## 6. Names, paths, and scopes

### 6.1 Paths

- `NamePath = identifier { :: identifier }`; `Named = NamePath [ < generic-args > ]`.
- Resolution tries package-relative resolution first, then world-absolute resolution. A world-absolute path shall not begin with the current package's own name.
- A generic-argument list applies only to the final path segment.
- *Type positions* accept only type entities; *value positions* accept only value entities. Structs, enums, unions, and contracts are type entities; function names and enum member paths are values. A namespace name denotes neither and is diagnostic in both positions.

### 6.2 using

- A `using` declaration imports a namespace — never a single symbol — into the translation unit's import layer. The target may be any namespace, including a namespace of another package or another package's root (which the dependency closure must declare first). Importing the same namespace twice is harmless.
- There is no single-symbol import.
- If a bare name resolves to more than one using target, each use of that bare name is ambiguous and ill-formed.

### 6.3 Lookup and shadowing

Name lookup walks the following levels from innermost to outermost:

1. block levels — every block statement introduces one level, and nested blocks stack;
2. the function level — the function body's own block does not introduce a level of its own: the body's top-level `let` bindings share the function level with the function's formal parameters and generic parameters;
3. the current namespace level;
4. the package level — the package root's declarations plus the first segments of all namespaces declared in the package;
5. the visible-import level — explicit `using` targets and the core prelude, on equal footing;
6. the global level — world-absolute names.

Rules:

- Within one level, two bindings of the same name are ill-formed. In particular, a `let` at the top of a function body that duplicates a formal parameter or a generic parameter of the same function is ill-formed.
- A binding in an inner block level shadows the same name from outer levels; no conflict is reported across levels.
- A declaration at the current-namespace or package-root level dominates any imported name (silent shadowing, including names from core). Two imported names competing for one bare name are a use-site ambiguity (§6.2).
- A block-level `using` injects the namespace's members as bare names into the block level. They are invisible outside the block and shadow everything outer, including parameters and outer bindings. A block-level `using` name colliding with a `let` of the same block is a same-level duplicate and ill-formed.
- Names of core may be shadowed by user declarations at any declaration level; shadowing is well-formed and the shadowing value governs (§10.3 applies this to guards).

```solid
func f(x: i32, y: i32) {
    let x = 0;        // ill-formed: duplicates parameter x (same level)
    {
        let x = 1;    // well-formed: an inner block shadows outer bindings
        let x = 2;    // ill-formed: same block, same level
    }
    {
        using A::B;   // well-formed when A::B declares y: that y shadows the parameter
    }
}
```

## 7. Expressions

### 7.1 Order

Operator precedence and associativity are defined in *doc/syntax.md*: binary operators are left-associative, unary operators right-associative, postfix operators bind tightest. Within an expression, operands are evaluated from left to right.

### 7.2 Operator dispatch

Every operator except the logic operators is sugar for a call resolved through a contract. Each contract corresponds to exactly one operator; the routing function named below is an ordinary generic function in core and the operator's uniform entry point — the operator form and the explicit call form share one resolution.

| Operator | Contract | Routing function |
|---|---|---|
| unary `+`, unary `-` | `PlusOp<T>`, `NegateOp<T>` | `_plus`, `_negate` |
| `+ - * / %` | `AddOp`, `SubOp`, `MulOp`, `DivOp`, `ModOp` (each `<TLeft, TRight>`) | `_add`, `_sub`, `_mul`, `_div`, `_mod` |
| `== !=` | `EqualOp`, `NotEqualOp` (each `<TLeft, TRight>`) | `_equal`, `_not_equal` |
| `< > <= >=` | `LessThanOp`, `GreaterThanOp`, `LessEqualOp`, `GreaterEqualOp` | `_less_than`, … |
| `<< >>` | `ShiftLeftOp`, `ShiftRightOp` | `_shift_left`, `_shift_right` |
| `~ & \| ^` | `BitNotOp<T>`, `BitAndOp`, `BitXorOp`, `BitOrOp` | `_bit_not`, `_bit_and`, `_bit_xor`, `_bit_or` |
| `a[i]` | `Index<TContainer, TIndex>` (wildcard return `*TValue`) | `_array_index` family |
| `! && \|\| ^^` | no contract; operands shall be `bool`; not overloadable | — |

Resolution of `a + b`: the operand types fix `TLeft`/`TRight`; the key `AddOp<TLeft, TRight>` — a contract together with all of its generic arguments — searches the visible implementations (§10.5); the found implementation's return type fills the wildcard position by lookup, not inference.

Rules:

- Comparisons return `bool`.
- `^^` evaluates both operands; only `&&` and `||` short-circuit.
- Subscripting resolves through `Index`. core provides built-in implementations for the four array forms: `Array<T, N>` by value yields `T`; `&Array<T, N>` yields `&T`; `&readonly Array<T, N>` yields `&readonly T`; `&writeonly Array<T, N>` yields `&writeonly T`. Contracts may be instantiated over reference types.
- Mixed-type arithmetic is opt-in: a user implementation of, e.g., `AddOp<i32, i64>` (returning the chosen result type) makes `1i32 + 2i64` well-formed. Without an implementation it is ill-formed.
- A user type gains operators by implementing the corresponding contracts, `==` included; the semantics are the implementer's.
- Unary `+` is a contract call, not identity.
- Enumerated synthesized operators enter the same search (§5.3).
- Arithmetic semantics: `/` and `%` truncate toward zero, `%` takes the sign of the dividend; shifts are logical for unsigned and arithmetic for signed operands; integer `+ - *` overflow wraps (defined behavior, never checked); integer division or remainder by zero, signed division overflow (`INT_MIN / -1`), and a shift count ≥ the operand's bit width terminate via a run-time check and panic (§14) with reason codes `DIV_ZERO`, `DIV_OVERFLOW`, and `SHIFT_RANGE`; floating-point division follows IEEE (±inf / NaN) and is not checked.

### 7.3 Direction rules

Expressions have no lvalue/rvalue distinction. Direction is determined by the operand's type:

| Expression | Operand type | Result type | Meaning |
|---|---|---|---|
| `a.field` | `T` (struct value) | `F` (field type, value) | copy of the field |
| `p.field` | `&T` | `&F` | address at the field's offset |
| `a[i]` | `[N]T` value | `T` | copy of the element |
| `p[i]` | `&[N]T` | `&T` | address at the element's offset |
| `*p` | `&T` or `&readonly T` | `T` | load |

- Through any nesting, `.` and `[]` on values produce values and on references produce addresses.
- The ref-kind of a chain is the intersection of the root kind and every member's declared kind (data fields declare read-write): readonly root over a `&writeonly` field yields `&noaccess` (§11.3).
- *Method call sugar*: `a.f(args)` resolves in two steps. If the receiver's type has a field `f`, it is a field access followed by a call of the field's function value (fields win). Otherwise `f` is resolved as a function whose first parameter type strictly matches the receiver expression's type — no conversions, no automatic referencing or dereferencing; generic candidates unify (§9.3) — and the call desugars to `f(a, args...)`. Only the call form triggers the sugar; `a.f` without parentheses is a field access. A matched function whose first parameter does not match is ill-formed; resolution does not fall through to outer scopes for a better match.
- Reading a reference requires explicit `*` on `&T` or `&readonly T`; writing goes through `set` (§8).
- There is no address-of operator; allocation produces addresses, and `.`/`[]` on references compute addresses.
- A run-time out-of-bounds subscript terminates via a run-time check and panic (reason code `BOUNDS`); an out-of-bounds subscript detectable at compile time is ill-formed. Bounds checks are always emitted; no build mode removes them.

### 7.4 Function values and calls

- A function's name is a constant of type `&func(signature)`; the callconv is part of the signature. `let f = g;`, `return g;`, passing `g` as an argument, and storing it in a field or array are all ordinary value transfers with exact type matching.
- Calling is uniform over `&func` values: `g()` on a constant, `f()` on a run-time value, and `(a.b)(x)` on a function-typed field are the same form. Whether the implementation emits a direct or an indirect call is unspecified.
- A bare generic function name is not a value. A fully instantiated one — contract arguments may be omitted when the search (§10.5) is unique — is a `&func` constant.
- `@` built-ins are not values; they exist only in call form (§11.2).
- A call with an argument count different from the parameter count is ill-formed.

### 7.5 Literals and construction

- Literal typing follows §4.8.
- A struct literal's type is explicit; omitted fields are zero-filled; `{}` is all-zero; unknown or duplicated fields are ill-formed.
- An array literal `[N]T{ e1, ... }` with an element count other than `N`, or with inconsistent element types, is ill-formed.
- `@` built-ins are primary expressions (§11.2).
- Nested literals infer nothing (§4.8).

## 8. Statements

| Statement | Semantics |
|---|---|
| `;` | empty statement, well-formed |
| `{ ... }` | introduces a new scope |
| `let id [: T] = value;` | binds an immutable local name; aggregates copy whole; a local binding occupies no storage unless its address is taken via an allocation intrinsic. Binding the same name twice in one level is ill-formed — the function body's top-level `let`s share their level with the formal and generic parameters; an inner block's `let` shadows outer bindings (§6.3) |
| `using path;` | block-level import (§6.3); may appear anywhere in a block |
| `set lhs = rhs;` | store instruction: the type of `lhs` shall be `&T` or `&writeonly T` and the type of `rhs` exactly `T` — no conversions. Ill-formed left sides include `*p` (a value), a let-bound name (not a location), and targets of type `&readonly` / `&noaccess`. Reading through `&writeonly` is ill-formed. Re-assigning a reference requires an explicit double slot: the left side has type `&&T` and the right side `&T` |
| `if (cond) body [else ...]` | a statement with no value; `cond` shall be `bool`; `else if` chains are well-formed |
| `loop body` | unconditional loop |
| `while (cond) body` | `cond` shall be `bool` |
| `break;` / `continue;` | well-formed only inside the nearest enclosing loop; no labeled breaks |
| `return [value];` | a function returning nothing shall `return;`; a function returning `T` shall return a value of exactly type `T` on every path that can reach the end — a path that falls through the end of a non-void function is ill-formed (simple reachability analysis) |

`if` is a statement, not an expression; the language has no conditional expression.

## 9. Generics and contracts

### 9.1 Generic parameters

| Kind | Form | Allowed on | Use |
|---|---|---|---|
| type parameter | `T` | structs and funcs | wherever a type is expected |
| constant parameter | `N: integer-type` | structs and funcs | type positions (`[N]i32`) and value positions in the body (`return N;`) — a compile-time constant |
| contract parameter | `F: Contract<args>` | funcs only | called directly in the body (`F(...)`) — a function value |

### 9.2 Generic arguments

- Type arguments: `List<i32>`.
- Named constant arguments: `N = 5u`; the literal carries the suffix of the parameter's type (§4.4). A named argument's value may reference outer generic parameters (`Array<T, N = N>`). Positional and named arguments may be mixed, but a named argument shall not target a parameter already filled positionally.
- Function arguments: `F = _one_i64` — a function value (§7.4).

### 9.3 Wildcard returns, contract search, and projection

- The *search key* of a contract is the contract together with all of its generic arguments; the wildcard output is not part of the key. When the search finds an implementation, the implementation's return type fills the wildcard position definitionally — implementations do not restate the output, and no consistency check is needed.
- The wildcard name shall not equal any of the contract's generic parameter names; a contract has at most one wildcard; `*` before an identifier is grammatically legal only in the return position of a contract declaration.
- A generic function's contract argument may be omitted, in which case the implementations of the required contract instance are searched (below); or given explicitly, `F = name`.
- *Automatic search* selects an implementation in the use-site scope, along the same lookup chain and with the same precedence as name lookup (§6.3): block-level `using` > file-level `using` > package level > visible imports (including core) > global. Implementations may themselves be generic, in which case the key unifies with the implementation's pattern; exactly one unifiable candidate per level resolves. Several candidates at the same level — concrete against generic, or generic against generic — are a use-site ambiguity; concreteness grants no priority. Across levels the innermost candidate wins.
- Within one namespace, two implementations with the same key are ill-formed. Implementations of the same key in different namespaces or packages are competing candidates, not a conflict.
- A constraint is solved at the instantiation point: a call site solves with the call site's chain; constraints inside a generic body solve in the body's own package scope. Injection propagates only through explicit contract arguments.
- *Projection*: `F::TResult` — a contract parameter followed by `::` and the wildcard name of its constraining contract. Resolution finds `F` (innermost hit in the function level) and verifies the final segment equals that contract's wildcard name. In generic context the projection stays delayed and normalizes at instantiation. `::` otherwise uniformly means "the named member of": namespace descent, enum member, or type projection. There is no concrete form (`Contract<i32, i64>::TResult` is not a type spelling).
- A contract parameter `F` constrained by an operator contract is itself a candidate at the function level of the operator search: inside a generic body, `x + x` resolves through `F` without a global re-search.

### 9.4 Instantiation

- Instance identity is `declaration + argument sequence`.
- `[N]T` normalizes to an `Array<T, N>` instance.
- An instantiation that cannot terminate (a generic instantiating itself without progress) is ill-formed; implementations shall detect it.

## 10. Compile-time evaluation and conditional compilation

### 10.1 Constant expressions

A constant expression is evaluated at compile time by an evaluator whose scope is:

- constant folding of the built-in arithmetic and comparison operators over literals, including `String8` equality (core's derived platform flags fold through it);
- the type queries `@sizeof(T)`, `@alignof(T)`, `@offsetof(T, field)`, `@nameof(T)`;
- suffixed literals;
- constant generic parameters;
- no function calls, and no other language constructs.

Floating-point arithmetic is not evaluated at compile time. An operation whose run-time counterpart would panic (§14.2) is ill-formed in a constant context — there is no undefined behavior at compile time.

Consumers of constant expressions: `@when` guards and the constant world (§10.3), array lengths, enum discriminants, annotation arguments, constant generic arguments, and the initializers of `@const` / `@static`.

`isize`/`usize` are pointer-width (§4.1); `@sizeof`, `@alignof`, and layout computation follow the target's widths.

### 10.2 The `@` built-ins

`@` marks a compiler built-in in two syntactic positions, disambiguated by position:

- *Annotation position* — on declarations, fields, generic parameters, and formal parameters: `@name` attaches metadata to the declaration.
- *Expression position* — a compile-time built-in function: `@name(args)`, with type arguments passed in parentheses.

Built-ins are exempt from the ordinary function rules: one built-in may expose several parameter forms (e.g. `@panic(msg)` / `@panic(msg, code)`). Built-in names are reserved: user annotations shall not use them.

| Annotation | Meaning |
|---|---|
| `@intrinsic` | built-in declaration (struct/func/let); lowered by the implementation |
| `@flag` | enables bit-operator synthesis on an enum (§5.3) |
| `@import` | external symbol import (func and let; §5.6) |
| `@export` | C symbol export (§5.6) |
| `@feature` | declares a package configuration knob (§3.4, §10.3) |
| `@when` | conditional-compilation guard (§10.4) |
| `@panic_handler` | marks a panic handler (§14.3) |
| any other name | a custom annotation: no semantics, preserved verbatim; shall not collide with a built-in name |

| Expression | Form | Meaning |
|---|---|---|
| `@sizeof(T)` | → `usize` | size of the type |
| `@alignof(T)` | → `usize` | alignment of the type |
| `@offsetof(T, field)` | → `usize` | offset of a field |
| `@nameof(T)` | → `String8` | name of the type |
| `@alloc(init)` | → `&T` | stack allocation, function bodies only (§11) |
| `@const(init)` | → `&readonly T` | `.rodata` allocation; `init` a constant expression (§11) |
| `@static(init)` | → `&T` | `.data`/`.bss` allocation; `init` a constant expression (§11) |
| `@panic(msg)` / `@panic(msg, code)` | terminating | program-declared panic (§14.3) |
| `@abort()` | terminating | unconditional termination (§14.3) |
| `@source_file` / `@source_line` / `@source_col` | `String8` / `u32` / `u32` | source position as literals |

### 10.3 The constant world

Compile-time constants form a single layer — the *constant world* — with three sources:

| Source | Declared as | Value provided by | Overridable |
|---|---|---|---|
| platform/build facts | core `@intrinsic let` facts (`TARGET_OS`, `TARGET_ARCH`, `BUILD_MODE`, …, §12.1) plus the derived boolean flags core computes from them (`IS_OS_LINUX`, `IS_ARCH_X64`, `IS_DEBUG`, …) | the driver, per target and build profile | no |
| package knobs | `@feature let` (package root, no initializer, §5.1) | the package's own `[features]` manifest defaults | the compilation root (§3.4) |
| source constants | top-level `let` (§5.1) | in-source initializers | no |

- Every externally supplied value has an explicit declaration; the driver supplies values by name only. core and the driver ship in lockstep, so the declared and supplied name sets cannot diverge. The driver shall supply each fact from the value set that core declares beside its derived flags (e.g. `TARGET_OS` ∈ linux / windows / macos / osless, `BUILD_MODE` ∈ debug / release).
- The manifest correspondence is validated before anything else: each `@feature` declaration shall have a `[features]` key in its own manifest, each `[features]` key shall have a declaration, each root override key shall have a declaration, and each supplied value shall match the declared type (manifest integers parse as `i64`; supplying one for a narrower integer knob shall be in range). Violations are ill-formed.
- Value precedence is two-level: a root override wins over the package's own `[features]` default. A library's own `[features]` values serve both as its standalone values and as the defaults for its dependents.
- Knob types are `bool`, any integer type, or `String8`. A `String8` knob is consumable only in code; the guard grammar (§10.4) contains no strings.
- Constant names are ordinary names: core flags and `true`/`false` may be shadowed inside a package, and guards observe the shadowing values.

### 10.4 Conditional compilation

`@when(condition)` gates a top-level declaration: the declaration exists in the compilation if and only if the condition holds.

- **Position**: guards apply only to the six top-level declaration kinds, at most one guard per declaration. Namespaces, fields, statements, and `@feature let` declarations cannot be guarded — a knob is an input to gating, not a subject of it.
- **Condition grammar** (closed): names from the constant world (bare or package-internal paths, including core's `true`/`false` and flags), suffixed integer literals (§4.4), the operators `! && || == != < > <= >= + - * /`, and parentheses; the value shall be `bool`. Division by zero in a constant context is ill-formed. Strings do not appear in conditions.
- **Visibility**: a guard sees core (the prelude: flags and `true`/`false`) and the current package's top-level declarations — nothing else. Configuration is local to a package; only the platform is universal. A bare name resolving to several package-level constants is ill-formed.
- **Dependency closure**: a guard's condition shall not — transitively through other constants — depend on the declaration it guards (self-reference is ill-formed), and shall not depend on the type-dependent intrinsics `@sizeof` / `@alignof` / `@offsetof` / `@nameof` (layout exists only after checking). Cycles among constant lets are ill-formed.
- **Evaluation semantics**: guards observe the *unpruned* constant world — every constant is evaluated first, then guards are decided; evaluation is total and involves no fixpoint. External flags enter the world through declarations only; a guard never writes back into the flag namespace.
- **Effect**: after pruning, the surviving declarations constitute the program. Two same-name declarations are a redeclaration diagnostic only if both survive pruning; guards with disjoint conditions make intentional same-name alternatives well-formed. Pruned declarations are not collected, resolved, or checked.

```solid
@when(IS_OS_LINUX)    let PAGE: usize = 4096u;
@when(!IS_OS_LINUX)   let PAGE: usize = 16384u;

@when(ENABLE_NET)  @import("net", "connect") func connect()cdecl: i32;
```

### 10.5 Contract search timing

Implementation search (§9.3) happens during checking, after name resolution (P4); the candidate sets the search consults are recorded during resolution.

### 10.6 Translation phases

Translation is defined by the following conceptual phases; a phase consumes only the products of earlier phases.

```
P0  parse; read manifests; validate feature/manifest correspondence; fix the knob value table
P1  build the constant world: core intrinsics + the package's features + the package's
    top-level lets; evaluate in topological order (cycles and the guard bans of §10.4 are ill-formed here)
P2  decide every @when guard against the constant world; prune; surviving declarations are the program
P3  collect: register the surviving declarations (forward references supported)
P4  resolve names
P5  normalize ([N]T to Array<T, N> instances; path and literal normalization)
P6  check types; evaluate the remaining constant expressions (type-dependent intrinsics are now available)
P7  generate code
```

The ordering `P2` before `P4` is normative: a pruned declaration is never resolved or checked (a pruned declaration may reference imports that do not exist for the target). `P1` is independent per package.

## 11. Memory and references

### 11.1 Binding and allocation

Binding and allocation are separate: `let` only binds a name; storage is obtained only through the allocation intrinsics.

| Intrinsic | Result | Legal position | Storage | Initializer |
|---|---|---|---|---|
| `@alloc(init)` | `&T` | function bodies only | stack; reclaimed at function exit | any expression |
| `@const(init)` | `&readonly T` | anywhere | `.rodata` | constant expression |
| `@static(init)` | `&T` | anywhere | `.data`/`.bss` | constant expression |

- The allocated type is the type of `init`; no type argument exists.
- An `@alloc` evaluation allocates one slot per function activation (the same expression evaluated several times in a loop yields the same address, with `init` re-evaluated each time); distinct allocation expressions yield distinct addresses; recursive calls yield distinct activations and distinct addresses.
- A function-local `@static` names global storage; each declaration site has its own storage.
- `@alloc` references escaping their activation are not diagnosed.
- Built-ins exist only in call form: their names cannot be passed as values, shadowed, or aliased.

### 11.2 The explicit heap

Dynamic storage is a program-owned object, not an implicit global heap:

- core provides `heap_create() -> &Heap`, `heap_destroy(h)` (arena-style reclamation of everything from the heap), `alloc<T>(h, count) -> Slice<T>` (uninitialized), `alloc_zero<T>(h, count)`, and `free<T>(h, s)`.
- A function that needs the heap takes it as an explicit parameter; there is no default heap.
- Heap memory is obtained from the platform layer (§13.2). Exhaustion routes to the panic handler (§14.3).
- A reference surviving `heap_destroy` is dangling; accessing it is an explicitly exempt behavior (§13.1). Arena semantics preclude double-free and per-object use-after-free by construction.
- In the `IS_DEBUG` build profile the implementation may poison destroyed arenas and report leaks; both profiles define behavior (§14.1).
- v1 execution is single-threaded; no construct creates threads, and no thread-local storage exists.
- Recursive structures need no boxing: `alloc<Node>(h, 1)` suffices.

### 11.3 Reading, writing, and mutability

- Reading a reference requires explicit `*` on `&T` or `&readonly T`. Writing requires `set` with a `&T` or `&writeonly T` left side (§8).
- `&T`, `&readonly T`, `&writeonly T`, and `&noaccess T` are four distinct types — the capability lattice {read, write} — with the same representation. There is no subtyping and no implicit conversion; `&T` re-qualifies to `&readonly T` / `&writeonly T` only through explicit conversion functions (§4.5). Every direction that widens access has no path.
- There is no null. The "empty" of `&T` is its zero value: `zero<&T>()` (all-zero bit pattern, via the `Zero` contract) and `is_zero(p)` (§13.1). The zero value is well-defined for every type. Dereferencing a zero reference is *target-defined*: on hosted targets it deterministically faults; on bare metal it accesses address 0. Implementations shall not infer non-nullness of a reference from the existence of a dereference.
- Reference equality compares addresses: core provides the `EqualOp` reference family over all sixteen ref-kind combinations, and `is_zero` is derivable as equality with `zero<&T>()`.
- A function whose parameters are all deeply-readonly value types (all reference fields in their transitive closure are `&readonly` or `&noaccess`) or `&readonly`/`&noaccess` references cannot modify caller-reachable storage through its parameters; the implementation may rely on this (§14.4).
- Value semantics: binding, `set`, argument passing, and return copy bitwise; `&` is the only aliasing mechanism.
- Lifetime and ownership are not tracked; a dangling reference after `heap_destroy` is the language's only exemption of its kind (§14.1).

## 12. The core library and the platform layer

### 12.1 core

core ships with the language version and contains:

| File | Contents |
|---|---|
| `type.solid` | the intrinsic types (§4.1); `Rune`, the string family, `Slice`, `Opaque` |
| `array.solid` | `@intrinsic struct Array<T, N: usize>;` and the four built-in `Index` implementations |
| `constant.solid` | `@intrinsic let true/false: bool;`; the platform/build facts (`TARGET_OS` / `TARGET_ARCH` / `BUILD_MODE`, driver-supplied `String8` values) and the derived boolean flags (`IS_OS_LINUX`, `IS_ARCH_X64`, `IS_DEBUG`, …) |
| `contract.solid` | the operator contracts (wildcard returns `*TResult`/`*TValue`), the comparison family, `Index`, and the special-value, extremal, hash, and length contracts |
| `function.solid` | the operator routing functions; `@intrinsic` implementations for the intrinsic types, including the `EqualOp` reference family (all sixteen ref-kind combinations), `String8` equality, and `as_readonly` / `as_writeonly` |

Built-in function names carry the `_` prefix; user declarations conventionally avoid that prefix.

Every `@intrinsic` has a defined lowering. Operations whose LLVM counterpart is total (arithmetic, bitwise, comparisons, conversions, pointer-integer conversions) translate directly. Operations with partial LLVM domains are preceded by a run-time check and panic: `sdiv`/`srem` by zero and `INT_MIN / -1`, `udiv`/`urem` by zero, shifts by ≥ the bit width. Float-to-integer conversion out of range terminates with `FPTOSI_RANGE` in the conversion functions. Implementations shall not emit operations carrying undefined-behavior flags (`nsw`, `nuw`, `exact`, `nnan`); integer overflow wraps by construction.

### 12.2 The platform layer

The platform layer is part of core and provides host capabilities over a minimal dependency surface:

| Platform | Dependency | Termination primitive |
|---|---|---|
| Linux | raw syscalls (memory: mmap/munmap; exit: exit_group; time via vDSO) | `ud2` |
| Windows | kernel32.dll (VirtualAlloc, ExitProcess, …) | `__fastfail` |
| macOS | libSystem.dylib | libSystem |
| bare metal | none; the toolchain provides start code and link scripts | halt |

- The toolchain generates `_start` and supplies the compiler-support runtime (memcpy/memset, integer division helpers).
- libc is an optional, explicit FFI dependency.
- Target facts enter the compilation as platform flags (§10.3); there are no implicit global states (no errno, no atexit, no stdio buffers).

### 12.3 Configuration knobs

A package declares its configuration surface as `@feature let NAME: Type;` at the package root (§5.1): no initializer, one name per knob. The package's own manifest `[features]` shall provide a default value for every knob; the compilation root may override any dependency's knobs through `[dependencies.X.features]`. See §3.4 and §10.3 for the validation and precedence rules, and §10.4 for the prohibition of guarding knobs.

## 13. Run-time semantics: defined checks and termination

### 13.1 Definedness

Every operation's outcome is defined: it either produces a defined value or terminates through a defined, deterministic path (§14). The same program on the same target behaves predictably, and every termination is observable (reason code and source position). Undefined behavior does not exist.

Exactly two behaviors are exempt and defined as garbage-valued:

1. access through a reference whose pointee was reclaimed by `heap_destroy` (the `IS_DEBUG` profile may poison and trap; otherwise the values are deterministic garbage);
2. reading memory allocated uninitialized from the heap (`alloc<T>`); stack and static allocations always have initializers.

The two build profiles `IS_DEBUG` and `IS_RELEASE` may differ in diagnostic strength only; both profiles define behavior. No build mode removes a run-time check (§8.2, §14.2).

### 13.2 Deterministic error table

| Violation | Defined outcome | Emitted by |
|---|---|---|
| integer division/remainder by zero | run-time check → panic (`DIV_ZERO`) | implementation |
| signed division overflow (`INT_MIN / -1`) | run-time check → panic (`DIV_OVERFLOW`) | implementation |
| shift count ≥ bit width | run-time check → panic (`SHIFT_RANGE`) | implementation |
| subscript out of bounds | run-time check → panic (`BOUNDS`); always emitted | implementation |
| arithmetic `+ - *` overflow | wraps | — |
| float→integer conversion out of range | check → panic (`FPTOSI_RANGE`) | the conversion functions |
| dereferencing a zero reference | target-defined (§11.3); no check inserted | — |
| OOM | routes to the panic handler | platform layer |
| stack overflow | guard-page fault → abort (the handler needs a working stack) | platform layer |

### 13.3 The three tiers

| | abort | panic | Result |
|---|---|---|---|
| owner | runtime floor + explicit `@abort()` | language checks + program declarations | the caller |
| carries | hard stop + payload, runs no code | `panic_handler(msg, code, file, line, col)` | a Result value |
| catchable | no | no — no unwinding exists | by ordinary control flow |

- A library shall not terminate on data-dependent conditions; termination belongs to language checks and explicit `@panic`. Expected failures return results.
- There is no exception machinery; every function is non-unwinding.

### 13.4 Checks are ordinary code

A run-time check is an ordinary comparison and branch to a panic block; it composes with user-written checks, and an implementation may eliminate a check proven redundant. An implementation shall not eliminate a check by assuming the checked condition cannot occur, shall not derive non-nullness from dereference (§11.3), and shall not fold floating-point arithmetic at compile time.

## 14. Termination: panic, panic_handler, and abort

### 14.1 The panic handler

- Exactly one panic handler is effective per compilation. A handler is a function annotated `@panic_handler(value)` where `value` is a compile-time integer constant. The effective handler is the surviving declaration with the largest value. By convention `-2` is the runtime's abort-wrapper tier, `-1` the runtime's per-platform tier, and `0` the user tier; the numbers are conventions, not reservations.
- After conditional-compilation pruning (§10.4), the compilation shall contain exactly one surviving `@panic_handler` declaration; otherwise the program is ill-formed. Two surviving declarations with the same value are ill-formed.
- The implementation injects no implicit handler: a compilation whose closure contains none is ill-formed.
- The handler signature is `func(msg: String8, code: u32, file: String8, line: u32, col: u32)`; it shall not allocate and shall not panic, and it never returns. Hosted implementations print and exit with code `100 + (code & 0xFF)`; the text carries a `[CODE]` prefix.

### 14.2 Panic

`@panic(msg)` / `@panic(msg, code)` is a program-declared termination. The implementation synthesizes the remaining handler arguments from `@source_file` / `@source_line` / `@source_col`. The call is a control-flow endpoint. Every panic — program-declared or emitted by a language check — calls the effective handler and is followed by unreachable control flow.

### 14.3 Abort

`@abort()` terminates unconditionally without calling any code. Abort is also the defined outcome of: a panic raised while a panic handler is executing (re-entrancy), a stack-overflow fault, and fault events in contexts (such as interrupt handlers) where the handler cannot run.

## 15. Program entry

Four forms of `main` are well-formed:

```
func main(args: Slice<String8>): i32 { ... return 0; }
func main(args: Slice<String8>)      { ... }
func main(): i32                     { ... return 0; }
func main()                          { ... }
```

- `args` carries the program arguments; an `i32` return value is the exit code; a `main` without a return value exits with code 0.
- Any other parameter or return shape is ill-formed. A package with `kind = "bin"` without a conforming `main` is ill-formed. Entry rules apply only to `kind = "bin"`; in a library, `main` is an ordinary function.
- The toolchain generates the entry code; on bare metal `args` is an empty slice.
