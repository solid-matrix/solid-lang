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

bool match(const Source *source, Span span, Strview strview);

Span span_consumed(Span span, Span rem);

ParserResult complete_longest_match(ParserResult *results, size_t count);
