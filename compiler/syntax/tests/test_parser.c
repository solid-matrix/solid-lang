#include "syntax_parses.h"
#include "parser_fixture.h"
#include "source.h"
#include "test_support.h"

void test_create_returns_usable_parser(void) {
  fx_begin("abc");
  TEST_ASSERT_NOT_NULL(fx_parser);
  // The parser borrows the source; parsing through it works.
  SyntaxNodeResult r = parse_identifier(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
}

void test_create_distinct_instances(void) {
  fx_begin("a");
  SyntaxParser *first = fx_parser;

  fx_begin("b"); // fixture releases the previous parse first
  TEST_ASSERT_NOT_EQUAL(first, fx_parser);
}

void test_destroy_cycle_is_repeatable(void) {
  for (int i = 0; i < 64; i++) {
    fx_begin("namespace a;");
    SyntaxNodeResult r = parse_program(fx_parser, source_get_span(fx_source));
    TEST_ASSERT_TRUE(r.matched);
    fx_release(); // sanitizer turns leaks/double-frees into failures
  }
}

static const TestDispatchEntry ENTRIES[] = {
    {"create_returns_usable_parser", test_create_returns_usable_parser},
    {"create_distinct_instances", test_create_distinct_instances},
    {"destroy_cycle_is_repeatable", test_destroy_cycle_is_repeatable},
};

TEST_DISPATCH_MAIN(ENTRIES)
