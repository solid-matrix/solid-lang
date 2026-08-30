#define TEST_SUPPORT_NO_DEFAULT_FIXTURES

#include <string.h>

#include "node.h"
#include "parse.h"
#include "parser_fixture.h"
#include "syntax_error.h"
#include "syntax_node.h"
#include "test_support.h"

void setUp(void) {}
void tearDown(void) { fx_release(); }

static const SyntaxCompileTime *as_ct(const SyntaxNodeResult *r, const char *name) {
  TEST_ASSERT_TRUE(r->matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_COMPILE_TIME, r->node->kind);
  const SyntaxCompileTime *ct = (const SyntaxCompileTime *)r->node;
  TEST_ASSERT_STRVIEW_EQ(ct->id->value, name);
  return ct;
}

static const SyntaxCompileTime *as_ct_node(const SyntaxNode *n) {
  TEST_ASSERT_NOT_NULL(n);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_COMPILE_TIME, n->kind);
  return (const SyntaxCompileTime *)n;
}

static size_t top_level_count(const SyntaxProgram *p) {
  size_t n = 0;
  for (const SyntaxNodeList *i = p->top_levels; i != NULL; i = i->next)
    n++;
  return n;
}

/* ---- parse_identifier ---------------------------------------------- */

void test_identifier_basic(void) {
  fx_begin("abc");
  SyntaxNodeResult r = parse_identifier(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_IDENTIFIER, r.node->kind);
  TEST_ASSERT_STRVIEW_EQ(((SyntaxIdentifier *)r.node)->value, "abc");
  TEST_ASSERT_EQUAL_size_t(3, r.rem.start);
}

void test_identifier_stops_at_non_word(void) {
  fx_begin("ab.c");
  SyntaxNodeResult r = parse_identifier(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_STRVIEW_EQ(((SyntaxIdentifier *)r.node)->value, "ab");
  TEST_ASSERT_EQUAL_size_t(2, r.rem.start);
}

void test_identifier_rejects_digit_start(void) {
  fx_begin("1abc");
  SyntaxNodeResult r = parse_identifier(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_FALSE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_NULL(r.errors);
}

/* ---- compile-time forms ---------------------------------------------- */

void test_ct_bare(void) {
  fx_begin("@private");
  SyntaxNodeResult r = parse_compile_time(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("@private"), r.rem.start);

  const SyntaxCompileTime *ct = as_ct(&r, "private");
  if (ct)
    TEST_ASSERT_EQUAL_size_t(0, syntax_nodelist_length(ct->args));
}

void test_ct_with_args(void) {
  fx_begin("@align(16)");
  SyntaxNodeResult r = parse_compile_time(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("@align(16)"), r.rem.start);

  const SyntaxCompileTime *ct = as_ct(&r, "align");
  if (!ct)
    return;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(ct->args));
  as_int(ct->args->node, "16");
}

void test_ct_multi_string_args_in_source_order(void) {
  fx_begin("@import(\"LLVM-C\",\"LLVMContextCreate\")");
  SyntaxNodeResult r = parse_compile_time(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_NULL(r.errors);

  const SyntaxCompileTime *ct = as_ct(&r, "import");
  if (!ct)
    return;
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(ct->args));
  // Source order: the first literal leads the chain.
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_STRING_LIT_EXPR, ct->args->node->kind);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxStringLitExpr *)ct->args->node)->value, "LLVM-C");
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxStringLitExpr *)ct->args->next->node)->value, "LLVMContextCreate");
}

void test_ct_missing_name_frame(void) {
  // The frame survives with a NULL name; one diagnostic.
  fx_begin("@");
  SyntaxNodeResult r = parse_compile_time(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_COMPILE_TIME, r.node->kind);

  const SyntaxCompileTime *ct = (const SyntaxCompileTime *)r.node;
  TEST_ASSERT_NULL(ct->id);
  TEST_ASSERT_EQUAL_size_t(0, syntax_nodelist_length(ct->args));
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, r.errors->error.code);
}

void test_ct_unclosed_args_frame(void) {
  // Parsed arguments stay in the frame; one RPAREN diagnostic.
  fx_begin("@align(16");
  SyntaxNodeResult r = parse_compile_time(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);

  const SyntaxCompileTime *ct = (const SyntaxCompileTime *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(ct->args));
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RPAREN, r.errors->error.code);
}

/* ---- parse_annotations ------------------------------------------------ */

