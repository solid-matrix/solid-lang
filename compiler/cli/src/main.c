/**
 * @file main.c
 * @brief CLI entry point, orchestrates the compilation pipeline.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdio.h>

#include "main.h"

const char *source_code = "namespace std::math;\n"
                          "using std::core;\n"
                          "func main():i32{\n"
                          "  return 0;\n"
                          "}\n";

SyntaxProgram *parse(const SyntaxParser *parser) {
  SyntaxNodeResult parser_res = parse_program(parser, source_get_span(parser->source));

  if (parser_res.matched == false || parser_res.node == NULL || parser_res.node->kind != SYNTAX_KIND_PROGRAM) {
    fprintf(stderr, "failed to parse program\n");

    return NULL;
  }

  if (parser_res.errors != NULL) {
    for (const SyntaxErrorList *n = parser_res.errors; n != NULL; n = n->next)
      fprintf(stderr, "error code: 0x%x\n", n->error.code);

    return NULL; // the list dies with the parse arena
  }

  return (SyntaxProgram *)parser_res.node;
}

int main(int argc, char *argv[]) {
  Source *source = source_from_cstr(source_code);
  SyntaxParser *parser = syntax_parser_create(source);

  SyntaxProgram *program = parse(parser);
  if (program != NULL) {
    size_t count = 0;
    for (const SyntaxNodeList *n = program->top_levels; n != NULL; n = n->next)
      count++;
    printf("top level declaration count = %zu\n", count);
  }

  // int _ = irgen_example();
  syntax_parser_destroy(parser);
  source_destroy(source);
  return 0;
}
