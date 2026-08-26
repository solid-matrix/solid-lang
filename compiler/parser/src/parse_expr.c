#include <stdbool.h>

#include "parse_shared.h"
#include "parser.h"
#include "parser_result.h"
#include "span.h"
#include "strview.h"
#include "syntax_error.h"
#include "syntax_node.h"

static const Strview OPERATOR_LOR = STRVIEW("||");
static const Strview OPERATOR_LXOR = STRVIEW("^^");
static const Strview OPERATOR_LAND = STRVIEW("&&");

static const Strview OPERATOR_EQ = STRVIEW("==");
static const Strview OPERATOR_NEQ = STRVIEW("!=");
static const Strview OPERATOR_LT = STRVIEW("<");
static const Strview OPERATOR_GT = STRVIEW(">");
static const Strview OPERATOR_LTE = STRVIEW("<=");
static const Strview OPERATOR_GTE = STRVIEW(">=");

static const Strview OPERATOR_BOR = STRVIEW("|");
static const Strview OPERATOR_BXOR = STRVIEW("^");
static const Strview OPERATOR_BAND = STRVIEW("&");

static const Strview OPERATOR_SHL = STRVIEW("<<");
static const Strview OPERATOR_SHR = STRVIEW(">>");

static const Strview OPERATOR_ADD = STRVIEW("+");
static const Strview OPERATOR_SUB = STRVIEW("-");
static const Strview OPERATOR_MUL = STRVIEW("*");
static const Strview OPERATOR_DIV = STRVIEW("/");
static const Strview OPERATOR_MOD = STRVIEW("%");

static const Strview OPERATOR_PLUS = STRVIEW("+");
static const Strview OPERATOR_MINUS = STRVIEW("-");
static const Strview OPERATOR_LNOT = STRVIEW("!");
static const Strview OPERATOR_BNOT = STRVIEW("~");
static const Strview OPERATOR_DEREF = STRVIEW("*");

static const Strview OPERATOR_DOT = STRVIEW(".");
static const Strview OPERATOR_INDEX = STRVIEW("[");
static const Strview OPERATOR_CALL = STRVIEW("(");
static const Strview OPERATOR_END_INDEX = STRVIEW("]");
static const Strview OPERATOR_END_CALL = STRVIEW(")");

/* Ladder order, loosest to tightest:
 *   ||   ^^   &&   == != < > <= >=   & | ^   << >>   + -   * / %
 * UnaryOp: - + ! ~ *        Postfix: .id  [Expr]  (CallArgs)
 *
 * Recovery doctrine across the whole expression grammar: a recognized
 * token is always consumed — on a missing right-hand side the operator
 * itself becomes the recovery run, one EXPECTED_EXPR is reported at
 * the failure point, and the level stops with whatever tree was built
 * so far.
 */

