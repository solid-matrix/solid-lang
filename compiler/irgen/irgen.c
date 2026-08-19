/**
 * @file irgen.c
 * @brief IR generation implementation (LLVM C API).
 * @author solid-matrix
 * @version 0.0.5
 */

#include "irgen.h"

#include <stdio.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Error.h>

int irgen_example(void)
{
    LLVMLinkInMCJIT();
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("my_module", context);

    LLVMTypeRef param_types[] = {
        LLVMInt32TypeInContext(context),
        LLVMInt32TypeInContext(context)};
    LLVMTypeRef return_type = LLVMInt32TypeInContext(context);
    LLVMTypeRef function_type = LLVMFunctionType(return_type, param_types, 2, 0);

    LLVMValueRef function = LLVMAddFunction(module, "sum", function_type);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");

    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef param_a = LLVMGetParam(function, 0);
    LLVMValueRef param_b = LLVMGetParam(function, 1);
    LLVMValueRef result = LLVMBuildAdd(builder, param_a, param_b, "result");

    LLVMBuildRet(builder, result);

    char *error = NULL;
    if (LLVMVerifyModule(module, LLVMAbortProcessAction, &error))
    {
        fprintf(stderr, "llvm failed to verify module: %s\n", error);
        LLVMDisposeMessage(error);
        LLVMDisposeBuilder(builder);
        LLVMDisposeModule(module);
        LLVMContextDispose(context);
        return 1;
    }

    printf("llvm ir generated:\n");
    LLVMDumpModule(module);

    LLVMExecutionEngineRef engine;
    char *engine_error = NULL;
    if (LLVMCreateExecutionEngineForModule(&engine, module, &engine_error) != 0)
    {
        fprintf(stderr, "llvm failed to create execution engine for module: %s\n", engine_error);
        LLVMDisposeMessage(engine_error);
        LLVMDisposeBuilder(builder);
        LLVMContextDispose(context);
        return 1;
    }

    int (*sum_func)(int, int) = (int (*)(int, int))LLVMGetFunctionAddress(engine, "sum");
    int x = 10, y = 20;
    int result_value = sum_func(x, y);

    printf("\nsum(%d, %d) = %d\n", x, y, result_value);

    LLVMDisposeBuilder(builder);
    LLVMDisposeExecutionEngine(engine);
    LLVMContextDispose(context);
    return 0;
}