void test_annotations_none(void) {
  fx_begin("foo");
  SyntaxListResult l = parse_annotations(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_NULL(l.list); // zero annotations: empty chain
  TEST_ASSERT_NULL(l.errors);
  TEST_ASSERT_EQUAL_size_t(0, l.rem.start);
}

void test_annotations_single_and_multi(void) {
  fx_begin("@a @b(1) foo");
  SyntaxListResult l = parse_annotations(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(l.list));
  TEST_ASSERT_STRVIEW_EQ(as_ct_node(l.list->node)->id->value, "a");
  TEST_ASSERT_STRVIEW_EQ(as_ct_node(l.list->next->node)->id->value, "b");
  TEST_ASSERT_NULL(l.errors);
  // Trivia before the next non-annotation stays with the enclosing sequence.
  TEST_ASSERT_EQUAL_size_t(strlen("@a @b(1)"), l.rem.start);
}

void test_annotations_error_frame(void) {
  // A bare "@" keeps its frame (node with no name) and one diagnostic.
  fx_begin("@");
  SyntaxListResult l = parse_annotations(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(l.list));
  TEST_ASSERT_NULL(as_ct_node(l.list->node)->id);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(l.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, l.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(1, l.rem.start);
}

/* ---- parse_generic_param / parse_call_param --------------------------- */

void test_generic_param_bare_and_bound(void) {
  fx_begin("T");
  SyntaxNodeResult r = parse_generic_param(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxGenericParam *p = (const SyntaxGenericParam *)r.node;
  TEST_ASSERT_STRVIEW_EQ(p->id->value, "T");
  TEST_ASSERT_NULL(p->type);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(p->annotations));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(1, r.rem.start);

  fx_begin("T : i32");
  r = parse_generic_param(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  p = (const SyntaxGenericParam *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, p->type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("T : i32"), r.rem.start);
}

void test_generic_param_annotated(void) {
  fx_begin("@align(8) T: i32");
  SyntaxNodeResult r = parse_generic_param(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxGenericParam *p = (const SyntaxGenericParam *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(p->annotations));
  TEST_ASSERT_STRVIEW_EQ(p->id->value, "T");
  TEST_ASSERT_NOT_NULL(p->type);
  TEST_ASSERT_NULL(r.errors);
}

void test_call_param_requires_colon_and_type(void) {
  fx_begin("x: i32");
  SyntaxNodeResult r = parse_call_param(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxCallParam *p = (const SyntaxCallParam *)r.node;
  TEST_ASSERT_STRVIEW_EQ(p->id->value, "x");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, p->type->kind);
  TEST_ASSERT_NULL(r.errors);

  // No colon and no type: the frame survives with two diagnostics.
  fx_begin("x");
  r = parse_call_param(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  p = (const SyntaxCallParam *)r.node;
  TEST_ASSERT_STRVIEW_EQ(p->id->value, "x");
  TEST_ASSERT_NULL(p->type);
  TEST_ASSERT_EQUAL_size_t(2, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_COLON, r.errors->next->error.code);
}

void test_call_param_annotated(void) {
  fx_begin("@intrinsic x : i32");
  SyntaxNodeResult r = parse_call_param(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxCallParam *p = (const SyntaxCallParam *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(p->annotations));
  TEST_ASSERT_STRVIEW_EQ(p->id->value, "x");
  TEST_ASSERT_NOT_NULL(p->type);
  TEST_ASSERT_NULL(r.errors);
}

/* ---- parse_program ---------------------------------------------------- */

void test_program_accumulates_decls_in_source_order(void) {
  fx_begin("namespace a;\nusing b;\n");
  SyntaxNodeResult r = parse_program(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_PROGRAM, r.node->kind);

  const SyntaxProgram *p = (const SyntaxProgram *)r.node;
  TEST_ASSERT_EQUAL_size_t(2, top_level_count(p));

  // Source order: the namespace decl was parsed first.
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMESPACE_DECL, p->top_levels->node->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_USING_DECL, p->top_levels->next->node->kind);
}

void test_program_junk_tail_reports_expected_eof(void) {
  fx_begin("namespace a;\n@@");
  SyntaxNodeResult r = parse_program(fx_parser, source_get_span(fx_source));

  const SyntaxProgram *p = (const SyntaxProgram *)r.node;
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, top_level_count(p));
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EOF, r.errors->error.code);
}

void test_program_empty_and_trivia_only(void) {
  fx_begin("");
  SyntaxNodeResult r = parse_program(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(0, top_level_count((const SyntaxProgram *)r.node));
  TEST_ASSERT_NULL(r.errors);

  fx_begin("  \n\t// comment\n");
  r = parse_program(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(0, top_level_count((const SyntaxProgram *)r.node));
  TEST_ASSERT_NULL(r.errors);
}

void test_program_func_sample_end_to_end(void) {
  static const char *const SAMPLE = "namespace std::math;\n"
                                    "using std::core;\n"
                                    "func main():i32{\n"
                                    "  return 0;\n"
                                    "}\n";
  fx_begin(SAMPLE);
  SyntaxNodeResult r = parse_program(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_PROGRAM, r.node->kind);
  TEST_ASSERT_EQUAL_size_t(strlen(SAMPLE), r.rem.start);

  const SyntaxProgram *p = (const SyntaxProgram *)r.node;
  TEST_ASSERT_EQUAL_size_t(3, syntax_nodelist_length(p->top_levels));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMESPACE_DECL, p->top_levels->node->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_USING_DECL, p->top_levels->next->node->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_FUNC_DECL, p->top_levels->next->next->node->kind);
}

static const TestDispatchEntry ENTRIES[] = {
    {"identifier_basic", test_identifier_basic},
    {"identifier_stops_at_non_word", test_identifier_stops_at_non_word},
    {"identifier_rejects_digit_start", test_identifier_rejects_digit_start},
    {"ct_bare", test_ct_bare},
    {"ct_with_args", test_ct_with_args},
    {"ct_multi_string_args_in_source_order", test_ct_multi_string_args_in_source_order},
    {"ct_missing_name_frame", test_ct_missing_name_frame},
    {"ct_unclosed_args_frame", test_ct_unclosed_args_frame},
    {"annotations_none", test_annotations_none},
    {"annotations_single_and_multi", test_annotations_single_and_multi},
    {"annotations_error_frame", test_annotations_error_frame},
    {"generic_param_bare_and_bound", test_generic_param_bare_and_bound},
    {"generic_param_annotated", test_generic_param_annotated},
    {"call_param_requires_colon_and_type", test_call_param_requires_colon_and_type},
    {"call_param_annotated", test_call_param_annotated},
    {"program_accumulates_decls_in_source_order", test_program_accumulates_decls_in_source_order},
    {"program_junk_tail_reports_expected_eof", test_program_junk_tail_reports_expected_eof},
    {"program_empty_and_trivia_only", test_program_empty_and_trivia_only},
    {"program_func_sample_end_to_end", test_program_func_sample_end_to_end},
};

TEST_DISPATCH_MAIN(ENTRIES)
