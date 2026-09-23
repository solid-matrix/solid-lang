/**
 * @file irgen.c
 * @brief IR generation implementation (LLVM C API).
 * @author solid-matrix
 */

#include "irgen.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "xmem.h"

/** Exit status of the user-level entry in the seed module. */
#define SEED_EXIT_CODE 13

struct IrgenModule {
  LLVMContextRef context;
  LLVMModuleRef module;
  LLVMTargetMachineRef machine;
  char *triple;
};

static int g_initialized = 0;

int irgen_init(void) {
  if (g_initialized) {
    return 0;
  }
  if (LLVMInitializeNativeTarget() != 0 || LLVMInitializeNativeAsmPrinter() != 0 ||
      LLVMInitializeNativeAsmParser() != 0) {
    fprintf(stderr, "irgen: failed to register the native LLVM target\n");
    return 1;
  }
  g_initialized = 1;
  return 0;
}

/** Prints an LLVM-owned error message to stderr and releases it. */
static void report_llvm_error(const char *what, char *message) {
  fprintf(stderr, "irgen: %s: %s\n", what, message != NULL ? message : "unknown error");
  if (message != NULL) {
    LLVMDisposeMessage(message);
  }
}

/** Builds the hidden user-level entry: i32 solid_user_main() { ret 13 }. */
static LLVMValueRef build_user_main(LLVMContextRef context, LLVMModuleRef module,
                                    LLVMTypeRef i32) {
  LLVMTypeRef user_type = LLVMFunctionType(i32, NULL, 0, 0);
  LLVMValueRef user = LLVMAddFunction(module, "solid_user_main", user_type);
  LLVMSetVisibility(user, LLVMHiddenVisibility);

  LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, user, "entry");
  LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
  LLVMPositionBuilderAtEnd(builder, entry);
  LLVMBuildRet(builder, LLVMConstInt(i32, SEED_EXIT_CODE, 0));
  LLVMDisposeBuilder(builder);
  return user;
}

/**
 * Builds the freestanding exit primitive: noreturn `void _solid_exit(u32)`.
 * The lowering dispatches on the target family (.draft/cli.md §2): the raw
 * exit syscall on Linux-family targets (no libc involved), the halt
 * sequence on bare metal with the code parked in %eax for QEMU/debuggers.
 * The exit code arrives in %edi (constraint "D"); %edi writes zero-extend,
 * so the full register reads the code.
 */
static LLVMValueRef build_exit_function(LLVMContextRef context, LLVMModuleRef module,
                                        LLVMTypeRef i32, IrgenSeedKind kind) {
  const char *asm_string = (kind == IRGEN_SEED_FREESTANDING_LINUX)
                               ? "movl $$60, %eax\nmovl $0, %edi\nsyscall"
                               : "movl $0, %eax\ncli\n1:\nhlt\njmp 1b";
  const char *constraints = (kind == IRGEN_SEED_FREESTANDING_LINUX)
                                ? "r,~{edi},~{rax},~{rcx},~{r11},~{memory}"
                                : "r,~{eax},~{memory}";

  LLVMTypeRef param_types[] = {i32};
  LLVMTypeRef exit_type = LLVMFunctionType(LLVMVoidTypeInContext(context), param_types, 1, 0);
  LLVMValueRef exit_fn = LLVMAddFunction(module, "_solid_exit", exit_type);
  unsigned noreturn_kind = LLVMGetEnumAttributeKindForName("noreturn", strlen("noreturn"));
  LLVMAttributeRef noreturn = LLVMCreateEnumAttribute(context, noreturn_kind, 0);
  LLVMAddAttributeAtIndex(exit_fn, LLVMAttributeFunctionIndex, noreturn);

  LLVMTypeRef asm_type =
      LLVMFunctionType(LLVMVoidTypeInContext(context), param_types, 1, 0);
  LLVMValueRef asm_decl =
      LLVMGetInlineAsm(asm_type, (char *)asm_string, strlen(asm_string), (char *)constraints,
                       strlen(constraints), true, false, LLVMInlineAsmDialectATT, false);

  LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, exit_fn, "entry");
  LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
  LLVMPositionBuilderAtEnd(builder, entry);
  LLVMValueRef args[] = {LLVMGetParam(exit_fn, 0)};
  LLVMBuildCall2(builder, asm_type, asm_decl, args, 1, "");
  LLVMBuildUnreachable(builder);
  LLVMDisposeBuilder(builder);
  return exit_fn;
}

