/**
 * Implemetnace lexikálního analyzátoru pro jazyk IFJ26
 * @file lexer.h
 * @brief Hlavičkový soubor lexikálního analyzátoru
 * @author Daniela Sladká, xsladkd00
 */

#ifndef LEXER_H
#define LEXER_H

#include "token.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Datový typ pro prvek zásobníku
 */
typedef struct stack_elem {
    int count;
    struct stack_elem *next; // Ukazatel na další prvek zásobníku
} tLStackElem;

/**
 * @brief Datový typ pro zásobník lexeru
 */
typedef struct LStack {
    tLStackElem *top;
} tLStack;

/**
 * @brief Stroktura tokenu ve frontě
 */
typedef struct FIFO_Elem {
    tToken *token;
    struct FIFO_Elem *next;
} tFIFO_Elem;

/**
 * @brief Struktura pro frontu tokenů
 */
typedef struct FIFO {
    tFIFO_Elem *first;
} tFIFO;

/**
 * @brief Inicializace zásobníku
 * @param stack Ukazatel na zásobník, který se má inicializovat
 */
void lstack_init(tLStack **stack);

/**
 * @brief Vložení nového odsazení nebo zanoření závorek
 * @param count Aktuální počet odsazení
 * @param stack Ukazatel na zásobník, kam se vloží
 */
void lstack_push(int count, tLStack *stack);

/**
 * @brief Odebrání posledního  odsazení nebo zanoření
 * @param stack Ukazatel na zásobník, ze kterého se odebere
 */
void lstack_pop(tLStack *stack);

/**
 * @brief Získání vrcholu zásobníku
 * @param stack Ukazatel na zásobník
 * @return Poslední velikost odsazení, -1 při chybě
 */
int lstack_top(tLStack *stack);

/**
 * @brief Kontrola, zda je zásobník prázdný
 * @param stack Ukazatel na zásobník
 * @return true, pokud je zásobník prázdný, jinak false
 */
bool lstack_empty(tLStack *stack);

/**
 * @brief Inicializace fronty tokenů
 * @param queue Ukazatel na inicializovanou frontu
 * @return true, pokud je zásobník prázdný, jinak false
 */
void fifo_init(tFIFO **queue);

/**
 * @brief Vložení prvku na konec fronty
 * @param token Token vkládaný na konec
 * @param first Prvek, za který se má vkládat
 */
void fifo_add(tToken *token, tFIFO_Elem **first);

/**
 * @brief Získání prvního prvku ze fronty
 * @param queue Fronta, jejíž prvek se vrací
 * @return Token na začátku fronty
 */
tToken fifo_get(tFIFO *queue);

/**
 * @brief Uvolnění paměti fronty
 * @param queue Fronta, která se uvolní
 */
void fifo_clean(tFIFO *queue);

/**
 * @brief Uvolnění paměti prvku fronty
 * @param elem Prvek, který se uvolní
 */
void clean_elem(tFIFO_Elem *elem);

#endif /*LEXER_H*/
