/**
 * @file test_literal.c
 * @brief Unit tests for literal-token classification and decoding.
 * @author solid-matrix
 */

#include "semantic_literal.h"
#include "test_support.h"

static void expect_int(Strview text, SemanticIntType type, uint64_t bits) {
  SemanticIntLiteral r = semantic_int_literal(text);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_LIT_OK, r.status);
  TEST_ASSERT_EQUAL_HEX32(type, r.type);
  TEST_ASSERT_EQUAL_UINT64(bits, r.bits);
}

static void expect_range(Strview text, SemanticIntType type) {
  SemanticIntLiteral r = semantic_int_literal(text);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_LIT_RANGE, r.status);
  TEST_ASSERT_EQUAL_HEX32(type, r.type);
}

void literal_int_type_matrix(void) {
  expect_int(STRVIEW("1i8"), SEMANTIC_INT_I8, 1);
  expect_int(STRVIEW("2i16"), SEMANTIC_INT_I16, 2);
  expect_int(STRVIEW("3i32"), SEMANTIC_INT_I32, 3);
  expect_int(STRVIEW("4i64"), SEMANTIC_INT_I64, 4);
  expect_int(STRVIEW("5isize"), SEMANTIC_INT_ISIZE, 5);
  expect_int(STRVIEW("6u8"), SEMANTIC_INT_U8, 6);
  expect_int(STRVIEW("7u16"), SEMANTIC_INT_U16, 7);
  expect_int(STRVIEW("8u32"), SEMANTIC_INT_U32, 8);
  expect_int(STRVIEW("9u64"), SEMANTIC_INT_U64, 9);
  expect_int(STRVIEW("10usize"), SEMANTIC_INT_USIZE, 10);
  expect_int(STRVIEW("7i"), SEMANTIC_INT_ISIZE, 7);
  expect_int(STRVIEW("9u"), SEMANTIC_INT_USIZE, 9);
}

void literal_int_bases_and_separators(void) {
  expect_int(STRVIEW("0xFF"), SEMANTIC_INT_I32, 255);
  expect_int(STRVIEW("0xFFu8"), SEMANTIC_INT_U8, 255);
  expect_int(STRVIEW("0b1010"), SEMANTIC_INT_I32, 10);
  expect_int(STRVIEW("0o17"), SEMANTIC_INT_I32, 15);
  expect_int(STRVIEW("1_000u64"), SEMANTIC_INT_U64, 1000);
  expect_int(STRVIEW("0xF_F"), SEMANTIC_INT_I32, 255);
  expect_int(STRVIEW("0b_0000_1111"), SEMANTIC_INT_I32, 15);
}

void literal_int_unsuffixed_default(void) {
  expect_int(STRVIEW("5"), SEMANTIC_INT_I32, 5);
  expect_int(STRVIEW("0"), SEMANTIC_INT_I32, 0);
}

void literal_int_range_signed(void) {
  expect_int(STRVIEW("127i8"), SEMANTIC_INT_I8, 127);
  expect_range(STRVIEW("128i8"), SEMANTIC_INT_I8);
  expect_int(STRVIEW("2147483647i32"), SEMANTIC_INT_I32, 2147483647u);
  expect_range(STRVIEW("2147483648i32"), SEMANTIC_INT_I32);
  expect_int(STRVIEW("9223372036854775807i64"), SEMANTIC_INT_I64, 9223372036854775807ull);
  expect_range(STRVIEW("9223372036854775808i64"), SEMANTIC_INT_I64);
  expect_range(STRVIEW("99999999999999999999999u64"), SEMANTIC_INT_U64); // exceeds u64
}

void literal_int_range_unsigned(void) {
  expect_int(STRVIEW("255u8"), SEMANTIC_INT_U8, 255);
  expect_range(STRVIEW("256u8"), SEMANTIC_INT_U8);
  expect_int(STRVIEW("4294967295u32"), SEMANTIC_INT_U32, 4294967295u);
  expect_range(STRVIEW("4294967296u32"), SEMANTIC_INT_U32);
  expect_int(STRVIEW("18446744073709551615u64"), SEMANTIC_INT_U64, UINT64_MAX);
}

