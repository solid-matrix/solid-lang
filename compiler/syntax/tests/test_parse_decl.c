#define TEST_SUPPORT_NO_DEFAULT_FIXTURES

#include <string.h>

#include "node.h"
#include "parse.h"
#include "parser_fixture.h"
#include "syntax_node.h"
#include "test_support.h"

void setUp(void) {}
void tearDown(void) { fx_release(); }

typedef SyntaxNodeResult (*DeclFn)(const SyntaxParser *, Span);

static const SyntaxNodeList *decl_path(const SyntaxNodeResult *r, SyntaxKind kind) {
  return kind == SYNTAX_KIND_NAMESPACE_DECL ? ((const SyntaxNamespaceDecl *)r->node)->path
                                            : ((const SyntaxUsingDecl *)r->node)->path;
}

// Asserts a fully formed declaration consuming the whole text.
static void expect_decl_ok(DeclFn fn, SyntaxKind kind, const char *text, const char *const *segs, size_t count) {
  fx_begin(text);
  SyntaxNodeResult r = fn(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NOT_NULL(r.node);
  TEST_ASSERT_EQUAL_HEX32(kind, r.node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen(text), r.rem.start);
  check_path(decl_path(&r, kind), segs, count);
}

// Asserts a recovered declaration: exact diagnostics in order
// (codes[0] = head = newest), exact rem, path per segments-or-empty.
static void expect_decl_bad(DeclFn fn, SyntaxKind kind, const char *text, const char *const *segs, size_t count,
                            const SyntaxErrorCode *codes, size_t code_count, size_t rem_at) {
  fx_begin(text);
  SyntaxNodeResult r = fn(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched); // recovered
  TEST_ASSERT_NOT_NULL(r.node);
  TEST_ASSERT_EQUAL_HEX32(kind, r.node->kind);
  TEST_ASSERT_EQUAL_size_t(rem_at, r.rem.start);

  TEST_ASSERT_EQUAL_size_t(code_count, error_count(&r));
  const SyntaxErrorList *e = r.errors;
  for (size_t i = 0; i < code_count; i++) {
    TEST_ASSERT_NOT_NULL(e);
    if (!e)
      return;
    TEST_ASSERT_EQUAL_HEX32(codes[i], e->error.code);
    e = e->next;
  }
  TEST_ASSERT_NULL(e);

  check_path(decl_path(&r, kind), segs, count);
}

/* ---- namespace / using declarations ---------------------------------- */

void test_namespace_decl_valid_forms(void) {
  static const char *const ONE[] = {"std"};
  static const char *const TWO[] = {"std", "io"};
  static const char *const THREE[] = {"a", "b", "c"};

  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace std;", ONE, 1);
  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace std::io;", TWO, 2);
  // Trivia at the junctions is part of the contract.
  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace std :: io ;", TWO, 2);
  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace a::b::c;", THREE, 3);
  // Trivia before the semicolon: the check reads the post-trivia byte.
  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace std ;", ONE, 1);
}

void test_using_decl_valid_forms(void) {
  static const char *const ONE[] = {"std"};
  static const char *const TWO[] = {"std", "io"};

  expect_decl_ok(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std;", ONE, 1);
  expect_decl_ok(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std::io;", TWO, 2);
  expect_decl_ok(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std :: io ;", TWO, 2);
  expect_decl_ok(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std ;", ONE, 1);
}

void test_decl_keyword_boundary(void) {
  // "namespacex" is one identifier, not the keyword + "x".
  fx_begin("namespacex");
  SyntaxNodeResult r = parse_namespace_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_FALSE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("usingx");
  r = parse_using_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_FALSE(r.matched);
}

void test_namespace_decl_malforms(void) {
  static const SyntaxErrorCode SEMI[] = {SYNTAX_EXPECTED_SEMICOLON};
  static const SyntaxErrorCode IDENT[] = {SYNTAX_EXPECTED_IDENTIFIER};
  static const SyntaxErrorCode SEMI_THEN_IDENT[] = {SYNTAX_EXPECTED_SEMICOLON, SYNTAX_EXPECTED_IDENTIFIER};
  static const char *const STD[] = {"std"};
  static const char *const A[] = {"a"};

  // Empty path: the missing identifier is reported at the post-keyword
  // anchor; a following well-formed ";" still closes the declaration.
  expect_decl_bad(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace ;", NULL, 0, IDENT, 1,
                  strlen("namespace ;"));
  // Empty path at EOF: semicolon diagnostic (newest) on top of the
  // missing identifier.
  expect_decl_bad(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace", NULL, 0, SEMI_THEN_IDENT, 2,
                  strlen("namespace"));
  expect_decl_bad(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace std io", STD, 1, SEMI, 1,
                  strlen("namespace std"));
  expect_decl_bad(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace a::", A, 1, SEMI_THEN_IDENT, 2,
                  strlen("namespace a::"));
}

void test_using_decl_malforms(void) {
  static const SyntaxErrorCode SEMI[] = {SYNTAX_EXPECTED_SEMICOLON};
  static const SyntaxErrorCode IDENT[] = {SYNTAX_EXPECTED_IDENTIFIER};
  static const SyntaxErrorCode SEMI_THEN_IDENT[] = {SYNTAX_EXPECTED_SEMICOLON, SYNTAX_EXPECTED_IDENTIFIER};
  static const char *const STD[] = {"std"};
  static const char *const A[] = {"a"};

  expect_decl_bad(parse_using_decl, SYNTAX_KIND_USING_DECL, "using ;", NULL, 0, IDENT, 1, strlen("using ;"));
  expect_decl_bad(parse_using_decl, SYNTAX_KIND_USING_DECL, "using", NULL, 0, SEMI_THEN_IDENT, 2, strlen("using"));
  expect_decl_bad(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std io", STD, 1, SEMI, 1, strlen("using std"));
  // Dangling "::" then a well-formed ";": closes cleanly with only the
  // identifier diagnostic.
  expect_decl_bad(parse_using_decl, SYNTAX_KIND_USING_DECL, "using a::;", A, 1, IDENT, 1, strlen("using a::;"));
}

/* ---- let declaration --------------------------------------------------- */

void test_let_decl_three_forms(void) {
  // Value only, through the dispatch: parse_decl picks the let branch.
  fx_begin("let PI = 3.1415926;");
  SyntaxNodeResult r = parse_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_LET_DECL, r.node->kind);
  const SyntaxLetDecl *d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "PI");
  TEST_ASSERT_NULL(d->type);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_FLOAT_LIT_EXPR, d->value->kind);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(d->annotations));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("let PI = 3.1415926;"), r.rem.start);

  // Both type and value.
  fx_begin("let PI: f64 = 3.1415926;");
  r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->type->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_FLOAT_LIT_EXPR, d->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("let PI: f64 = 3.1415926;"), r.rem.start);

  // Type only.
  fx_begin("let TMP: i32;");
  r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->type->kind);
  TEST_ASSERT_NULL(d->value);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("let TMP: i32;"), r.rem.start);

  // Annotations ride along (doc example).
  fx_begin("@import(\"COUNT\") let COUNT: usize;");
  r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->annotations));
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "COUNT");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->type->kind);
  TEST_ASSERT_NULL(d->value);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("@import(\"COUNT\") let COUNT: usize;"), r.rem.start);
}