static ParserResult parse_named_expr(const Parser *parser, Span span) {
  // W2: NamePath-based operands.
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

static ParserResult parse_sub_expr(const Parser *parser, Span span) {
  if (!(span_len(span) > 0 &&
        source_byte_at(parser->source, span.start) == '('))
    return parser_result_not_match(span);

  Span inner = skip_trivia(parser->source, span_advance(span, 1));

  ParserResult expr = parse_expr(parser, inner);
  if (!expr.matched || expr.node == NULL) {
    return parser_result_matched(
        span, NULL,
        syntax_errorlist_prepend(
            parser->arena, expr.errors,
            syntax_error_create(SYNTAX_EXPECTED_EXPR, inner)));
  }

  Span close = skip_trivia(parser->source, expr.rem);
  if (!(span_len(close) > 0 &&
        source_byte_at(parser->source, close.start) == ')')) {
    // Transparent parens: the inner node survives, the span does not.
    return parser_result_matched(
        expr.rem, expr.node,
        syntax_errorlist_prepend(
            parser->arena, expr.errors,
            syntax_error_create(SYNTAX_EXPECTED_RPAREN, close)));
  }

  return parser_result_matched(span_advance(close, 1), expr.node, expr.errors);
}

static ParserResult parse_operand_expr(const Parser *parser, Span span) {
  // StructLit/ArrayLit join here once their type dependencies land.

  // A matched result always beats an unmatched one, even when recovery
  // rewound its remainder back to the start.
  ParserResult best = parse_number_lit_expr(parser, span);

  ParserResult rune = parse_rune_lit_expr(parser, span);
  if (rune.matched && (!best.matched || rune.rem.start > best.rem.start))
    best = rune;

  ParserResult string = parse_string_lit_expr(parser, span);
  if (string.matched && (!best.matched || string.rem.start > best.rem.start))
    best = string;

  ParserResult named = parse_named_expr(parser, span);
  if (named.matched && (!best.matched || named.rem.start > best.rem.start))
    best = named;

  ParserResult sub = parse_sub_expr(parser, span);
  if (sub.matched && (!best.matched || sub.rem.start > best.rem.start))
    best = sub;

  return best;
}

/* ---- postfix ----------------------------------------------------------- */

// CallArgs = Expr { "," Expr } (minimal form).
//
// Builds the CALL_EXPR with callee left open; the postfix layer fills
// it in once the receiver is known.
static ParserResult parse_call_args(const Parser *parser, Span span) {
  ParserResult first = parse_expr(parser, span);
  if (!first.matched || first.node == NULL) {
    return parser_result_matched(
        span, NULL,
        syntax_errorlist_prepend(
            parser->arena, first.errors,
            syntax_error_create(SYNTAX_EXPECTED_EXPR, span)));
  }

  SyntaxErrorList *errors = first.errors;
  SyntaxNodeList *arguments = syntax_nodelist_append(
      parser->arena, syntax_nodelist_empty(), first.node);
  Span rem = first.rem;

  while (true) {
    Span adv = skip_trivia(parser->source, rem);
    if (!(span_len(adv) > 0 &&
          source_byte_at(parser->source, adv.start) == ','))
      break; // trivia stays with the enclosing construct

    Span next_span = skip_trivia(parser->source, span_advance(adv, 1));
    ParserResult next = parse_expr(parser, next_span);
    if (!next.matched || next.node == NULL) {
      // Consume the dangling comma as a recovery run.
      return parser_result_matched(
          next_span, NULL,
          syntax_errorlist_prepend(
              parser->arena, errors,
              syntax_error_create(SYNTAX_EXPECTED_EXPR, next_span)));
    }

    arguments = syntax_nodelist_append(parser->arena, arguments, next.node);
    errors = syntax_errorlist_concat(parser->arena, next.errors, errors);
    rem = next.rem;
  }

  SyntaxCallExpr *call = arena_alloc(parser->arena, sizeof(SyntaxCallExpr));
  *call = (SyntaxCallExpr){
      .header =
          syntax_node_header(SYNTAX_KIND_CALL_EXPR, span_consumed(span, rem)),
      .callee = NULL, // filled in by the postfix layer
      .arguments = arguments,
  };
  return parser_result_matched(rem, (SyntaxNode *)call, errors);
}

static ParserResult parse_postfix_expr(const Parser *parser, Span span) {

  ParserResult op_res = parse_operand_expr(parser, span);
  if (!op_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = op_res.errors;
  SyntaxNode *node = op_res.node;
  Span rem = op_res.rem;

  while (node != NULL) { // error recovery may yield a NULL receiver
    Span adv = skip_trivia(parser->source, rem);

    if (match(parser->source, adv, OPERATOR_DOT)) {
      Span name_span =
          skip_trivia(parser->source, span_advance(adv, OPERATOR_DOT.len));

      ParserResult id = parse_identifier(parser, name_span);
      if (!id.matched) {
        return parser_result_matched(
            rem, node, // keep the receiver
            syntax_errorlist_prepend(
                parser->arena, errors,
                syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, name_span)));
      }

      SyntaxDotExpr *dot = arena_alloc(parser->arena, sizeof(SyntaxDotExpr));
      *dot = (SyntaxDotExpr){
          .header = syntax_node_header(SYNTAX_KIND_DOT_EXPR,
                                       span_consumed(span, id.rem)),
          .receiver = node,
          .name = (SyntaxIdentifier *)id.node,
      };
      node = (SyntaxNode *)dot;
      rem = id.rem;
      continue;
    }

    if (match(parser->source, adv, OPERATOR_INDEX)) {
      Span inner =
          skip_trivia(parser->source, span_advance(adv, OPERATOR_INDEX.len));

      ParserResult index = parse_expr(parser, inner);
      if (!index.matched || index.node == NULL) {
        return parser_result_matched(
            rem, node,
            syntax_errorlist_prepend(
                parser->arena, index.errors,
                syntax_error_create(SYNTAX_EXPECTED_EXPR, inner)));
      }

      Span close = skip_trivia(parser->source, index.rem);
      if (!match(parser->source, close, OPERATOR_END_INDEX)) {
        return parser_result_matched(
            rem, node,
            syntax_errorlist_prepend(
                parser->arena, errors,
                syntax_error_create(SYNTAX_EXPECTED_EXPR, close)));
      }

      SyntaxIndexExpr *ix = arena_alloc(parser->arena, sizeof(SyntaxIndexExpr));
      *ix = (SyntaxIndexExpr){
          .header = syntax_node_header(
              SYNTAX_KIND_INDEX_EXPR,
              span_consumed(span, span_advance(close, OPERATOR_END_INDEX.len))),
          .receiver = node,
          .index = index.node,
      };
      node = (SyntaxNode *)ix;
      rem = span_advance(close, OPERATOR_END_INDEX.len);
      continue;
    }

    if (match(parser->source, adv, OPERATOR_CALL)) {
      Span inner =
          skip_trivia(parser->source, span_advance(adv, OPERATOR_CALL.len));

      if (match(parser->source, inner, OPERATOR_END_CALL)) { // "()"
        Span end = span_advance(inner, OPERATOR_END_CALL.len);
        SyntaxCallExpr *call =
            arena_alloc(parser->arena, sizeof(SyntaxCallExpr));
        *call = (SyntaxCallExpr){
            .header = syntax_node_header(SYNTAX_KIND_CALL_EXPR,
                                         span_consumed(span, end)),
            .callee = node,
            .arguments = syntax_nodelist_empty(),
        };
        node = (SyntaxNode *)call;
        rem = end;
        continue;
      }

      ParserResult args = parse_call_args(parser, inner);
      errors = syntax_errorlist_concat(parser->arena, args.errors, errors);
      if (!args.matched || args.node == NULL) {
        return parser_result_matched(rem, node, errors);
      }

      Span close = skip_trivia(parser->source, args.rem);
      if (!match(parser->source, close, OPERATOR_END_CALL)) {
        return parser_result_matched(
            rem, node,
            syntax_errorlist_prepend(
                parser->arena, errors,
                syntax_error_create(SYNTAX_EXPECTED_RPAREN, close)));
      }

      ((SyntaxCallExpr *)args.node)->callee = node;
      ((SyntaxCallExpr *)args.node)->header.span =
          span_consumed(span, span_advance(close, OPERATOR_END_CALL.len));
      node = args.node;
      rem = span_advance(close, OPERATOR_END_CALL.len);
      continue;
    }

    break; // no postfix follows; trivia belongs to the enclosing construct
  }

  return parser_result_matched(rem, node, errors);
}

/* ---- unary ----------------------------------------------------------------
 */

static ParserResult parse_unary_expr(const Parser *parser, Span span) {

  SyntaxOperator op;
  size_t opw = 0;

  if (match(parser->source, span, OPERATOR_MINUS)) {
    op = SYNTAX_OPERATOR_MINUS;
    opw = OPERATOR_MINUS.len;
  } else if (match(parser->source, span, OPERATOR_PLUS)) {
    op = SYNTAX_OPERATOR_PLUS;
    opw = OPERATOR_PLUS.len;
  } else if (match(parser->source, span, OPERATOR_LNOT)) {
    op = SYNTAX_OPERATOR_LNOT;
    opw = OPERATOR_LNOT.len;
  } else if (match(parser->source, span, OPERATOR_BNOT)) {
    op = SYNTAX_OPERATOR_BNOT;
    opw = OPERATOR_BNOT.len;
  } else if (match(parser->source, span, OPERATOR_DEREF)) {
    op = SYNTAX_OPERATOR_DEREF;
    opw = OPERATOR_DEREF.len;
  } else {
    return parse_postfix_expr(parser, span);
  }

  Span rem = span_advance(span, opw);
  SyntaxErrorList *errors = syntax_errorlist_empty();
  ParserResult un_res =
      parse_unary_expr(parser, skip_trivia(parser->source, rem));

  if (!un_res.matched) {
    errors = syntax_errorlist_prepend(
        parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
  }

  rem = un_res.rem;

  errors = syntax_errorlist_concat(parser->arena, un_res.errors, errors);

  SyntaxUnaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxUnaryExpr));
  *expr = (SyntaxUnaryExpr){
      .header =
          syntax_node_header(SYNTAX_KIND_UNARY_EXPR, span_consumed(span, rem)),
      .operator = op,
      .operand = un_res.node,
  };

  return parser_result_matched(rem, (SyntaxNode *)expr, errors);
}

