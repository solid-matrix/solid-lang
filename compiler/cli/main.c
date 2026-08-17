/**
 * @file main.c
 * @brief CLI entry point, orchestrates the compilation pipeline.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "options.h"

// CLI entry point: orchestrates the whole compilation pipeline
// (read source -> parser -> sema -> irgen -> emit IR/bitcode/object).
int main(int argc, char *argv[]) {
  // TODO: parse command line arguments and drive the four subprojects.
  (void)argc;
  (void)argv;
  return 0;
}