void test_let_decl_malforms(void) {
  // Neither type nor value: one diagnostic for the likelier intent.
  fx_begin("let x;");
  SyntaxNodeResult r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxLetDecl *d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "x");
  TEST_ASSERT_NULL(d->type);
  TEST_ASSERT_NULL(d->value);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EQUALS, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("let x;"), r.rem.start);

  // Missing ";".
  fx_begin("let x = 1");
  r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NOT_NULL(((const SyntaxLetDecl *)r.node)->value);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_SEMICOLON, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("let x = 1"), r.rem.start);

  // Missing identifier still binds the value.
  fx_begin("let = 1;");
  r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_NULL(d->id);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, d->value->kind);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, r.errors->error.code);

  // Dangling ":" reports the missing type and still closes.
  fx_begin("let x : ;");
  r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_NULL(d->type);
  TEST_ASSERT_NULL(d->value);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("let x : ;"), r.rem.start);
}

/* ---- struct / union declarations --------------------------------------- */

void test_struct_decl_bare_and_empty_body(void) {
  // Bare form, through the dispatch.
  fx_begin("struct Foo;");
  SyntaxNodeResult r = parse_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_STRUCT_DECL, r.node->kind);
  const SyntaxStructDecl *d = (const SyntaxStructDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "Foo");
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(d->generic_params));
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(d->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("struct Foo;"), r.rem.start);

  // Empty braced body.
  fx_begin("struct Foo {}");
  r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(((const SyntaxStructDecl *)r.node)->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("struct Foo {}"), r.rem.start);
}

