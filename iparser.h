/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file iparser.h
 * @brief Veřejné rozhraní pro syntaktický analyzátor (parser) (Top-down, rekurzivní sestup)
 * @author František Bilavčík (<xbilavf00>)
 *
 */

#ifndef IPARSER_H
#define IPARSER_H

#include "ast.h"
#include "ierror.h"

/**
 * @brief Hlavní funkce parseru, bere tokeny z lexeru a staví z něj AST
 *
 * @param root Ukazatel na ukazatel na kořen AST, pokud je úspěšně sestavený, tak se nastaví na
 * kořen AST, zpracuje pravidal 1 <program> a 3 <decl_list>
 * @return 0/1/2/99, 0 = bez chyby, 1 = lexikální chyba, 2 = syntaktická chyba, 99 = interní chyba.
 * *root == NULL při chybě, pokud je úspěšně sestavený,n astaví na kořen AST
 */
ErrorCode parse_program(ASTNode **root);

#endif // IPARSER_H
