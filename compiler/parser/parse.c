#include <assert.h>
#include <string.h>

#include "ast.h"
#include "parse.h"
#include "parser.h"
#include "span.h"

bool is_letter_or_underscore(uint8_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool is_letter_digit_or_underscore(uint8_t c) {
  return is_letter_or_underscore(c) || (c >= '0' && c <= '9');
}

bool is_decimal_digit(uint8_t c) { return c >= '0' && c <= '9'; }

bool is_binary_digit(uint8_t c) { return c == '0' || c == '1'; }

bool is_octal_digit(uint8_t c) { return c >= '0' && c <= '7'; }

bool is_hex_digit(uint8_t c) {
  return is_decimal_digit(c) || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

bool is_base_digit(uint8_t c, int base) {
  switch (base) {
  case 2:
    return is_binary_digit(c);
  case 8:
    return is_octal_digit(c);
  case 16:
    return is_hex_digit(c);
  default:
    return is_decimal_digit(c);
  }
}

bool is_whitespace(uint8_t c) {
  return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' ||
         c == '\n';
}

Span skip_trivia(const Source *source, Span span) {
  size_t i = span.start;

  while (i < span.end) {
    uint8_t c = source_byte_at(source, i);

    if (is_whitespace(c)) {
      i += 1;
      continue;
    }

    if (c == '/' && i + 1 < span.end && source_byte_at(source, i + 1) == '/') {
      i += 2;
      while (i < span.end && source_byte_at(source, i) != '\n' &&
             source_byte_at(source, i) != '\r')
        i += 1;

      continue;
    }

    break;
  }

  return (Span){.start = i, .end = span.end};
}

ParserResult parse_program(const Parser *parser, Span span) {

  span = skip_trivia(parser->source, span);

  SyntaxProgram *program = xmalloc(sizeof(SyntaxProgram));

  *program = (SyntaxProgram){
      .header = syntax_node_header(SYNTAX_KIND_PROGRAM, span_empty()),
      .top_levels = syntax_node_list_create(),
  };

  Span rem = span;
  SyntaxErrorList *errors = syntax_error_list_create();

  while (true) {
    // Layout between top-level declarations is this loop's duty; the
    // last skip also positions the SYNTAX_EXPECTED_EOF check below.
    rem = skip_trivia(parser->source, rem);

    ParserResult res = parse_decl(parser, rem);
    rem = res.rem;

    if (!res.matched)
      break;

    syntax_error_list_merge(&errors, &(res.errors));

    if (res.node == NULL)
      continue;

    assert((res.node->kind & SYNTAX_KIND_DECL_MASK) != 0);
    syntax_node_list_append(&(program->top_levels), res.node);
  }

  program->header.span = span_create(span.start, rem.start);

  rem = skip_trivia(parser->source, rem);

  if (span_len(rem) > 0)
    syntax_error_list_append(&errors,
                             syntax_error_create(SYNTAX_EXPECTED_EOF, rem));

  return parser_result_matched(rem, (SyntaxNode *)program, errors);
}