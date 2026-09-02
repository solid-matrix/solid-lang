/**
 * @file symbol_table_chain.c
 * @brief Immutable singly-linked chain of symbol table lists.
 * @author solid-matrix
 */

#include <assert.h>

#include "symbol_table_chain.h"

LIST_DEFINE(SemanticSymbolTableChain, semantic_symbol_table_chain, SemanticSymbolTableList *)
