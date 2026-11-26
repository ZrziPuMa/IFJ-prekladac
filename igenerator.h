/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file igenerator.h
 * @brief Veřejný hlavičkový soubor generátoru
 * @author Martin Chlubna (<xchlubm00>)
 *
 */

#ifndef I_GENERATOR_H
#define I_GENERATOR_H

#include "ast.h"
#include "ierror.h"

/**
 * @brief Hlavní řídící funkce generátoru (Zavolá ji main)
 *
 * @param root Ukazatel ne kořenový uzel AST
 * @return ErrorCode Chybový kód, který vrátí do main()
 */
ErrorCode generate_code(const ASTNode *root);

#endif