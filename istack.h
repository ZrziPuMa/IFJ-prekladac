/**
 * Implementace překladače jazyka IFJ26
 * @file istack.h
 * @brief Hlavičkový soubor zásobníku
 * @author Daniela Sladká, xsladkd00
 */

#ifndef ISTACK_H
#define ISTACK_H

#include "isymtable.h"

/**
 * @brief Datový typ pro prvek zásobníku
 */
typedef struct stack_elem {
    tBtNode *scope; // Ukazatel na kořen stromu reprezentující lokální rozsah platnosti (scope)
    struct stack_elem *next; // Ukazatel na další prvek zásobníku
} tStackElem;

/**
 * @brief Datový typ pro zásobník
 */
typedef struct stack {
    tStackElem *top; // Ukazatel na vrchol zásobníku s lokálními rozsahy platnosti (scope)
} tStack;

/**
 * @brief Inicializace zásobníku
 * @param stack Ukazatel na zásobník, který se má inicializovat
 * @return ErrorCode chybový kód
 */
ErrorCode stack_init(tStack *stack);

/**
 * @brief Vložení nového lokálního rozsahu platnosti (scope) na vrchol zásobníku
 * @param root Ukazatel na kořen stromu reprezentujícího nový lokální rozsah
 * @param stack Ukazatel na zásobník, na jehož vrchol se nový rozsah vloží
 * @return ErrorCode chybový kód
 */
ErrorCode symtable_push_scope(tBtNode **root, tStack *stack);

/**
 * @brief Odebrání aktuálního lokálního rozsahu platnosti z vrcholu zásobníku
 * @param stack Ukazatel na zásobník, ze kterého se odebere aktuální rozsah platnosti
 * @return ErrorCode chybový kód
 */
ErrorCode symtable_pop_scope(tStack *stack);

/**
 * @brief Vložení symbolu do aktuálního (nejvnitřnějšího) bloku
 * @param stack Ukazatel na zásobník, do jehož bloku se bude vkládat
 * @param key Klíč symbolu, který bude vložen
 * @param data Data vloženého symbolu
 * @return ErrorCode chybový kód
 */
ErrorCode stack_insert(tStack *stack, char *key, tBtNodeData data);

/**
 * @brief Vyhledání symbolu od nejvnitřnějšího bloku k nejvnějšímu
 * @param stack Ukazatel na zásobník, ve kterém se bude hledat
 * @param key Klíč hledaného symbolu
 * @param location Ukazatel, kam se uloží nalezený symbol
 * @return ErrorCode chybový kód
 */
ErrorCode stack_search_symbol(tStack *stack, char *key, tBtNode **location);

/**
 * @brief Získání vrcholu zásobníku
 * @param stack Ukazatel na zásobník
 * @return Ukazatel na vrchol zásobníku
 */
tStackElem *stack_top(tStack *stack);

/**
 * @brief Kontrola, zda je zásobník prázdný
 * @param stack Ukazatel na zásobník
 * @return true, pokud je zásobník prázdný, jinak false
 */
bool stack_empty(tStack *stack);

#endif // ISTACK_H