/**
 * Builds the process entry `void _start()` (ld's default entry name):
 * calls the user function, then hands the code to the noreturn exit
 * primitive. v0: the `()` entry shape needs no args assembly.
 */
static void build_start(LLVMContextRef context, LLVMModuleRef module, LLVMValueRef user,
                        LLVMValueRef exit_fn) {
  LLVMTypeRef start_type = LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, 0);
  LLVMValueRef start = LLVMAddFunction(module, "_start", start_type);

  LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, start, "entry");
  LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
  LLVMPositionBuilderAtEnd(builder, entry);
  LLVMValueRef code = LLVMBuildCall2(builder, LLVMGlobalGetValueType(user), user, NULL, 0, "code");
  LLVMValueRef args[] = {code};
  LLVMBuildCall2(builder, LLVMGlobalGetValueType(exit_fn), exit_fn, args, 1, "");
  LLVMBuildUnreachable(builder);
  LLVMDisposeBuilder(builder);
}

IrgenModule *irgen_build_min_module(IrgenSeedKind kind) {
  IrgenModule *m = xcalloc(1, sizeof(*m));
  if (m == NULL) {
    return NULL;
  }
  m->context = LLVMContextCreate();
  m->module = LLVMModuleCreateWithNameInContext("solid", m->context);

  m->triple = LLVMGetDefaultTargetTriple();
  LLVMSetTarget(m->module, m->triple);

  LLVMTargetRef target = NULL;
  char *error = NULL;
  if (LLVMGetTargetFromTriple(m->triple, &target, &error) != 0) {
    report_llvm_error("unknown target triple", error);
    irgen_dispose_module(m);
    return NULL;
  }
  m->machine = LLVMCreateTargetMachine(target, m->triple, "generic", "",
                                       LLVMCodeGenLevelNone, LLVMRelocPIC,
                                       LLVMCodeModelDefault);
  if (m->machine == NULL) {
    fprintf(stderr, "irgen: failed to create target machine\n");
    irgen_dispose_module(m);
    return NULL;
  }
  LLVMTargetDataRef layout = LLVMCreateTargetDataLayout(m->machine);
  LLVMSetModuleDataLayout(m->module, layout);
  LLVMDisposeTargetData(layout);

  LLVMTypeRef i32 = LLVMInt32TypeInContext(m->context);
  LLVMValueRef user = build_user_main(m->context, m->module, i32);

  if (kind == IRGEN_SEED_HOSTED) {
    // C ABI adapter: the symbol crt1.o references by name. v0: the seed
    // uses the `()` entry shape, so argc/argv/envp are received and ignored.
    LLVMTypeRef param_types[] = {i32, LLVMPointerTypeInContext(m->context, 0),
                                 LLVMPointerTypeInContext(m->context, 0)};
    LLVMTypeRef main_type = LLVMFunctionType(i32, param_types, 3, 0);
    LLVMValueRef main_fn = LLVMAddFunction(m->module, "main", main_type);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(m->context, main_fn, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(m->context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef code =
        LLVMBuildCall2(builder, LLVMGlobalGetValueType(user), user, NULL, 0, "code");
    LLVMBuildRet(builder, code);
    LLVMDisposeBuilder(builder);
  } else {
    LLVMValueRef exit_fn = build_exit_function(m->context, m->module, i32, kind);
    build_start(m->context, m->module, user, exit_fn);
  }

  return m;
}

void irgen_dispose_module(IrgenModule *m) {
  if (m == NULL) {
    return;
  }
  if (m->machine != NULL) {
    LLVMDisposeTargetMachine(m->machine);
  }
  if (m->module != NULL) {
    LLVMDisposeModule(m->module);
  }
  if (m->context != NULL) {
    LLVMContextDispose(m->context);
  }
  if (m->triple != NULL) {
    LLVMDisposeMessage(m->triple);
  }
  xfree(m);
}

int irgen_emit_object(IrgenModule *m, const char *path) {
  if (m == NULL || path == NULL) {
    return 1;
  }
  char *error = NULL;
  if (LLVMVerifyModule(m->module, LLVMReturnStatusAction, &error) != 0) {
    report_llvm_error("module verification failed", error);
    return 1;
  }
  if (LLVMTargetMachineEmitToFile(m->machine, m->module, (char *)path,
                                  LLVMObjectFile, &error) != 0) {
    report_llvm_error("failed to emit object file", error);
    return 1;
  }
  return 0;
}
