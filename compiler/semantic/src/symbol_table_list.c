/**
 * @file symbol_table_list.c
 * @brief Immutable singly-linked list of symbol tables.
 * @author solid-matrix
 */

#include <assert.h>

#include "symbol_table_list.h"

LIST_DEFINE(SemanticSymbolTableList, semantic_symbol_table_list, SemanticSymbolTable *)
