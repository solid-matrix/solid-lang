/**
 * @file parser.c
 * @brief Parser implementation.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>

#include "mem.h"
#include "parser.h"

#pragma region PARSER

Parser parser_create(Source *source) {
  return (Parser){.source = source, .errors = NULL};
}

void parser_destroy(Parser *parser) {
  SyntaxErrorLinkedList *en = parser->errors;
  while (en != NULL) {
    SyntaxErrorLinkedList *next = en->next;
    xfree(en);
    en = next;
  }
}

void parser_append_error(Parser *parser, Span span, SyntaxErrorCode code) {
  SyntaxErrorLinkedList *en = xmalloc(sizeof(SyntaxErrorLinkedList));
  en->error = (SyntaxError){.code = code, .span = span};
  en->next = parser->errors;
  parser->errors = en;
}

#pragma endregion

#pragma region AUXILIARY

static inline bool is_letter_or_underscore(uint8_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static inline bool is_letter_digit_or_underscore(uint8_t c) {
  return is_letter_or_underscore(c) || (c >= '0' && c <= '9');
}

static inline bool is_space(uint8_t c) {
  return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' ||
         c == '\n';
}

static Span skip_trivia(const Source *source, Span span) {
  size_t i = span.start;

  while (i < span.end) {
    uint8_t c = source_byte_at(source, i);

    if (is_space(c)) {
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

#pragma endregion

#pragma region PARSE

SyntaxProgram *parse(Parser *parser) {
  Span span = source_get_span(parser->source);
  span = skip_trivia(parser->source, span);

  ParserResult res = parse_program(parser, span);

  assert(res.matched);
  assert(res.node->kind == SYNTAX_KIND_PROGRAM);

  return (SyntaxProgram *)res.node;
}

ParserResult parse_program(Parser *parser, Span span) {
  SyntaxProgram *program = xmalloc(sizeof(SyntaxProgram));

  *program = (SyntaxProgram){
      .header =
          {
              .kind = SYNTAX_KIND_PROGRAM,
              .span = {.start = span.start},
          },
      .top_levels = syntax_node_list_create(),
  };

  Span rem = span;
  ParserResult res;

  while (true) {
    res = parse_decl(parser, rem);
    rem = res.rem;

    if (!res.matched)
      break;

    if (res.node == NULL)
      continue;

    assert((res.node->kind & SYNTAX_KIND_DECL_MASK) != 0);
    syntax_node_list_append(&(program->top_levels), res.node);
  }

  if (program->top_levels.len == 0) {
    program->header.span.end = rem.start;
  } else {
    program->header.span.end =
        program->top_levels.nodes[program->top_levels.len - 1]->span.end;
  }

  rem = skip_trivia(parser->source, rem);

  if (span_len(rem) > 0) {
    parser_append_error(parser, rem, SYNTAX_EXPECTED_EOF);
  }

  return (ParserResult){
      .matched = true,
      .node = (SyntaxNode *)program,
      .rem = rem,
  };
}

ParserResult parse_identifier(Parser *parser, Span span) {
  if (span_is_empty(span))
    return (ParserResult){.matched = false, .node = NULL, .rem = span};

  uint8_t c = source_byte_at(parser->source, span.start);

  if (!is_letter_or_underscore(c))
    return (ParserResult){.matched = false, .node = NULL, .rem = span};

  size_t i = span.start + 1;

  while (i < span.end) {
    c = source_byte_at(parser->source, i);
    if (!is_letter_digit_or_underscore(c))
      break;

    i++;
  }

  Span consumed = {.start = span.start, .end = i};
  SyntaxIdentifier *id = xmalloc(sizeof(SyntaxIdentifier));
  *id = (SyntaxIdentifier){
      .header = {.kind = SYNTAX_KIND_IDENTIFIER, .span = consumed},
      .string_view = source_string_view_at(parser->source, consumed),
  };

  Span rem = skip_trivia(parser->source, (Span){.start = i, .end = span.end});

  return (ParserResult){
      .matched = true,
      .node = (SyntaxNode *)id,
      .rem = rem,
  };
}

#pragma endregion