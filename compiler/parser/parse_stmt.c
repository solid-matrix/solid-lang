#include "parser.h"

ParserResult parse_stmt(const Parser *parser, Span span) {
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}
