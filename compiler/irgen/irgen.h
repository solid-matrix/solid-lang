/**
 * @file irgen.h
 * @brief IR generator: annotated AST -> LLVM IR.
 * @author solid-matrix
 * @version 0.0.5
 */

#ifndef SOLID_IRGEN_IRGEN_H
#define SOLID_IRGEN_IRGEN_H

#include "ast.h"

 /**
  * @brief An opaque handle to generated IR.
  *
  * Holds an LLVMModuleRef internally; LLVM details never leak to callers.
  */
typedef struct SolidIrgenModule SolidIrgenModule;

// TODO: SolidIrgenGenerate(SolidIrgenModule **out, SolidAstNode *root)
// TODO: SolidIrgenEmitIRText(SolidIrgenModule *module, ...)
// TODO: SolidIrgenEmitBitcode / SolidIrgenEmitObject
// TODO: SolidIrgenDispose(SolidIrgenModule *module)

#endif /* SOLID_IRGEN_IRGEN_H */
