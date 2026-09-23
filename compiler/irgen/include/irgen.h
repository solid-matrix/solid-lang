/**
 * @file irgen.h
 * @brief IR generation: the seed module and object emission. The LLVM C API
 *        stays private to this library; clients hold an opaque handle.
 * @author solid-matrix
 */

#pragma once

/**
 * @brief Which startup-contract shape the seed module takes
 *        (.draft/cli.md §2, .draft/spec.md §2.13).
 */
typedef enum {
  /** crt chain: `main(i32, ptr, ptr)` adapter -> user function. */
  IRGEN_SEED_HOSTED,
  /** freestanding on a Linux-family target: `_start` -> user -> raw exit
   *  syscall in `_solid_exit` (no libc involved). */
  IRGEN_SEED_FREESTANDING_LINUX,
  /** freestanding bare metal: `_start` -> user -> architecture halt
   *  sequence in `_solid_exit` (cli/hlt on x86_64, code in %eax). */
  IRGEN_SEED_FREESTANDING_BARE,
} IrgenSeedKind;

/**
 * @brief Opaque seed module: owns the LLVM context, module, and target
 *        machine. Created by irgen_build_min_module(), released exactly
 *        once with irgen_dispose_module().
 */
typedef struct IrgenModule IrgenModule;

/**
 * @brief Registers the native LLVM target and assembly printer.
 * @return 0 on success; nonzero when registration fails.
 */
int irgen_init(void);

/**
 * @brief Builds the seed module over a default triple. The user-level
 *        entry `solid_user_main` (hidden visibility, returns 13) is
 *        shared by every shape; @p kind picks the startup skeleton:
 *        - IRGEN_SEED_HOSTED: C ABI `main(i32 argc, ptr argv, ptr envp)`
 *          adapter, default visibility, ignores its parameters (v0: the
 *          `()` entry shape needs no argv assembly), calls the user
 *          function and returns its code;
 *        - IRGEN_SEED_FREESTANDING_*: `_start` assembles an empty args
 *          slice (v0: nothing to do for the `()` shape), calls the user
 *          function and tail-calls `_solid_exit(code)` — the raw exit
 *          syscall on Linux-family targets, the halt sequence on bare
 *          metal.
 * @param kind The startup-contract shape to emit.
 * @return The new module, or NULL on failure (diagnostics on stderr).
 */
IrgenModule *irgen_build_min_module(IrgenSeedKind kind);

/**
 * @brief Releases a module created by irgen_build_min_module().
 * @param module The module to dispose; may be NULL.
 */
void irgen_dispose_module(IrgenModule *module);

/**
 * @brief Emits the module as a relocatable PIC object file.
 * @param module A module from irgen_build_min_module().
 * @param path Destination file path, overwritten.
 * @return 0 on success; nonzero on failure (diagnostics on stderr).
 */
int irgen_emit_object(IrgenModule *module, const char *path);
