/**
 * @file main.c
 * @brief CLI entry point, orchestrates the compilation pipeline.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "irgen.h"
#include "parser.h"
#include "source.h"
#include "syntax_node.h"
#include <stdio.h>

const char *source_code = "namespace std::math;\n"
                          "using std::core;\n"
                          "func main():i32{\n"
                          "  return 0;\n"
                          "}\n";

SyntaxProgram *parse(const Parser *parser) {
  ParserResult parser_res =
      parse_program(parser, source_get_span(parser->source));

  if (parser_res.matched == false || parser_res.node == NULL ||
      parser_res.node->kind != SYNTAX_KIND_PROGRAM) {
    fprintf(stderr, "failed to parse program\n");

    return NULL;
  }

  if (!syntax_errorlist_is_empty(parser_res.errors)) {
    for (const SyntaxErrorListNode *n = parser_res.errors->head; n != NULL;
         n = n->next)
      fprintf(stderr, "error code: 0x%x\n", n->error.code);

    syntax_errorlist_destroy(parser_res.errors);
    return NULL;
  }

  return (SyntaxProgram *)parser_res.node;
}

int main(int argc, char *argv[]) {
  Source *source = source_from_cstr(source_code);
  Parser *parser = parser_create(source);

  SyntaxProgram *program = parse(parser);
  if (program != NULL) {
    printf("top level declaration count = %zu\n", program->top_levels.len);
  }

  // int _ = irgen_example();
  parser_destroy(parser);
  source_destroy(source);
  return 0;
}
