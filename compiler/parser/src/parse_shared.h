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

/**
 * @brief Matches @p keyword exactly at span.start, requiring an
 *        identifier boundary right after it ("in" does not match
 *        "input").
 *
 * Primitive matcher: only the matched and rem fields of the result are
 * meaningful — errors is always NULL and node is always NULL, so the
 * result must never be fed into combinator or error-merging machinery.
 *
 * @param parser The parser providing the source text.
 * @param span Position to test; leading trivia must already be skipped.
 * @param keyword The keyword text to match.
 * @return matched == true with rem just past the keyword, or the
 *         not-match outcome for @p span.
 */
ParserResult match_keyword(const Parser *parser, Span span, Strview keyword);

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