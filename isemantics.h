/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file isemantics.h
 * @brief Veřejný hlavičkový soubor pro sémantickou analýzu
 * @author Natalie Morávková (<xmoravn00>)
 */

#ifndef ISEMANTICS_H
#define ISEMANTICS_H

#include "ast.h"
#include "ierror.h"

/**
 * @brief Provede kompletní semantickou analýzu AST a jeho obohacení.
 *
 * @param root Ukazatel na kořen AST.
 * @return 0 při úspěchu, jinak chybový kód IFJ26.
 */
ErrorCode check_semantics(ASTNode *root);

#endif // ISEMANTICS_H
