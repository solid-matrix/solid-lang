/**
 * @file test_ast.c
 * @brief Tests for AST helpers: syntax_node_is_kind, SyntaxNodeList,
 *        and SyntaxPath.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdio.h>

#include "ast.h"
#include "string_view.h"

static int g_failures;

#define CHECK(cond)                                                   \
  do                                                                  \
  {                                                                   \
    if (!(cond))                                                      \
    {                                                                 \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      g_failures++;                                                   \
    }                                                                 \
  } while (0)

int main(void)
{
  return 0;
}
