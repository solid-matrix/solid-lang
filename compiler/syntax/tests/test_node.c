#include "syntax_node.h"
#include "test_support.h"

void test_header_carries_kind_and_span(void) {
  SyntaxNode n = syntax_node_create(SYNTAX_KIND_PROGRAM, (Span){.start = 3, .end = 9});
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_PROGRAM, n.kind);
  TEST_ASSERT_EQUAL_size_t(3, n.span.start);
  TEST_ASSERT_EQUAL_size_t(9, n.span.end);
}
static const TestDispatchEntry ENTRIES[] = {
    {"header_carries_kind_and_span", test_header_carries_kind_and_span},
};

TEST_DISPATCH_MAIN(ENTRIES)
