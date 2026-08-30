/**
 * @file parse_aux.c
 * @brief Internal helper implementations.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "error.h"
#include "node.h"
#include "parse.h"
#include "source.h"
#include "span.h"
#include "strview.h"
#include "syntax_error.h"

bool is_letter_or_underscore(uint8_t c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }

bool is_letter_digit_or_underscore(uint8_t c) { return is_letter_or_underscore(c) || (c >= '0' && c <= '9'); }

bool is_decimal_digit(uint8_t c) { return c >= '0' && c <= '9'; }

bool is_binary_digit(uint8_t c) { return c == '0' || c == '1'; }

bool is_octal_digit(uint8_t c) { return c >= '0' && c <= '7'; }

bool is_hex_digit(uint8_t c) { return is_decimal_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

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

bool is_whitespace(uint8_t c) { return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == '\n'; }

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
      while (i < span.end && source_byte_at(source, i) != '\n' && source_byte_at(source, i) != '\r')
        i += 1;

      continue;
    }

    break;
  }

  return (Span){.start = i, .end = span.end};
}

Span span_consumed(Span span, Span rem) {
  assert(span.start <= rem.start && rem.start <= span.end);
  return (Span){.start = span.start, .end = rem.start};
}

SyntaxNodeResult complete_longest_match(SyntaxNodeResult *results, size_t count) {
  assert(count > 0);

  size_t selected = 0;

  for (size_t i = 1; i < count; i++) {
    if (results[i].rem.start > results[selected].rem.start) {
      selected = i;
    }
  }

  return results[selected];
}

SyntaxListResult parse_expr_list(const SyntaxParser *parser, Span span, Strview separator) {
  Span rem = span;
  SyntaxNodeList *list = syntax_nodelist_empty();
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult res = parse_expr(parser, rem);
  if (res.matched) {
    list = syntax_nodelist_prepend(parser->arena, list, res.node);
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    rem = res.rem;

    while (true) {
      SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), separator);
      if (!mres.matched)
        break;

      rem = mres.rem;

      res = parse_expr(parser, skip_trivia(parser->source, rem));
      if (!res.matched) {
        SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
        errors = syntax_errorlist_prepend(parser->arena, errors, error);
      } else {
        list = syntax_nodelist_prepend(parser->arena, list, res.node);
        errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
        rem = res.rem;
      }
    }
  }

  return (SyntaxListResult){.list = syntax_nodelist_reverse(parser->arena, list), .errors = errors, .rem = rem};
}

SyntaxListResult parse_identifier_list(const SyntaxParser *parser, Span span, Strview separator) {
  Span rem = span;
  SyntaxNodeList *list = syntax_nodelist_empty();
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult res = parse_identifier(parser, rem);
  if (res.matched) {
    list = syntax_nodelist_prepend(parser->arena, list, res.node);
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    rem = res.rem;

    while (true) {
      SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), separator);
      if (!mres.matched)
        break;

      rem = mres.rem;

      res = parse_identifier(parser, skip_trivia(parser->source, rem));
      if (!res.matched) {
        SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem);
        errors = syntax_errorlist_prepend(parser->arena, errors, error);
      } else {
        list = syntax_nodelist_prepend(parser->arena, list, res.node);
        errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
        rem = res.rem;
      }
    }
  }

  return (SyntaxListResult){.list = syntax_nodelist_reverse(parser->arena, list), .errors = errors, .rem = rem};
}

SyntaxListResult parse_generic_param_list(const SyntaxParser *parser, Span span) {
  Span rem = span;
  SyntaxNodeList *list = syntax_nodelist_empty();
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult res = parse_generic_param(parser, rem);
  if (res.matched) {
    list = syntax_nodelist_prepend(parser->arena, list, res.node);
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    rem = res.rem;

    while (true) {
      SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
      if (!mres.matched)
        break;

      rem = mres.rem;

      res = parse_generic_param(parser, skip_trivia(parser->source, rem));
      if (!res.matched) {
        SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem);
        errors = syntax_errorlist_prepend(parser->arena, errors, error);
      } else {
        list = syntax_nodelist_prepend(parser->arena, list, res.node);
        errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
        rem = res.rem;
      }
    }
  }

  return (SyntaxListResult){.list = syntax_nodelist_reverse(parser->arena, list), .errors = errors, .rem = rem};
}

SyntaxListResult parse_call_param_list(const SyntaxParser *parser, Span span) {
  Span rem = span;
  SyntaxNodeList *list = syntax_nodelist_empty();
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult res = parse_call_param(parser, rem);
  if (res.matched) {
    list = syntax_nodelist_prepend(parser->arena, list, res.node);
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    rem = res.rem;

    while (true) {
      SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
      if (!mres.matched)
        break;

      rem = mres.rem;

      res = parse_call_param(parser, skip_trivia(parser->source, rem));
      if (!res.matched) {
        SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem);
        errors = syntax_errorlist_prepend(parser->arena, errors, error);
      } else {
        list = syntax_nodelist_prepend(parser->arena, list, res.node);
        errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
        rem = res.rem;
      }
    }
  }

  return (SyntaxListResult){.list = syntax_nodelist_reverse(parser->arena, list), .errors = errors, .rem = rem};
}

SyntaxListResult parse_generic_arg_list(const SyntaxParser *parser, Span span) {
  Span rem = span;
  SyntaxNodeList *list = syntax_nodelist_empty();
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult res = parse_generic_arg(parser, rem);
  if (res.matched) {
    list = syntax_nodelist_prepend(parser->arena, list, res.node);
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    rem = res.rem;

    while (true) {
      SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
      if (!mres.matched)
        break;

      rem = mres.rem;

      res = parse_generic_arg(parser, skip_trivia(parser->source, rem));
      if (!res.matched) {
        SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem);
        errors = syntax_errorlist_prepend(parser->arena, errors, error);
      } else {
        list = syntax_nodelist_prepend(parser->arena, list, res.node);
        errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
        rem = res.rem;
      }
    }
  }

  return (SyntaxListResult){.list = syntax_nodelist_reverse(parser->arena, list), .errors = errors, .rem = rem};
}

SyntaxListResult parse_field_list(const SyntaxParser *parser, Span span, SyntaxFieldFn parse_field,
                                  SyntaxErrorCode missing_code) {
  Span rem = span;
  SyntaxNodeList *list = syntax_nodelist_empty();
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult res = parse_field(parser, rem);
  if (res.matched) {
    rem = res.rem;
    list = syntax_nodelist_prepend(parser->arena, list, res.node);
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);

    while (true) {
      SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
      if (!mres.matched)
        break;
      rem = mres.rem;

      if (match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE).matched)
        break;

      res = parse_field(parser, skip_trivia(parser->source, rem));
      if (!res.matched) {
        errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(missing_code, rem));
        break;
      }

      rem = res.rem;
      list = syntax_nodelist_prepend(parser->arena, list, res.node);
      errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    }
  }

  return (SyntaxListResult){.list = syntax_nodelist_reverse(parser->arena, list), .errors = errors, .rem = rem};
}

SyntaxNodeResult parse_body_position(const SyntaxParser *parser, Span span) {
  Span adv = skip_trivia(parser->source, span);

  SyntaxNodeResult results[] = {
      parse_body_stmt(parser, adv),
      parse_empty_stmt(parser, adv),
  };
  SyntaxNodeResult res = complete_longest_match(results, COUNT_OF(results));
  if (res.matched)
    return res;

  SyntaxErrorList *errors = syntax_errorlist_prepend(parser->arena, syntax_errorlist_empty(),
                                                     syntax_error_create(SYNTAX_EXPECTED_BODY, span));
  return syntax_node_result_matched(span, NULL, errors);
}

SyntaxMatchResult match_keyword(const SyntaxParser *parser, Span span, Strview keyword) {
  if (keyword.len == 0 || keyword.len > span_len(span)) {
    return (SyntaxMatchResult){.matched = false, .rem = span, .errors = syntax_errorlist_empty()};
  }

  Strview token = source_strview_at(parser->source, span_slice(span, 0, keyword.len));
  if (!strview_equals(keyword, token))
    return (SyntaxMatchResult){.matched = false, .rem = span, .errors = syntax_errorlist_empty()};

  Span rem = span_advance(span, keyword.len);
  if (!span_is_empty(rem) && is_letter_digit_or_underscore(source_byte_at(parser->source, rem.start)))
    return (SyntaxMatchResult){.matched = false, .rem = span, .errors = syntax_errorlist_empty()};

  return (SyntaxMatchResult){.matched = true, .rem = rem, .errors = syntax_errorlist_empty()};
}

SyntaxMatchResult match(const SyntaxParser *parser, Span span, Strview strview) {
  if (strview.len == 0 || strview.len > span_len(span))
    return (SyntaxMatchResult){.matched = false, .rem = span, .errors = syntax_errorlist_empty()};

  Strview token = source_strview_at(parser->source, span_slice(span, 0, strview.len));
  if (!strview_equals(strview, token))
    return (SyntaxMatchResult){.matched = false, .rem = span, .errors = syntax_errorlist_empty()};

  return (SyntaxMatchResult){
      .matched = true, .rem = span_advance(span, strview.len), .errors = syntax_errorlist_empty()};
}

uint32_t hex_value(uint8_t d) { return is_decimal_digit(d) ? (uint32_t)(d - '0') : (uint32_t)((d | 0x20) - 'a' + 10); }
