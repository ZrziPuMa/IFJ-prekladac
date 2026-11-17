/**
 * Implemetnace lexikálního analyzátoru pro jazyk IFJ26
 * @file lexer.h
 * @brief Hlavičkový soubor lexikálního analyzátoru
 * @author Daniela Sladká, xsladkd00
 */

#ifndef LEXER_H
#define LEXER_H

// #include "ierror.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Výčet stavů FSM
 */
typedef enum {
    state_start,
    state_plus,
    state_concatenation,
    state_pipe,
    state_or,
    state_ampersand,
    state_and,
    state_slash,
    state_com_line,
    state_com_block,
    state_com_block_2,
    state_com_end,
    state_underscore,
    state_ID,

    state_string_check,
    state_empty_str,
    state_chain_end,
    state_string_end,
    state_string,
    state_string_escape,

    state_interpolateds,
    state_interp_ID,

    state_dot,

    state_exclam,
    // state_s,

    state_zero,
    state_int,
    state_decimal,
    state_exponent,
    state_exp_d,

    state_greater_than,
    state_greater_equal,
    state_lesser_than,
    state_lesser_equal,
    state_left_arrow,

    state_at,
    state_main1,
    state_main2,
    state_main3,

    state_not,
    state_not_equal,
    state_equal,
    state_is_equal,
    state_right_arrow,

    state_eol
} sState;

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
 * @brief Inicializace zásobníku
 * @param stack Ukazatel na zásobník, který se má inicializovat
 */
void lstack_init(tLStack *stack);

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

#endif /*LEXER_H*/