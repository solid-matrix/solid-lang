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
├── note.md       # Designing notes
├── spec.md       # Overall specification
├── syntax.md     # Syntax specification
├── semantic.md   # Semantics specification
└── guide.md      # Usage guide
```

## Compiler

```
compiler/
├── common/             # Foundation: StringView, Arena, Source
├── parser/             # AST definitions + lexing + parsing
├── sema/               # Semantic analysis
├── irgen/              # LLVM IR generation (the only LLVM C API user)
└── cli/                # The `solid` executable (entry point)
```

Dependency chain: `common <- parser <- sema <- irgen <- cli`; CMake targets are named `solid-lang-*`.

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