void test_struct_decl_fields_and_trailing_comma(void) {
  fx_begin("struct Vector2F { x: f32, y: f32 }");
  SyntaxNodeResult r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  const SyntaxStructDecl *d = (const SyntaxStructDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->fields));
  // Source order: x leads.
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxStructField *)d->fields->node)->id->value, "x");
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxStructField *)d->fields->next->node)->id->value, "y");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ((const SyntaxStructField *)d->fields->node)->type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("struct Vector2F { x: f32, y: f32 }"), r.rem.start);

  // A trailing comma hands the brace back cleanly.
  fx_begin("struct V { x: f32, }");
  r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxStructDecl *)r.node)->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("struct V { x: f32, }"), r.rem.start);
}

void test_struct_decl_generics(void) {
  fx_begin("@pack(4) struct Vector2<T> { x: T, y: T }");
  SyntaxNodeResult r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxStructDecl *d = (const SyntaxStructDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->annotations));
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->generic_params));
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxGenericParam *)d->generic_params->node)->id->value, "T");
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->fields));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ((const SyntaxStructField *)d->fields->node)->type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("@pack(4) struct Vector2<T> { x: T, y: T }"), r.rem.start);
}

void test_struct_decl_generics_malforms(void) {
  // Empty "<>" reports the missing parameter and still closes.
  fx_begin("struct V<> { x: i32 }");
  SyntaxNodeResult r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(((const SyntaxStructDecl *)r.node)->generic_params));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, r.errors->error.code);

  // Missing ">": the clause frame survives and the body still parses.
  fx_begin("struct V<T { x: i32 }");
  r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxStructDecl *)r.node)->generic_params));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_GT, r.errors->error.code);
}

void test_struct_field_frames(void) {
  // Missing colon: the frame survives holding the type.
  fx_begin("struct V { x i32 }");
  SyntaxNodeResult r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  const SyntaxStructDecl *d = (const SyntaxStructDecl *)r.node;
  const SyntaxStructField *f = (const SyntaxStructField *)d->fields->node;
  TEST_ASSERT_STRVIEW_EQ(f->id->value, "x");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, f->type->kind);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_COLON, r.errors->error.code);

  // Missing type: the frame survives with a NULL type.
  fx_begin("struct V { x: }");
  r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  d = (const SyntaxStructDecl *)r.node;
  f = (const SyntaxStructField *)d->fields->node;
  TEST_ASSERT_STRVIEW_EQ(f->id->value, "x");
  TEST_ASSERT_NULL(f->type);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
}

void test_struct_decl_body_malforms(void) {
  // Neither ";" nor "{": one DECL_BODY diagnostic, rem at the header end.
  fx_begin("struct Foo");
  SyntaxNodeResult r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_DECL_BODY, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("struct Foo"), r.rem.start);

  // Missing "}": the fields frame survives.
  fx_begin("struct V { x: f32");
  r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxStructDecl *)r.node)->fields));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RBRACE, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("struct V { x: f32"), r.rem.start);
}

void test_union_decl_forms(void) {
  fx_begin("union U;");
  SyntaxNodeResult r = parse_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_UNION_DECL, r.node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("union U;"), r.rem.start);

  fx_begin("union FooUnion<T> { value: T, ptr: &T }");
  r = parse_union_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxUnionDecl *d = (const SyntaxUnionDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->generic_params));
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->fields));
  // The reference-typed field keeps REF_TYPE with a NAMED inner type.
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxUnionField *)d->fields->node)->id->value, "value"); // source order
  const SyntaxUnionField *ptr = (const SyntaxUnionField *)d->fields->next->node;
  TEST_ASSERT_STRVIEW_EQ(ptr->id->value, "ptr");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_REF_TYPE, ptr->type->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ((const SyntaxRefType *)ptr->type)->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("union FooUnion<T> { value: T, ptr: &T }"), r.rem.start);
}

/* ---- enum / variant declarations --------------------------------------- */

