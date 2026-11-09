/**
 * Implemetnace překladače jazyka IFJ26
 * @file stack.c
 * @brief Implementace zásobníku
 * @author Daniela Sladká, xsladkd00
 */


#include "istack.h"
#include "isymtable.h"
#include "ierror.h"

#include <stdio.h>
#include <stdlib.h>


void stack_init(tStack *stack) {
    if (!stack) {
        //print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na zásobník.");
        return;
    }
    stack->top = NULL;
}

void symtable_push_scope(tBtNode **root, tStack *stack) {
    if (root == NULL || stack == NULL) {
        //print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu nebo zásobník.");
        return;
    }

    tStackElem *new_scope = (tStackElem *)malloc(sizeof(tStackElem));
    if (new_scope == NULL) {
        //print_error(ERR_INTERNAL, -1, "Chyba alokace paměti pro nový uzel tabulky symbolů.");
        return;
    }

    new_scope->next = stack->top;
    new_scope->scope = *root;
    stack->top = new_scope;
}

void symtable_pop_scope(tStack *stack) {
    if (stack == NULL) {
        //print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na zásobník.");
        return;
    }

    if (stack->top) {
        tStackElem *top_scope = stack->top;
        stack->top = top_scope->next;
        free(top_scope);
    }
}

tStackElem *stack_top(tStack *stack) {
    if (stack == NULL) {
        return NULL;
    }
    return stack->top;
}

bool stack_empty(tStack *stack) {
    if (stack == NULL) {
        return true;
    }
    return stack->top == NULL;
}
