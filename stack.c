/**
 * Implemetnace překladače jazyka IFJ26
 * @file stack.c
 * @brief Implementace zásobníku
 * @author Daniela Sladká, xsladkd00
 */

#include "ierror.h"
#include "istack.h"
#include "isymtable.h"
#include "symtable.h"

#include <stdio.h>
#include <stdlib.h>

ErrorCode stack_init(tStack *stack) {
    if (!stack) {
        return ERR_INTERNAL;
    }
    stack->top = NULL;
    return ERR_OK;
}

ErrorCode symtable_push_scope(tBtNode **root, tStack *stack) {
    if (!stack || !root) {
        return ERR_INTERNAL;
    }

    tStackElem *new_scope = (tStackElem *)malloc(sizeof(tStackElem));
    if (new_scope == NULL) {
        return ERR_INTERNAL;
    }

    new_scope->scope = *root;
    new_scope->next = stack->top;
    stack->top = new_scope;
    return ERR_OK;
}

ErrorCode symtable_pop_scope(tStack *stack) {
    if (stack == NULL) {
        return ERR_INTERNAL;
    }

    if (stack->top) {
        tStackElem *top_scope = stack->top;
        stack->top = top_scope->next;

        ErrorCode err = symtable_dispose(&(top_scope->scope));
        free(top_scope);
        return err;
    }
    return ERR_OK;
}

ErrorCode stack_insert(tStack *stack, char *key, tBtNodeData data) {
    if (!stack || !stack->top)
        return ERR_INTERNAL;
    return symtable_insert(&(stack->top->scope), key, data);
}

ErrorCode stack_search_symbol(tStack *stack, char *key, tBtNode **location) {
    if (!location)
        return ERR_INTERNAL;
    if (!stack) {
        *location = NULL;
        return ERR_INTERNAL;
    }

    tStackElem *current = stack->top;
    while (current != NULL) {
        ErrorCode err = symtable_search(&(current->scope), key, location);
        if (err != ERR_OK)
            return err;
        if (*location != NULL)
            return ERR_OK;
        current = current->next;
    }
    *location = NULL;
    return ERR_OK;
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