void literal_int_negate_forms(void) {
  SemanticIntLiteral n = semantic_int_negate(semantic_int_literal(STRVIEW("5i8")));
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_LIT_OK, n.status);
  TEST_ASSERT_EQUAL_UINT64(0xFB, n.bits); // -5

  n = semantic_int_negate(semantic_int_literal(STRVIEW("128i8")));
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_LIT_OK, n.status); // INT_MIN fits
  TEST_ASSERT_EQUAL_UINT64(0x80, n.bits);

  n = semantic_int_negate(semantic_int_literal(STRVIEW("129i8")));
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_LIT_RANGE, n.status);

  n = semantic_int_negate(semantic_int_literal(STRVIEW("0u32")));
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_LIT_OK, n.status);
  TEST_ASSERT_EQUAL_UINT64(0, n.bits);

  n = semantic_int_negate(semantic_int_literal(STRVIEW("1u32")));
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_LIT_OK, n.status);
  TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFF, n.bits); // wrap
}

void literal_string_content_escapes(void) {
  Arena *a = arena_create();

  Strview s = semantic_string_content(STRVIEW("\"hi\""), a);
  TEST_ASSERT_EQUAL_size_t(2, s.len);
  TEST_ASSERT_EQUAL_MEMORY("hi", s.data, 2);

  s = semantic_string_content(STRVIEW("\"a\\nb\\tc\""), a);
  TEST_ASSERT_EQUAL_size_t(5, s.len);
  TEST_ASSERT_EQUAL_MEMORY("a\nb\tc", s.data, 5);

  s = semantic_string_content(STRVIEW("\"q\\\"q\\\\z\""), a);
  TEST_ASSERT_EQUAL_size_t(5, s.len);
  TEST_ASSERT_EQUAL_MEMORY("q\"q\\z", s.data, 5);

  s = semantic_string_content(STRVIEW("\"\\x41\\0\""), a);
  TEST_ASSERT_EQUAL_size_t(2, s.len);
  TEST_ASSERT_EQUAL_MEMORY("A\0", s.data, 2);

  s = semantic_string_content(STRVIEW("\"\\u{1F600}\""), a);
  TEST_ASSERT_EQUAL_size_t(4, s.len);
  TEST_ASSERT_EQUAL_MEMORY("\xF0\x9F\x98\x80", s.data, 4);

  arena_destroy(a);
}

void literal_rune_scalars(void) {
  TEST_ASSERT_EQUAL_UINT32(0x61, semantic_rune_scalar(STRVIEW("'a'")));
  TEST_ASSERT_EQUAL_UINT32(0x0A, semantic_rune_scalar(STRVIEW("'\\n'")));
  TEST_ASSERT_EQUAL_UINT32(0x27, semantic_rune_scalar(STRVIEW("'\\''")));
  TEST_ASSERT_EQUAL_UINT32(0x41, semantic_rune_scalar(STRVIEW("'\\x41'")));
  TEST_ASSERT_EQUAL_UINT32(0x0, semantic_rune_scalar(STRVIEW("'\\0'")));
  TEST_ASSERT_EQUAL_UINT32(0x10FFFF, semantic_rune_scalar(STRVIEW("'\\u{10FFFF}'")));
  TEST_ASSERT_EQUAL_UINT32(0x20AC, semantic_rune_scalar(STRVIEW("'€'"))); // raw UTF-8
}

static const TestDispatchEntry ENTRIES[] = {
    {"literal_int_type_matrix", literal_int_type_matrix},
    {"literal_int_bases_and_separators", literal_int_bases_and_separators},
    {"literal_int_unsuffixed_default", literal_int_unsuffixed_default},
    {"literal_int_range_signed", literal_int_range_signed},
    {"literal_int_range_unsigned", literal_int_range_unsigned},
    {"literal_int_negate_forms", literal_int_negate_forms},
    {"literal_string_content_escapes", literal_string_content_escapes},
    {"literal_rune_scalars", literal_rune_scalars},
};

TEST_DISPATCH_MAIN(ENTRIES)
