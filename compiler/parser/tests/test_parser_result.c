#include "arena.h"
#include "parser.h"
#include "parser_result.h"
#include "test_support.h"

void test_matched_carries_fields(void) {
  ParserResult r =
      parser_result_matched((Span){.start = 4, .end = 9}, NULL, NULL);

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(4, r.rem.start);
  TEST_ASSERT_EQUAL_size_t(9, r.rem.end);
  TEST_ASSERT_NULL(r.node);   // passed through verbatim
  TEST_ASSERT_NULL(r.errors); // passed through verbatim
}

void test_not_match_resets_everything(void) {
  ParserResult r = parser_result_not_match((Span){.start = 1, .end = 5});

  TEST_ASSERT_FALSE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_NULL(r.errors); // convention: not-match carries no list
  TEST_ASSERT_EQUAL_size_t(1, r.rem.start);
  TEST_ASSERT_EQUAL_size_t(5, r.rem.end);
}

void test_is_ok_requires_match_and_silence(void) {
  TEST_ASSERT_TRUE(parser_result_is_ok(
      parser_result_matched((Span){.start = 0}, NULL, NULL)));

  Arena *a = arena_create();
  SyntaxErrorList *errs = syntax_errorlist_append(
      a, NULL,
      syntax_error_create(SYNTAX_EXPECTED_EOF, (Span){.start = 0}));
  TEST_ASSERT_FALSE(parser_result_is_ok(
      parser_result_matched((Span){.start = 0}, NULL, errs)));
  arena_destroy(a);

  TEST_ASSERT_FALSE(parser_result_is_ok(
      parser_result_not_match((Span){.start = 0})));
}

static const TestDispatchEntry ENTRIES[] = {
    {"matched_carries_fields", test_matched_carries_fields},
    {"not_match_resets_everything", test_not_match_resets_everything},
    {"is_ok_requires_match_and_silence",
     test_is_ok_requires_match_and_silence},
};

TEST_DISPATCH_MAIN(ENTRIES)
