#include "parse_internal.h"
#include "source.h"
#include "span.h"
#include "strview.h"
#include "test_support.h"

/* ---- character classes ---------------------------------------------- */

void test_digit_classes(void) {
  TEST_ASSERT_TRUE(is_decimal_digit('0') && is_decimal_digit('9'));
  TEST_ASSERT_FALSE(is_decimal_digit('a'));
  TEST_ASSERT_FALSE(is_decimal_digit('/'));

  TEST_ASSERT_TRUE(is_binary_digit('0') && is_binary_digit('1'));
  TEST_ASSERT_FALSE(is_binary_digit('2'));

  TEST_ASSERT_TRUE(is_octal_digit('0') && is_octal_digit('7'));
  TEST_ASSERT_FALSE(is_octal_digit('8'));

  TEST_ASSERT_TRUE(is_hex_digit('0') && is_hex_digit('9'));
  TEST_ASSERT_TRUE(is_hex_digit('a') && is_hex_digit('F'));
  TEST_ASSERT_FALSE(is_hex_digit('g'));

  TEST_ASSERT_TRUE(is_base_digit('1', 2));
  TEST_ASSERT_FALSE(is_base_digit('2', 2));
  TEST_ASSERT_TRUE(is_base_digit('f', 16));
}

void test_letter_classes(void) {
  TEST_ASSERT_TRUE(is_letter_or_underscore('a'));
  TEST_ASSERT_TRUE(is_letter_or_underscore('_'));
  TEST_ASSERT_FALSE(is_letter_or_underscore('0'));

  // Word characters continue an identifier; digits qualify there.
  TEST_ASSERT_TRUE(is_letter_digit_or_underscore('0'));
  TEST_ASSERT_FALSE(is_letter_digit_or_underscore('.'));
}

void test_whitespace_class(void) {
  TEST_ASSERT_TRUE(is_whitespace(' '));
  TEST_ASSERT_TRUE(is_whitespace('\t'));
  TEST_ASSERT_TRUE(is_whitespace('\n'));
  TEST_ASSERT_TRUE(is_whitespace('\r'));
  TEST_ASSERT_FALSE(is_whitespace('x'));
}

/* ---- trivia ---------------------------------------------------------- */

static Span span_over(const char *text) { return (Span){.start = 0, .end = strlen(text)}; }

void test_skip_trivia_consumes_runs(void) {
  static const char *const TEXT = " \t\n// note\nx";
  Source *s = source_from_cstr(TEXT);
  Span rem = skip_trivia(s, span_over(TEXT));
  TEST_ASSERT_EQUAL_size_t(strlen(" \t\n// note\n"), rem.start);
  TEST_ASSERT_EQUAL_UINT8('x', source_byte_at(s, rem.start));
  source_destroy(s);
}

void test_skip_trivia_on_code_is_noop(void) {
  Source *s = source_from_cstr("abc");
  Span rem = skip_trivia(s, span_over("abc"));
  TEST_ASSERT_EQUAL_size_t(0, rem.start);
  source_destroy(s);
}

void test_skip_trivia_at_end(void) {
  Source *s = source_from_cstr("  ");
  Span rem = skip_trivia(s, span_over("  "));
  TEST_ASSERT_TRUE(span_is_empty(rem));
  source_destroy(s);
}

/* ---- span / keyword helpers ------------------------------------------ */

void test_span_consumed_measures_progress(void) {
  Span from = {.start = 3, .end = 10};
  Span to = {.start = 7, .end = 10};
  Span consumed = span_consumed(from, to);
  TEST_ASSERT_EQUAL_size_t(3, consumed.start);
  TEST_ASSERT_EQUAL_size_t(7, consumed.end);
  TEST_ASSERT_EQUAL_size_t(4, span_len(consumed));
}

void test_match_keyword_positive_and_negative(void) {
  static const char *const TEXT = "namespace";
  Source *s = source_from_cstr(TEXT);

  TEST_ASSERT_TRUE(match_keyword(s, span_over(TEXT), STRVIEW("namespace")));
  TEST_ASSERT_FALSE(match_keyword(s, span_over(TEXT), STRVIEW("using")));
  TEST_ASSERT_FALSE(match_keyword(s, span_over(TEXT + 1), STRVIEW("namespace")));

  // A longer identifier that merely starts with the keyword does not
  // match: the boundary byte must not be a word character.
  static const char *const GLUED = "namespacex";
  Source *g = source_from_cstr(GLUED);
  TEST_ASSERT_FALSE(match_keyword(g, span_over(GLUED), STRVIEW("namespace")));

  source_destroy(s);
  source_destroy(g);
}

/* ---- longest-match selection ----------------------------------------- */

void test_complete_longest_match_picks_furthest_rem(void) {
  ParserResult results[3];
  results[0] = parser_result_matched((Span){.start = 2}, NULL, NULL);
  results[1] = parser_result_not_match((Span){.start = 0});
  results[2] = parser_result_matched((Span){.start = 5}, NULL, NULL);

  ParserResult best = complete_longest_match(results, 3);
  TEST_ASSERT_TRUE(best.matched);
  TEST_ASSERT_EQUAL_size_t(5, best.rem.start);

  // Ties keep the earliest alternative.
  results[2].rem.start = 2;
  best = complete_longest_match(results, 3);
  TEST_ASSERT_EQUAL_size_t(2, best.rem.start);
}

static const TestDispatchEntry ENTRIES[] = {
    {"digit_classes", test_digit_classes},
    {"letter_classes", test_letter_classes},
    {"whitespace_class", test_whitespace_class},
    {"skip_trivia_consumes_runs", test_skip_trivia_consumes_runs},
    {"skip_trivia_on_code_is_noop", test_skip_trivia_on_code_is_noop},
    {"skip_trivia_at_end", test_skip_trivia_at_end},
    {"span_consumed_measures_progress", test_span_consumed_measures_progress},
    {"match_keyword_positive_and_negative", test_match_keyword_positive_and_negative},
    {"complete_longest_match_picks_furthest_rem", test_complete_longest_match_picks_furthest_rem},
};

TEST_DISPATCH_MAIN(ENTRIES)
