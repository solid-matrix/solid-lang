#pragma once

#include "parser.h"
#include "xmem.h"

bool is_letter_or_underscore(uint8_t c);

bool is_letter_digit_or_underscore(uint8_t c);

bool is_decimal_digit(uint8_t c);

bool is_binary_digit(uint8_t c);

bool is_octal_digit(uint8_t c);

bool is_hex_digit(uint8_t c);

bool is_base_digit(uint8_t c, int base);

bool is_whitespace(uint8_t c);

Span skip_trivia(const Source *source, Span span);

bool match_keyword(const Source *source, Span span, Strview keyword);

/**
 * @brief The part of @p span consumed before reaching its remainder
 *        @p rem: [span.start, rem.start).
 *
 * Parser-contract term rather than general span algebra (see
 * ParserResult: "rem.start - span.start is exactly the consumed
 * length"), which is why it lives here and not in common/span.h.
 * Together span_consumed(span, rem) ++ rem rebuilds span whenever rem
 * ends at span.end.
 * @param span The original working span.
 * @param rem The unconsumed remainder of span.
 *            Asserts: span.start <= rem.start <= span.end.
 * @return The consumed prefix.
 */
Span span_consumed(Span span, Span rem);

ParserResult complete_longest_match(ParserResult *results, size_t count);
