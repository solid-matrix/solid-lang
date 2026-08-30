/**
 * @file main.c
 * @brief CLI entry point, orchestrates the compilation pipeline.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdio.h>

#include "arena.h"
#include "source.h"
#include "syntax_parse.h"

const char *source_code = "namespace std::math;\n"
                          "using std::core;\n"
                          "func main():i32{\n"
                          "  return 0;\n"
                          "}\n";

int main(void) {
  Source *source = source_from_cstr(source_code);
  Arena *arena = arena_create();

  SyntaxParseResult result = syntax_parse(source, arena);

  for (const SyntaxErrorList *n = result.errors; n != NULL; n = n->next)
    fprintf(stderr, "error code: 0x%x\n", n->error.code);

  size_t count = 0;
  for (const SyntaxNodeList *n = result.program->top_levels; n != NULL; n = n->next)
    count++;
  printf("top level declaration count = %zu\n", count);

  // int _ = irgen_example();

  arena_destroy(arena);
  source_destroy(source);
  return 0;
}
