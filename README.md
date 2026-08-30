# The SOLID Programming Language

> The SOLID Programming Language, hereinafter referred to as SOLID, is a purely static, strongly explicit systems programming language.

This project covers SOLID's language design, compiler development, and standard library development.

Version: 0.0.5

## Project Structure

```
solid-lang/
├── compiler/   # The compiler
├── doc/        # Language design documents
├── std/        # Standard library
├── examples/   # Project examples
├── LICENSE
└── README.md
```

## Design Documents

```
doc/
├── notes.md         # Notes
├── spec.md          # Specification
├── manual.md        # User manual
├── syntax.md        # Syntax parser specification
├── semantic.md      # Semantic analyzer specification
└── ir-gen.md   # IR generator specification
```

- [Notes](./doc/notes.md): A development log that tracks ephemeral design choices, debugging histories, and incremental implementation tactics.
- [Specification](./doc/spec.md): The formal, unchanging contract that exhaustively defines the language's lexical structure, syntactic forms, and operational semantics.
- [User Manual](./doc/manual.md): A practical, high-level guide for end-users that explains installation, toolchain invocation, and idiomatic usage of the language.
- [Syntax Parser Document](./doc/syntax.md): Implementation-focused documentation for the parser, detailing tokenization, grammar-rule application, and Abstract Syntax Tree (AST) construction.
- [Semantic Analyzer Document](./doc/semantic.md): Implementation-focused documentation covering name resolution, type inference, scoping rules, and static semantic validation over the generated AST.
- [IR Generator Document](./doc/ir-gen.md): Implementation-focused documentation outlining the transformation pipeline that translates the decorated AST into a target-agnostic intermediate representation.

## Compiler

```
compiler/
├── common/             # Foundation: Strview, Arena, Source
├── syntax/             # AST definitions + lexing + parsing
├── sema/               # Semantic analysis
├── irgen/              # LLVM IR generation (the only LLVM C API user)
└── cli/                # The `solid` executable (entry point)
```

Dependency chain: `common <- syntax <- sema <- irgen <- cli`; CMake targets are named `solid-lang-*`.

### Requirements

- CMake >= 3.10
- LLVM with the C API available; the `LLVM_DIR` environment variable must point at the install location
- Windows: MSVC (Visual Studio); Linux: GCC or Clang

### Building and Testing

```bash
# linux-x64

# export LLVM_DIR=[path_to_llvm]
cmake --preset linux-x64-debug
cmake --build --preset linux-x64-debug
ctest --test-dir .build/linux-x64-debug --output-on-failure
```

```powershell
# windows-x64

# set LLVM_DIR=[path_to_llvm]
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug
ctest --test-dir .build\windows-x64-debug\ --output-on-failure
```

Executables go to `<build>/bin/`; on Windows the runtime `LLVM-C.dll` is copied next to the executable. Static libraries stay in their own subdirectories.

### Code Conventions

- C17;
- Encoding: UTF-8 without BOM;
- Comments: Doxygen JavaDoc; file headers carry `@file`, `@brief`, `@author`, `@version`; all in English

## Standard Library

```
std/   # To be developed
```

## Examples

```
examples/   # To be added
```
