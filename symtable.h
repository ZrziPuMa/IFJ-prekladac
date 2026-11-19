/**
 * Implementace tabulky symbolů jazyka IFJ26
 * @file symtable.h
 * @brief Hlavičkový soubor tabulky symbolů
 * @author Daniela Sladká, xsladkd00
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "isymtable.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Rotace stromu doleva
 * @param root Ukazatel na kořen stromu, který se má rotovat
 * @return ErrorCode chybový kód
 */
ErrorCode rotate_left(tBtNode **root);

/**
 * @brief Rotace stromu doprava
 * @param root Ukazatel na kořen stromu, který se má rotovat
 * @return ErrorCode chybový kód
 */
ErrorCode rotate_right(tBtNode **root);

/**
 * @brief Rotace stromu doleva a pak doprava
 * @param root Ukazatel na kořen stromu, který se má rotovat
 * @return ErrorCode chybový kód
 */
ErrorCode rotate_left_right(tBtNode **root);

/**
 * @brief Rotace stromu doprava a pak doleva
 * @param root Ukazatel na kořen stromu, který se má rotovat
 * @return ErrorCode chybový kód
 */
ErrorCode rotate_right_left(tBtNode **root);

/**
 * @brief Nahrazení nejpravějším uzlem
 * @param node Uzel, který se má nahradit
 * @param root Ukazatel na kořen stromu
 * @return ErrorCode chybový kód
 */
ErrorCode replace_most_right(tBtNode *node, tBtNode **root);

/**
 * @brief Nahrazení nejlevějším uzlem
 * @param node Uzel, který se má nahradit
 * @param root Ukazatel na kořen stromu
 * @return ErrorCode chybový kód
 */
ErrorCode replace_most_left(tBtNode *node, tBtNode **root);

/**
 * @brief Kontrola výšky stromu pro případnou rotaci
 * @param root Ukazatel na kořen stromu
 * @param first_unbalanced Ukazatel na první nevyvážený uzel
 * @return Výška stromu, -1 když dojde k chybě, -2 když je špatný ukazatel
 */
int check_height(tBtNode **root, tBtNode ***first_unbalanced);

/**
 * @brief Vyvážení stromu
 * @param root Ukazatel na kořen stromu
 * @return ErrorCode chybový kód
 */
ErrorCode realign_tree(tBtNode **root);

#endif // SYMTABLE_H