void test_enum_decl_forms(void) {
  fx_begin("enum Color;");
  SyntaxNodeResult r = parse_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_ENUM_DECL, r.node->kind);
  const SyntaxEnumDecl *d = (const SyntaxEnumDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "Color");
  TEST_ASSERT_NULL(d->behind_type);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(d->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("enum Color;"), r.rem.start);

  fx_begin("enum Color { Red, Green, Blue }");
  r = parse_enum_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxEnumDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(3, syntax_nodelist_length(d->fields));
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxEnumField *)d->fields->node)->id->value, "Red"); // source order
  TEST_ASSERT_NULL(((const SyntaxEnumField *)d->fields->node)->value);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("enum Color { Red, Green, Blue }"), r.rem.start);

  fx_begin("enum SomeFlag: u32 { A = 0x0001_u32, B = 0x0002_u32 }");
  r = parse_enum_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxEnumDecl *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->behind_type->kind);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->fields));
  const SyntaxEnumField *f = (const SyntaxEnumField *)d->fields->node;
  TEST_ASSERT_STRVIEW_EQ(f->id->value, "A");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, f->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("enum SomeFlag: u32 { A = 0x0001_u32, B = 0x0002_u32 }"), r.rem.start);
}

void test_enum_decl_trailing_comma(void) {
  fx_begin("enum Color { Red, }");
  SyntaxNodeResult r = parse_enum_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxEnumDecl *)r.node)->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("enum Color { Red, }"), r.rem.start);
}

void test_enum_decl_field_malform(void) {
  fx_begin("enum C { A = }");
  SyntaxNodeResult r = parse_enum_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxEnumDecl *d = (const SyntaxEnumDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->fields));
  TEST_ASSERT_NULL(((const SyntaxEnumField *)d->fields->node)->value);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("enum C { A = }"), r.rem.start);
}

void test_variant_decl_forms(void) {
  fx_begin("variant Option;");
  SyntaxNodeResult r = parse_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_VARIANT_DECL, r.node->kind);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("variant Option<T> { None, Value: T }");
  r = parse_variant_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxVariantDecl *d = (const SyntaxVariantDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "Option");
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->generic_params));
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->fields));
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxVariantField *)d->fields->node)->id->value, "None"); // source order
  TEST_ASSERT_NULL(((const SyntaxVariantField *)d->fields->node)->type);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ((const SyntaxVariantField *)d->fields->next->node)->type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("variant Option<T> { None, Value: T }"), r.rem.start);

  fx_begin("variant State: u8 { Idle, Run, }");
  r = parse_variant_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxVariantDecl *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->behind_type->kind);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("variant State: u8 { Idle, Run, }"), r.rem.start);
}

/* ---- contract declaration ---------------------------------------------- */

void test_contract_decl_forms(void) {
  fx_begin("contract Addable<TLeft, TRight, TResult>(left: TLeft, right: TRight): TResult;");
  SyntaxNodeResult r = parse_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CONTRACT_DECL, r.node->kind);
  const SyntaxContractDecl *d = (const SyntaxContractDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "Addable");
  TEST_ASSERT_EQUAL_size_t(3, syntax_nodelist_length(d->generic_params));
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->call_params));
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxCallParam *)d->call_params->node)->id->value, "left"); // source order
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ((const SyntaxCallParam *)d->call_params->node)->type->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->return_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("contract Addable<TLeft, TRight, TResult>(left: TLeft, right: TRight): TResult;"),
                           r.rem.start);

  fx_begin("contract Foo();");
  r = parse_contract_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxContractDecl *)r.node;
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(d->call_params));
  TEST_ASSERT_NULL(d->return_type);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("contract Foo();"), r.rem.start);
}

void test_contract_decl_malforms(void) {
  fx_begin("contract Foo");
  SyntaxNodeResult r = parse_contract_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(2, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_SEMICOLON, r.errors->error.code);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_LPAREN, r.errors->next->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("contract Foo"), r.rem.start);

  fx_begin("contract Foo(x: T");
  r = parse_contract_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxContractDecl *)r.node)->call_params));
  TEST_ASSERT_EQUAL_size_t(2, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_SEMICOLON, r.errors->error.code);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RPAREN, r.errors->next->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("contract Foo(x: T"), r.rem.start);

  fx_begin("contract Foo()");
  r = parse_contract_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_SEMICOLON, r.errors->error.code);

  fx_begin("contract Foo(): ;");
  r = parse_contract_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(((const SyntaxContractDecl *)r.node)->return_type);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("contract Foo(): ;"), r.rem.start);
}

/* ---- func declaration --------------------------------------------------- */

