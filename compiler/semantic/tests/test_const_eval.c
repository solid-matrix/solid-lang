/**
 * @file test_const_eval.c
 * @brief Unit tests for the foldable operator evaluator (spec §10.1).
 * @author solid-matrix
 */

#include "semantic_cvalue.h"
#include "test_support.h"

static SemanticCValue iv(SemanticIntType t, uint64_t bits) { return semantic_cvalue_int(t, bits); }

static void expect_value(SemanticEvalOutcome o, SemanticCValueKind kind, uint64_t bits) {
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_EVAL_OK, o.status);
  TEST_ASSERT_EQUAL_HEX32(kind, o.value.kind);
  TEST_ASSERT_EQUAL_UINT64(bits, o.value.bits);
}

static void expect_int(SemanticEvalOutcome o, SemanticIntType t, uint64_t bits) {
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_EVAL_OK, o.status);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_CV_INT, o.value.kind);
  TEST_ASSERT_EQUAL_HEX32(t, o.value.int_type);
  TEST_ASSERT_EQUAL_UINT64(bits, o.value.bits);
}

static void expect_diag(SemanticEvalOutcome o, SemanticEvalStatus s) {
  TEST_ASSERT_EQUAL_HEX32(s, o.status);
}

void eval_int_arithmetic_wraps(void) {
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_ADD, iv(SEMANTIC_INT_U8, 255),
                                    iv(SEMANTIC_INT_U8, 1)),
             SEMANTIC_INT_U8, 0);
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_ADD, iv(SEMANTIC_INT_I8, 127),
                                    iv(SEMANTIC_INT_I8, 1)),
             SEMANTIC_INT_I8, 0x80); // wraps to INT_MIN
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_SUB, iv(SEMANTIC_INT_U32, 0),
                                    iv(SEMANTIC_INT_U32, 1)),
             SEMANTIC_INT_U32, 0xFFFFFFFF);
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_MUL, iv(SEMANTIC_INT_U16, 300),
                                    iv(SEMANTIC_INT_U16, 300)),
             SEMANTIC_INT_U16, 0x5F90); // 90000 mod 65536 = 24464
}

void eval_int_div_semantics(void) {
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_DIV, iv(SEMANTIC_INT_I32, 7),
                                    iv(SEMANTIC_INT_I32, 2)),
             SEMANTIC_INT_I32, 3);
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_DIV, iv(SEMANTIC_INT_I32, 0xFFFFFFF9),
                                    iv(SEMANTIC_INT_I32, 2)),
             SEMANTIC_INT_I32, 0xFFFFFFFD); // -7/2 = -3 (truncates toward zero)
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_MOD, iv(SEMANTIC_INT_I32, 7),
                                    iv(SEMANTIC_INT_I32, 0xFFFFFFFE)),
             SEMANTIC_INT_I32, 1); // 7 % -2 = 1 (sign follows dividend)
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_DIV, iv(SEMANTIC_INT_U32, 7),
                                    iv(SEMANTIC_INT_U32, 2)),
             SEMANTIC_INT_U32, 3);
}

void eval_div_zero_diag(void) {
  expect_diag(semantic_cvalue_binary(SYNTAX_OPERATOR_DIV, iv(SEMANTIC_INT_I32, 5),
                                     iv(SEMANTIC_INT_I32, 0)),
              SEMANTIC_EVAL_DIV_ZERO);
  expect_diag(semantic_cvalue_binary(SYNTAX_OPERATOR_MOD, iv(SEMANTIC_INT_U32, 5),
                                     iv(SEMANTIC_INT_U32, 0)),
              SEMANTIC_EVAL_DIV_ZERO);
}

void eval_div_overflow_diag(void) {
  expect_diag(semantic_cvalue_binary(SYNTAX_OPERATOR_DIV, iv(SEMANTIC_INT_I32, 0x80000000),
                                     iv(SEMANTIC_INT_I32, 0xFFFFFFFF)),
              SEMANTIC_EVAL_DIV_OVERFLOW);
}

void eval_shift_semantics(void) {
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_SHL, iv(SEMANTIC_INT_U8, 1),
                                    iv(SEMANTIC_INT_U8, 7)),
             SEMANTIC_INT_U8, 128);
  expect_diag(semantic_cvalue_binary(SYNTAX_OPERATOR_SHL, iv(SEMANTIC_INT_U8, 1),
                                     iv(SEMANTIC_INT_U8, 8)),
              SEMANTIC_EVAL_SHIFT_RANGE);
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_SHR, iv(SEMANTIC_INT_I32, 0xFFFFFFF8),
                                    iv(SEMANTIC_INT_I32, 1)),
             SEMANTIC_INT_I32, 0xFFFFFFFC); // arithmetic shift keeps the sign
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_SHR, iv(SEMANTIC_INT_U32, 0x80000000),
                                    iv(SEMANTIC_INT_U32, 31)),
             SEMANTIC_INT_U32, 1); // logical shift on unsigned
}

void eval_bitwise_and_comparisons(void) {
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_BAND, iv(SEMANTIC_INT_I32, 5),
                                    iv(SEMANTIC_INT_I32, 3)),
             SEMANTIC_INT_I32, 1);
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_BOR, iv(SEMANTIC_INT_I32, 5),
                                    iv(SEMANTIC_INT_I32, 3)),
             SEMANTIC_INT_I32, 7);
  expect_int(semantic_cvalue_binary(SYNTAX_OPERATOR_BXOR, iv(SEMANTIC_INT_I32, 5),
                                    iv(SEMANTIC_INT_I32, 3)),
             SEMANTIC_INT_I32, 6);
  expect_value(semantic_cvalue_binary(SYNTAX_OPERATOR_LT, iv(SEMANTIC_INT_I32, 0xFFFFFFFF),
                                      iv(SEMANTIC_INT_I32, 0)),
               SEMANTIC_CV_BOOL, 1); // -1 < 0 (signed)
  expect_value(semantic_cvalue_binary(SYNTAX_OPERATOR_GT, iv(SEMANTIC_INT_U32, 0xFFFFFFFF),
                                      iv(SEMANTIC_INT_U32, 0)),
               SEMANTIC_CV_BOOL, 1); // unsigned: big > 0
}

