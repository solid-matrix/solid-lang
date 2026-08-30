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

/* ---- parse_named_type -------------------------------------------------- */

void test_named_type_single_and_path(void) {
  fx_begin("i32");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  static const char *const I32[] = {"i32"};
  check_path(t->path, I32, 1);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(3, r.rem.start);

  fx_begin("std::math::Vector2");
  r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  t = as_named(r.node);
  static const char *const PATH[] = {"std", "math", "Vector2"};
  check_path(t->path, PATH, 3);
  TEST_ASSERT_EQUAL_size_t(strlen("std::math::Vector2"), r.rem.start);
}

void test_named_type_trivia_between_segments(void) {
  fx_begin("a :: b");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  static const char *const AB[] = {"a", "b"};
  check_path(t->path, AB, 2);
  TEST_ASSERT_NULL(r.errors);
}

void test_named_type_not_match_on_non_identifier(void) {
  fx_begin("&i32");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_FALSE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(0, r.rem.start);
}

/* ---- parse_ref_type ------------------------------------------------------ */

void test_ref_type_readwrite_named_inner(void) {
  fx_begin("&i32");
  SyntaxNodeResult r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_REF_TYPE, r.node->kind);
  const SyntaxRefType *ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_REF_KIND_READWRITE, ref->ref_kind);
  const SyntaxNamed *inner = as_named(ref->inner_type);
  static const char *const I32[] = {"i32"};
  check_path(inner->path, I32, 1);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(4, r.rem.start);
  TEST_ASSERT_EQUAL_size_t(0, ref->header.span.start);
  TEST_ASSERT_EQUAL_size_t(4, ref->header.span.end);
}

void test_ref_type_readonly_and_writeonly(void) {
  fx_begin("&readonly i32");
  SyntaxNodeResult r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxRefType *ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_REF_KIND_READONLY, ref->ref_kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ref->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("&readonly i32"), r.rem.start);

  fx_begin("&writeonly i32");
  r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_REF_KIND_WRITEONLY, ref->ref_kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ref->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
}

void test_ref_type_keyword_word_boundary(void) {
  // "readonlyi32" is an identifier, not the keyword: the reference stays
  // READWRITE and the whole word becomes the inner type's path.
  fx_begin("&readonlyi32");
  SyntaxNodeResult r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxRefType *ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_REF_KIND_READWRITE, ref->ref_kind);
  const SyntaxNamed *inner = as_named(ref->inner_type);
  static const char *const NAME[] = {"readonlyi32"};
  check_path(inner->path, NAME, 1);
  TEST_ASSERT_NULL(r.errors);
}

void test_ref_type_nested_and_trivia(void) {
  fx_begin("&&i32");
  SyntaxNodeResult r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxRefType *ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_REF_TYPE, ref->inner_type->kind);
  const SyntaxRefType *inner_ref = (const SyntaxRefType *)ref->inner_type;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_REF_KIND_READWRITE, inner_ref->ref_kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, inner_ref->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("&  readonly  \n i32");
  r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_REF_KIND_READONLY, ref->ref_kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ref->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
}

void test_ref_type_missing_inner_reports_type(void) {
  fx_begin("&");
  SyntaxNodeResult r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NOT_NULL(r.errors);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
  TEST_ASSERT_NULL(r.errors->next);
  TEST_ASSERT_NULL(((const SyntaxRefType *)r.node)->inner_type);
  TEST_ASSERT_EQUAL_size_t(1, r.rem.start);

  fx_begin("& +");
  r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(1, r.rem.start);
}

void test_ref_type_not_match_without_amp(void) {
  fx_begin("i32");
  SyntaxNodeResult r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_FALSE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(0, r.rem.start);
}

void test_type_dispatch_prefers_ref_for_amp(void) {
  fx_begin("&i32");
  SyntaxNodeResult r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_REF_TYPE, r.node->kind);
  TEST_ASSERT_NULL(r.errors);
}

/* ---- array / func types -------------------------------------------------- */

void test_array_type_forms(void) {
  fx_begin("[5]i32");
  SyntaxNodeResult r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_ARRAY_TYPE, r.node->kind);
  const SyntaxArrayType *t = (const SyntaxArrayType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, t->len->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, t->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("[5]i32"), r.rem.start);

  fx_begin("[5+10]i32");
  r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  t = (const SyntaxArrayType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BINARY_EXPR, t->len->kind);

  fx_begin("[2][2]i32");
  r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  t = (const SyntaxArrayType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_ARRAY_TYPE, t->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("[2][2]i32"), r.rem.start);
}

void test_array_type_malform(void) {
  fx_begin("[]i32");
  SyntaxNodeResult r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxArrayType *t = (const SyntaxArrayType *)r.node;
  TEST_ASSERT_NULL(t->len);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, t->inner_type->kind);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("[]i32"), r.rem.start);

  fx_begin("[5 i32");
  r = parse_array_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RBRACKET, r.errors->error.code);

  fx_begin("[5]");
  r = parse_array_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(((const SyntaxArrayType *)r.node)->inner_type);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
}

void test_func_type_forms(void) {
  fx_begin("&func()");
  SyntaxNodeResult r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_FUNC_TYPE, r.node->kind);
  const SyntaxFuncType *t = (const SyntaxFuncType *)r.node;
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(t->call_params));
  TEST_ASSERT_NULL(t->callconv);
  TEST_ASSERT_NULL(t->return_type);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("&func(i32, i32):i32");
  r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  t = (const SyntaxFuncType *)r.node;
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(t->call_params));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, t->return_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("&func(i32, i32):i32"), r.rem.start);

  fx_begin("&func(i32)cdecl:i32");
  r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  t = (const SyntaxFuncType *)r.node;
  TEST_ASSERT_STRVIEW_EQ(t->callconv->value, "cdecl");
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(t->call_params));
  TEST_ASSERT_NULL(r.errors);
}

