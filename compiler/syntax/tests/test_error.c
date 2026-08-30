#include "error.h"
#include "test_support.h"

void test_error_carries_code_and_span(void) {
  SyntaxError e = syntax_error_create(SYNTAX_EXPECTED_DIGIT, (Span){.start = 2, .end = 8});
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_DIGIT, e.code);
  TEST_ASSERT_EQUAL_size_t(2, e.span.start);
  TEST_ASSERT_EQUAL_size_t(8, e.span.end);
}
static const TestDispatchEntry ENTRIES[] = {
    {"error_carries_code_and_span", test_error_carries_code_and_span},
};

TEST_DISPATCH_MAIN(ENTRIES)
