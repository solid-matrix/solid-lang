/**
 * @file main.c
 * @brief CLI entry point, orchestrates the compilation pipeline.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "ast.h"
#include "irgen.h"
#include "parser.h"
#include "source.h"
#include <stdio.h>

const char *source_code = "namespace std::math;\n"
                          "using std::core;\n"
                          "func main():i32{\n"
                          "  return 0;\n"
                          "}\n";

int main(int argc, char *argv[]) {
  Source source = source_from_cstr(source_code);
  Parser parser = parser_create(&source);

  ParserResult parser_res = parse_program(&parser, source_get_span(&source));

  if (parser_res.matched == false || parser_res.node == NULL ||
      parser_res.node->kind != SYNTAX_KIND_PROGRAM) {
    fprintf(stderr, "failed to parse program");

    parser_destroy(&parser);
    source_destroy(&source);
    return 1;
  }

  if (parser_res.errors != NULL) {
    SyntaxErrorList *node = parser_res.errors;

    while (node != NULL) {
      fprintf(stderr, "error code: 0x%x", node->error.code);
      node = node->next;
    }

    syntax_error_list_free(&parser_res.errors);
    parser_destroy(&parser);
    source_destroy(&source);
    return 2;
  }

  SyntaxProgram *program = (SyntaxProgram *)parser_res.node;

  printf("top level declaration count = %zu", program->top_levels.len);

  // int _ = irgen_example();
  parser_destroy(&parser);
  source_destroy(&source);
  return 0;
}