void eval_bool_logic(void) {
  expect_value(semantic_cvalue_unary(SYNTAX_OPERATOR_LNOT, semantic_cvalue_bool(0)),
               SEMANTIC_CV_BOOL, 1);
  expect_value(semantic_cvalue_binary(SYNTAX_OPERATOR_LAND, semantic_cvalue_bool(1),
                                      semantic_cvalue_bool(0)),
               SEMANTIC_CV_BOOL, 0);
  expect_value(semantic_cvalue_binary(SYNTAX_OPERATOR_LOR, semantic_cvalue_bool(0),
                                      semantic_cvalue_bool(1)),
               SEMANTIC_CV_BOOL, 1);
  expect_value(semantic_cvalue_binary(SYNTAX_OPERATOR_EQ, semantic_cvalue_bool(1),
                                      semantic_cvalue_bool(1)),
               SEMANTIC_CV_BOOL, 1);
}

void eval_string_and_rune_eq(void) {
  SemanticCValue a = semantic_cvalue_string((Strview){.data = (const uint8_t *)"linux", .len = 5});
  SemanticCValue b = semantic_cvalue_string((Strview){.data = (const uint8_t *)"linux", .len = 5});
  SemanticCValue c = semantic_cvalue_string((Strview){.data = (const uint8_t *)"macos", .len = 5});
  expect_value(semantic_cvalue_binary(SYNTAX_OPERATOR_EQ, a, b), SEMANTIC_CV_BOOL, 1);
  expect_value(semantic_cvalue_binary(SYNTAX_OPERATOR_EQ, a, c), SEMANTIC_CV_BOOL, 0);
  expect_value(semantic_cvalue_binary(SYNTAX_OPERATOR_NEQ, a, c), SEMANTIC_CV_BOOL, 1);
  expect_value(semantic_cvalue_binary(SYNTAX_OPERATOR_EQ, semantic_cvalue_rune(0x61),
                                      semantic_cvalue_rune(0x61)),
               SEMANTIC_CV_BOOL, 1);
}

void eval_type_mismatches_diag(void) {
  expect_diag(semantic_cvalue_binary(SYNTAX_OPERATOR_ADD, iv(SEMANTIC_INT_I32, 1),
                                     iv(SEMANTIC_INT_U32, 1)),
              SEMANTIC_EVAL_TYPE); // mixed integer types
  expect_diag(semantic_cvalue_binary(SYNTAX_OPERATOR_EQ, iv(SEMANTIC_INT_U8, 1),
                                     iv(SEMANTIC_INT_I8, 1)),
              SEMANTIC_EVAL_TYPE); // mixed integer types in comparison
  expect_diag(semantic_cvalue_binary(SYNTAX_OPERATOR_ADD,
                                     semantic_cvalue_string((Strview){.data = (const uint8_t *)"a", .len = 1}),
                                     semantic_cvalue_string((Strview){.data = (const uint8_t *)"b", .len = 1})),
              SEMANTIC_EVAL_TYPE); // String8 supports equality only
  expect_diag(semantic_cvalue_unary(SYNTAX_OPERATOR_LNOT, iv(SEMANTIC_INT_I32, 0)),
              SEMANTIC_EVAL_TYPE); // ! on integer
  expect_diag(semantic_cvalue_binary(SYNTAX_OPERATOR_LT, semantic_cvalue_float(STRVIEW("1.0_d")),
                                     semantic_cvalue_float(STRVIEW("2.0_d"))),
              SEMANTIC_EVAL_TYPE); // float operators are not foldable
  expect_diag(semantic_cvalue_binary(SYNTAX_OPERATOR_ADD, semantic_cvalue_deferred(),
                                     semantic_cvalue_deferred()),
              SEMANTIC_EVAL_TYPE); // deferred (layout) values do not fold
}

void eval_unary_int_forms(void) {
  expect_int(semantic_cvalue_unary(SYNTAX_OPERATOR_MINUS, iv(SEMANTIC_INT_I32, 5)),
             SEMANTIC_INT_I32, 0xFFFFFFFB); // -5
  expect_int(semantic_cvalue_unary(SYNTAX_OPERATOR_PLUS, iv(SEMANTIC_INT_I32, 5)),
             SEMANTIC_INT_I32, 5);
  expect_int(semantic_cvalue_unary(SYNTAX_OPERATOR_BNOT, iv(SEMANTIC_INT_U8, 0)),
             SEMANTIC_INT_U8, 0xFF);
}

static const TestDispatchEntry ENTRIES[] = {
    {"eval_int_arithmetic_wraps", eval_int_arithmetic_wraps},
    {"eval_int_div_semantics", eval_int_div_semantics},
    {"eval_div_zero_diag", eval_div_zero_diag},
    {"eval_div_overflow_diag", eval_div_overflow_diag},
    {"eval_shift_semantics", eval_shift_semantics},
    {"eval_bitwise_and_comparisons", eval_bitwise_and_comparisons},
    {"eval_bool_logic", eval_bool_logic},
    {"eval_string_and_rune_eq", eval_string_and_rune_eq},
    {"eval_type_mismatches_diag", eval_type_mismatches_diag},
    {"eval_unary_int_forms", eval_unary_int_forms},
};

TEST_DISPATCH_MAIN(ENTRIES)
