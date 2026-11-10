/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file expression.c
 * @brief Implementace precedenční SA (Bottom-up)
 * @author František Bilavčík (<xbilavf00>)
 */

#include "expression.h"

#include "parser.h"

static PrecedentSymbol token_to_sym(const tToken *t, bool expect_operand);