void test_func_type_word_boundary(void) {
  fx_begin("&functional");
  SyntaxNodeResult r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_REF_TYPE, r.node->kind);
  const SyntaxRefType *ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ref->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("&functional"), r.rem.start);
}

void test_func_type_malform(void) {
  fx_begin("&func(i32");
  SyntaxNodeResult r = parse_func_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxFuncType *)r.node)->call_params));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RPAREN, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("&func(i32"), r.rem.start);
}

/* ---- generic arguments ------------------------------------------------- */

void test_named_type_generic_forms(void) {
  fx_begin("Array<i32, N = 5>");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  static const char *const PATH[] = {"Array"};
  check_path(t->path, PATH, 1);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(t->generic_args));
  const SyntaxGenericArg *ty = (const SyntaxGenericArg *)t->generic_args->node; // source order
  TEST_ASSERT_NULL(ty->id);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ty->value->kind);
  const SyntaxGenericArg *named = (const SyntaxGenericArg *)t->generic_args->next->node;
  TEST_ASSERT_STRVIEW_EQ(named->id->value, "N");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, named->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Array<i32, N = 5>"), r.rem.start);

  fx_begin("Box<[5]i32>");
  r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  t = as_named(r.node);
  const SyntaxGenericArg *arg = (const SyntaxGenericArg *)t->generic_args->node;
  TEST_ASSERT_NULL(arg->id);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_ARRAY_TYPE, arg->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Box<[5]i32>"), r.rem.start);
}

void test_named_type_generic_paren_escape(void) {
  fx_begin("Array<i32, N = (LEN + 1)>");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  const SyntaxGenericArg *named = (const SyntaxGenericArg *)t->generic_args->next->node;
  TEST_ASSERT_STRVIEW_EQ(named->id->value, "N");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BINARY_EXPR, named->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Array<i32, N = (LEN + 1)>"), r.rem.start);
}

void test_named_type_generic_nested(void) {
  fx_begin("Box<Box<i32>>");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *outer = as_named(r.node);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(outer->generic_args));
  const SyntaxGenericArg *arg = (const SyntaxGenericArg *)outer->generic_args->node;
  TEST_ASSERT_NULL(arg->id);
  const SyntaxNamed *inner = as_named(arg->value);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(inner->generic_args));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Box<Box<i32>>"), r.rem.start);
}

void test_named_type_generic_malform(void) {
  fx_begin("a<>");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(as_named(r.node)->generic_args));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("a<>"), r.rem.start);

  fx_begin("a<b");
  r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(as_named(r.node)->generic_args));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_GT, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("a<b"), r.rem.start);
}

void test_generic_arg_compile_time_value(void) {
  fx_begin("M<i32, N = @sizeof(i32)>");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  const SyntaxGenericArg *named = (const SyntaxGenericArg *)t->generic_args->next->node;
  TEST_ASSERT_STRVIEW_EQ(named->id->value, "N");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_COMPILE_TIME, named->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("M<i32, N = @sizeof(i32)>"), r.rem.start);
}

void test_generic_arg_named_generic_value(void) {
  fx_begin("add<i32, i32, F = Addable<i32, i32>>");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  TEST_ASSERT_EQUAL_size_t(3, syntax_nodelist_length(t->generic_args));
  const SyntaxGenericArg *f = (const SyntaxGenericArg *)t->generic_args->next->next->node; // source order
  TEST_ASSERT_STRVIEW_EQ(f->id->value, "F");
  const SyntaxNamed *v = as_named(f->value);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(v->generic_args));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("add<i32, i32, F = Addable<i32, i32>>"), r.rem.start);
}

static const TestDispatchEntry ENTRIES[] = {
    {"named_type_single_and_path", test_named_type_single_and_path},
    {"named_type_trivia_between_segments", test_named_type_trivia_between_segments},
    {"named_type_not_match_on_non_identifier", test_named_type_not_match_on_non_identifier},
    {"ref_type_readwrite_named_inner", test_ref_type_readwrite_named_inner},
    {"ref_type_readonly_and_writeonly", test_ref_type_readonly_and_writeonly},
    {"ref_type_keyword_word_boundary", test_ref_type_keyword_word_boundary},
    {"ref_type_nested_and_trivia", test_ref_type_nested_and_trivia},
    {"ref_type_missing_inner_reports_type", test_ref_type_missing_inner_reports_type},
    {"ref_type_not_match_without_amp", test_ref_type_not_match_without_amp},
    {"type_dispatch_prefers_ref_for_amp", test_type_dispatch_prefers_ref_for_amp},
    {"array_type_forms", test_array_type_forms},
    {"array_type_malform", test_array_type_malform},
    {"func_type_forms", test_func_type_forms},
    {"func_type_word_boundary", test_func_type_word_boundary},
    {"func_type_malform", test_func_type_malform},
    {"named_type_generic_forms", test_named_type_generic_forms},
    {"named_type_generic_paren_escape", test_named_type_generic_paren_escape},
    {"named_type_generic_nested", test_named_type_generic_nested},
    {"named_type_generic_malform", test_named_type_generic_malform},
    {"generic_arg_compile_time_value", test_generic_arg_compile_time_value},
    {"generic_arg_named_generic_value", test_generic_arg_named_generic_value},
};

TEST_DISPATCH_MAIN(ENTRIES)
