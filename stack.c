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
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na zásobník.");
        return;
    }
    stack->top = NULL;
}

void symtable_push_scope(tStack *stack) {
    if (!stack) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu nebo zásobník.");
        return;
    }

    tStackElem *new_scope = (tStackElem *)malloc(sizeof(tStackElem));
    if (new_scope == NULL) {
        print_error(ERR_INTERNAL, -1, "Chyba alokace paměti pro nový uzel tabulky symbolů.");
        return;
    }

    new_scope->scope = NULL;
    new_scope->next = stack->top;
    stack->top = new_scope;
}

void symtable_pop_scope(tStack *stack) {
    if (stack == NULL) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na zásobník.");
        return;
    }

    if (stack->top) {
        tStackElem *top_scope = stack->top;
        stack->top = top_scope->next;

        symtable_dispose(&(top_scope->scope));
        free(top_scope);
    }
}

void stack_insert(tStack *stack, char *key, tBtNodeData data){
    if (!stack || !stack->top) return;
    symtable_insert(&(stack->top->scope), key, data);
    realign_tree(&(stack->top->scope));
}

void stack_search_symbol(tStack *stack, char *key, tBtNode **location){
    if (!stack){
        *location = NULL;
        return;
    }

    tStackElem *current = stack->top;
    while (current != NULL){
        symtable_search(&(current->scope), key, location);
        if (*location != NULL) return;
        current = current->next;
    }
    *location = NULL;
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
