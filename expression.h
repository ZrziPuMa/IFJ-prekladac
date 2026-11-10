/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file expression.h
 * @brief Interní rozhraní pro syntaktický analyzátor (parser) (Bottom-up, precedenční analýza)
 * @author František Bilavčík (<xbilavf00>)
 */

#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "ast.h"
#include "token.h"

#include <stdbool.h>

typedef enum {
    PREC_UNARY_OP, ///< +i or -i
    PREC_NOT,      ///< !i
    PREC_MUL,      ///< i * i
    PREC_DIV,      ///< i / i
    PREC_PLUS,     ///< i + i
    PREC_MINUS,    ///< i - i
    PREC_CONCAT,   ///< i ++ i
    PREC_LT,       ///< i < i
    PREC_LE,       ///< i <= i
    PREC_GT,       ///< i > i
    PREC_GE,       ///< i >= i
    PREC_EQ,       ///< i == i
    PREC_NEQ,      ///< i != i
    PREC_AND,      ///< i && i
    PREC_OR,       ///< i || i
    PREC_I,        ///< i (term)
    PREC_END,      ///< $ (konec výrazu)
    PREC__COUNT,
    PREC__INVALID
} PrecedentSymbol;

typedef enum {
    PREC_REL_ERROR, ///< Chyba (X)
    PREC_REL_LT,    ///< <
    PREC_REL_GT,    ///< >
    PREC_REL_OK     ///< ok
} PrecedentRelation;

typedef enum {
    PI_TERM,
    PI_NONTERM,
    PI_HANDLE
} PrecedentItemKind; // PI_HANDLE je ~,zarážka, pro redukci

typedef struct {
    PrecedentSymbol sym;    ///< Symbol
    ASTNode *node;          ///< U i hotový podstrom z parse_term, jinak NULL
    PrecedentItemKind kind; ///< term / nonterm / handle
    tTokenType op_token;    ///< Původní operátor
    int line;               ///< Číslo řádku, kde se nachází operátor
} PrecedentItem;

#endif // EXPRESSION_H