void test_func_decl_full_ladder(void) {
  fx_begin("@private func add<T: i32, U>(x: T, y: U)cdecl: i32 fulfills Addable { return x; }");
  SyntaxNodeResult r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxFuncDecl *d = (const SyntaxFuncDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "add");
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->annotations));
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->generic_params));
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxGenericParam *)d->generic_params->node)->id->value, "T"); // source order
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->call_params));
  TEST_ASSERT_STRVIEW_EQ(d->callconv->value, "cdecl");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->return_type->kind);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->fulfills));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, d->body->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("@private func add<T: i32, U>(x: T, y: U)cdecl: i32 fulfills Addable { return x; }"),
                           r.rem.start);
}

void test_func_decl_body_forms(void) {
  fx_begin("func f();");
  SyntaxNodeResult r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EMPTY_STMT, ((const SyntaxFuncDecl *)r.node)->body->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("func f();"), r.rem.start);

  fx_begin("func f() {}");
  r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, ((const SyntaxFuncDecl *)r.node)->body->kind);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("func main():i32{ return 0; }");
  r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxFuncDecl *d = (const SyntaxFuncDecl *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->return_type->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, d->body->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_RETURN_STMT, ((const SyntaxBodyStmt *)d->body)->stmts->node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("func main():i32{ return 0; }"), r.rem.start);
}

void test_func_decl_callconv_and_fulfills(void) {
  fx_begin("func f()cdecl:i32{}");
  SyntaxNodeResult r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxFuncDecl *)r.node)->callconv->value, "cdecl");
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(((const SyntaxFuncDecl *)r.node)->fulfills));

  // "fulfills" starts its clause; it is not lexed as a calling convention.
  fx_begin("func f() fulfills Addable;");
  r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxFuncDecl *d = (const SyntaxFuncDecl *)r.node;
  TEST_ASSERT_NULL(d->callconv);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->fulfills));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EMPTY_STMT, d->body->kind);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("func f(): i32{}");
  r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxFuncDecl *)r.node;
  TEST_ASSERT_NULL(d->callconv);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(d->fulfills));
  TEST_ASSERT_NOT_NULL(d->return_type);
}

void test_generic_fulfills(void) {
  fx_begin("func f() fulfills Addable<i32, i32>;");
  SyntaxNodeResult r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxFuncDecl *d = (const SyntaxFuncDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->fulfills));
  const SyntaxNamed *n = as_named(d->fulfills->node);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(n->generic_args));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EMPTY_STMT, d->body->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("func f() fulfills Addable<i32, i32>;"), r.rem.start);
}

static const TestDispatchEntry ENTRIES[] = {
    {"namespace_decl_valid_forms", test_namespace_decl_valid_forms},
    {"using_decl_valid_forms", test_using_decl_valid_forms},
    {"decl_keyword_boundary", test_decl_keyword_boundary},
    {"namespace_decl_malforms", test_namespace_decl_malforms},
    {"using_decl_malforms", test_using_decl_malforms},
    {"let_decl_three_forms", test_let_decl_three_forms},
    {"let_decl_malforms", test_let_decl_malforms},
    {"struct_decl_bare_and_empty_body", test_struct_decl_bare_and_empty_body},
    {"struct_decl_fields_and_trailing_comma", test_struct_decl_fields_and_trailing_comma},
    {"struct_decl_generics", test_struct_decl_generics},
    {"struct_decl_generics_malforms", test_struct_decl_generics_malforms},
    {"struct_field_frames", test_struct_field_frames},
    {"struct_decl_body_malforms", test_struct_decl_body_malforms},
    {"union_decl_forms", test_union_decl_forms},
    {"enum_decl_forms", test_enum_decl_forms},
    {"enum_decl_trailing_comma", test_enum_decl_trailing_comma},
    {"enum_decl_field_malform", test_enum_decl_field_malform},
    {"variant_decl_forms", test_variant_decl_forms},
    {"contract_decl_forms", test_contract_decl_forms},
    {"contract_decl_malforms", test_contract_decl_malforms},
    {"func_decl_full_ladder", test_func_decl_full_ladder},
    {"func_decl_body_forms", test_func_decl_body_forms},
    {"func_decl_callconv_and_fulfills", test_func_decl_callconv_and_fulfills},
    {"generic_fulfills", test_generic_fulfills},
};

TEST_DISPATCH_MAIN(ENTRIES)