/* ---- multiplicative: * / % ----------------------------------------------- */

static ParserResult parse_multiplicative_expr(const Parser *parser, Span span) {

  ParserResult un_res = parse_unary_expr(parser, span);

  if (!un_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = un_res.errors;
  SyntaxNode *left = un_res.node;
  Span rem = un_res.rem;

  while (left != NULL) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op;
    size_t opw = 0;

    if (match(parser->source, adv, OPERATOR_MUL)) {
      op = SYNTAX_OPERATOR_MUL;
      opw = OPERATOR_MUL.len;
    } else if (match(parser->source, adv, OPERATOR_DIV)) {
      op = SYNTAX_OPERATOR_DIV;
      opw = OPERATOR_DIV.len;
    } else if (match(parser->source, adv, OPERATOR_MOD)) {
      op = SYNTAX_OPERATOR_MOD;
      opw = OPERATOR_MOD.len;
    } else {
      break;
    }

    rem = span_advance(adv, opw);

    un_res = parse_unary_expr(parser, skip_trivia(parser->source, rem));

    if (!un_res.matched) {
      errors = syntax_errorlist_prepend(
          parser->arena, errors,
          syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
      break;
    }
    rem = un_res.rem;

    errors = syntax_errorlist_concat(parser->arena, un_res.errors, errors);

    SyntaxBinaryExpr *expr =
        arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR,
                                     span_consumed(span, rem)),
        .operator = op,
        .left = left,
        .right = un_res.node,
    };

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

/* ---- additive: + - ---------------------------------------------------------
 */

static ParserResult parse_additive_expr(const Parser *parser, Span span) {

  ParserResult mul_res = parse_multiplicative_expr(parser, span);

  if (!mul_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = mul_res.errors;
  SyntaxNode *left = mul_res.node;
  Span rem = mul_res.rem;

  while (left != NULL) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op;
    size_t opw = 0;

    if (match(parser->source, adv, OPERATOR_ADD)) {
      op = SYNTAX_OPERATOR_ADD;
      opw = OPERATOR_ADD.len;
    } else if (match(parser->source, adv, OPERATOR_SUB)) {
      op = SYNTAX_OPERATOR_SUB;
      opw = OPERATOR_SUB.len;
    } else {
      break;
    }

    rem = span_advance(adv, opw);

    mul_res =
        parse_multiplicative_expr(parser, skip_trivia(parser->source, rem));

    if (!mul_res.matched) {
      errors = syntax_errorlist_prepend(
          parser->arena, errors,
          syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
      break;
    }
    rem = mul_res.rem;

    errors = syntax_errorlist_concat(parser->arena, mul_res.errors, errors);

    SyntaxBinaryExpr *expr =
        arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR,
                                     span_consumed(span, rem)),
        .operator = op,
        .left = left,
        .right = mul_res.node,
    };

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

/* ---- shift: << >>
 * ------------------------------------------------------------ */

static ParserResult parse_shift_expr(const Parser *parser, Span span) {

  ParserResult add_res = parse_additive_expr(parser, span);

  if (!add_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = add_res.errors;
  SyntaxNode *left = add_res.node;
  Span rem = add_res.rem;

  while (left != NULL) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op;
    size_t opw = 0;

    if (match(parser->source, adv, OPERATOR_SHL)) {
      op = SYNTAX_OPERATOR_SHL;
      opw = OPERATOR_SHL.len;
    } else if (match(parser->source, adv, OPERATOR_SHR)) {
      op = SYNTAX_OPERATOR_SHR;
      opw = OPERATOR_SHR.len;
    } else {
      break;
    }

    rem = span_advance(adv, opw);

    add_res = parse_additive_expr(parser, skip_trivia(parser->source, rem));

    if (!add_res.matched) {
      errors = syntax_errorlist_prepend(
          parser->arena, errors,
          syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
      break;
    }
    rem = add_res.rem;

    errors = syntax_errorlist_concat(parser->arena, add_res.errors, errors);

    SyntaxBinaryExpr *expr =
        arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR,
                                     span_consumed(span, rem)),
        .operator = op,
        .left = left,
        .right = add_res.node,
    };

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

/* ---- bitwise: & | ^  (never on "&&" "||" "^^") ------------------------------
 */

static ParserResult parse_bitwise_expr(const Parser *parser, Span span) {

  ParserResult sh_res = parse_shift_expr(parser, span);

  if (!sh_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = sh_res.errors;
  SyntaxNode *left = sh_res.node;
  Span rem = sh_res.rem;

  while (left != NULL) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op;
    size_t opw = 0;

    // A doubled form owned by a looser level ("&&", "||", "^^") is not
    // this level's single-byte operator.
    if (match(parser->source, adv, OPERATOR_BAND) &&
        !(span_len(adv) >= 2 &&
          source_byte_at(parser->source, adv.start + 1) == '&')) {
      op = SYNTAX_OPERATOR_BAND;
      opw = OPERATOR_BAND.len;
    } else if (match(parser->source, adv, OPERATOR_BOR) &&
               !(span_len(adv) >= 2 &&
                 source_byte_at(parser->source, adv.start + 1) == '|')) {
      op = SYNTAX_OPERATOR_BOR;
      opw = OPERATOR_BOR.len;
    } else if (match(parser->source, adv, OPERATOR_BXOR) &&
               !(span_len(adv) >= 2 &&
                 source_byte_at(parser->source, adv.start + 1) == '^')) {
      op = SYNTAX_OPERATOR_BXOR;
      opw = OPERATOR_BXOR.len;
    } else {
      break;
    }

    rem = span_advance(adv, opw);

    sh_res = parse_shift_expr(parser, skip_trivia(parser->source, rem));

    if (!sh_res.matched) {
      errors = syntax_errorlist_prepend(
          parser->arena, errors,
          syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
      break;
    }
    rem = sh_res.rem;

    errors = syntax_errorlist_concat(parser->arena, sh_res.errors, errors);

    SyntaxBinaryExpr *expr =
        arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR,
                                     span_consumed(span, rem)),
        .operator = op,
        .left = left,
        .right = sh_res.node,
    };

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

/* ---- relational: == != < > <= >=
 * --------------------------------------------- */

static ParserResult parse_relational_expr(const Parser *parser, Span span) {

  ParserResult bw_res = parse_bitwise_expr(parser, span);

  if (!bw_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = bw_res.errors;
  SyntaxNode *left = bw_res.node;
  Span rem = bw_res.rem;

  while (left != NULL) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op;
    size_t opw = 0;

    if (match(parser->source, adv, OPERATOR_EQ)) {
      op = SYNTAX_OPERATOR_EQ;
      opw = OPERATOR_EQ.len;
    } else if (match(parser->source, adv, OPERATOR_NEQ)) {
      op = SYNTAX_OPERATOR_NEQ;
      opw = OPERATOR_NEQ.len;
    } else if (match(parser->source, adv, OPERATOR_LTE)) {
      op = SYNTAX_OPERATOR_LTE;
      opw = OPERATOR_LTE.len;
    } else if (match(parser->source, adv, OPERATOR_GTE)) {
      op = SYNTAX_OPERATOR_GTE;
      opw = OPERATOR_GTE.len;
    } else if (match(parser->source, adv, OPERATOR_LT)) {
      op = SYNTAX_OPERATOR_LT;
      opw = OPERATOR_LT.len;
    } else if (match(parser->source, adv, OPERATOR_GT)) {
      op = SYNTAX_OPERATOR_GT;
      opw = OPERATOR_GT.len;
    } else {
      break;
    }

    rem = span_advance(adv, opw);

    bw_res = parse_bitwise_expr(parser, skip_trivia(parser->source, rem));

    if (!bw_res.matched) {
      errors = syntax_errorlist_prepend(
          parser->arena, errors,
          syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
      break;
    }
    rem = bw_res.rem;

    errors = syntax_errorlist_concat(parser->arena, bw_res.errors, errors);

    SyntaxBinaryExpr *expr =
        arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR,
                                     span_consumed(span, rem)),
        .operator = op,
        .left = left,
        .right = bw_res.node,
    };

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

/* ---- logical and: &&
 * ------------------------------------------------------------ */

static ParserResult parse_logical_and_expr(const Parser *parser, Span span) {

  ParserResult rel_res = parse_relational_expr(parser, span);

  if (!rel_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = rel_res.errors;
  SyntaxNode *left = rel_res.node;
  Span rem = rel_res.rem;

  while (left != NULL) {
    Span adv = skip_trivia(parser->source, rem);

    if (!match(parser->source, adv, OPERATOR_LAND))
      break;

    rem = span_advance(adv, OPERATOR_LAND.len);

    rel_res = parse_relational_expr(parser, skip_trivia(parser->source, rem));

    if (!rel_res.matched) {
      errors = syntax_errorlist_prepend(
          parser->arena, errors,
          syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
      break;
    }
    rem = rel_res.rem;

    errors = syntax_errorlist_concat(parser->arena, rel_res.errors, errors);

    SyntaxBinaryExpr *expr =
        arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR,
                                     span_consumed(span, rem)),
        .operator = SYNTAX_OPERATOR_LAND,
        .left = left,
        .right = rel_res.node,
    };

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

/* ---- logical xor: ^^
 * --------------------------------------------------------------- */

static ParserResult parse_logical_xor_expr(const Parser *parser, Span span) {

  ParserResult and_res = parse_logical_and_expr(parser, span);

  if (!and_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = and_res.errors;
  SyntaxNode *left = and_res.node;
  Span rem = and_res.rem;

  while (left != NULL) {
    Span adv = skip_trivia(parser->source, rem);

    if (!match(parser->source, adv, OPERATOR_LXOR))
      break;

    rem = span_advance(adv, OPERATOR_LXOR.len);

    and_res = parse_logical_and_expr(parser, skip_trivia(parser->source, rem));

    if (!and_res.matched) {
      errors = syntax_errorlist_prepend(
          parser->arena, errors,
          syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
      break;
    }
    rem = and_res.rem;

    errors = syntax_errorlist_concat(parser->arena, and_res.errors, errors);

    SyntaxBinaryExpr *expr =
        arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR,
                                     span_consumed(span, rem)),
        .operator = SYNTAX_OPERATOR_LXOR,
        .left = left,
        .right = and_res.node,
    };

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

/* ---- logical or: ||
 * ------------------------------------------------------------------- */

static ParserResult parse_logical_or_expr(const Parser *parser, Span span) {

  ParserResult xor_res = parse_logical_xor_expr(parser, span);

  if (!xor_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = xor_res.errors;
  SyntaxNode *left = xor_res.node;
  Span rem = xor_res.rem;

  while (left != NULL) {
    Span adv = skip_trivia(parser->source, rem);

    if (!match(parser->source, adv, OPERATOR_LOR))
      break;

    rem = span_advance(adv, OPERATOR_LOR.len);

    xor_res = parse_logical_xor_expr(parser, skip_trivia(parser->source, rem));

    if (!xor_res.matched) {
      errors = syntax_errorlist_prepend(
          parser->arena, errors,
          syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
      break;
    }
    rem = xor_res.rem;

    errors = syntax_errorlist_concat(parser->arena, xor_res.errors, errors);

    SyntaxBinaryExpr *expr =
        arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR,
                                     span_consumed(span, rem)),
        .operator = SYNTAX_OPERATOR_LOR,
        .left = left,
        .right = xor_res.node,
    };

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

/* ---- entry
 * ------------------------------------------------------------------------------
 */

ParserResult parse_expr(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  return parse_logical_or_expr(parser, span);
}
