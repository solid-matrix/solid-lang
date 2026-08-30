#include "parse.h"
#include "parser_fixture.h"
#include "source.h"
#include "test_support.h"

void test_parser_borrows_caller_state(void) {
  fx_begin("abc");
  TEST_ASSERT_TRUE(fx_parser->source == fx_source);
  TEST_ASSERT_TRUE(fx_parser->arena == fx_arena);
  // The value parser borrows the source; parsing through it works.
  SyntaxNodeResult r = parse_identifier(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
}

void test_refreeze_replaces_state(void) {
  fx_begin("a");
  SyntaxNodeResult r1 = parse_identifier(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r1.matched);

  fx_begin(";"); // fixture releases the previous parse first
  TEST_ASSERT_TRUE(fx_parser->source == fx_source);
  TEST_ASSERT_TRUE(fx_parser->arena == fx_arena);
  SyntaxNodeResult r2 = parse_identifier(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_FALSE(r2.matched); // ';' starts nothing
  SyntaxNodeResult r3 = parse_program(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r3.matched); // always matched; the junk tail reports EXPECTED_EOF
}

void test_cycle_is_repeatable(void) {
  for (int i = 0; i < 64; i++) {
    fx_begin("namespace a;");
    SyntaxNodeResult r = parse_program(fx_parser, source_get_span(fx_source));
    TEST_ASSERT_TRUE(r.matched);
    fx_release(); // sanitizer turns leaks/double-frees into failures
  }
}

static const TestDispatchEntry ENTRIES[] = {
    {"parser_borrows_caller_state", test_parser_borrows_caller_state},
    {"refreeze_replaces_state", test_refreeze_replaces_state},
    {"cycle_is_repeatable", test_cycle_is_repeatable},
};

TEST_DISPATCH_MAIN(ENTRIES)
