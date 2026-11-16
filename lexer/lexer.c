/**
 * Implemetnace lexikálního analyzátoru pro jazyk IFJ26
 * @file lexer.c
 * @brief Zdrojový soubor lexikálního analyzátoru
 * @author Daniela Sladká, xsladkd00
 */

// #include "ierror.h"
#include "lexer.h"

#include "ilexer.h"
#include "token.h"

FILE *file;
tLStack *indent_stack;

void lexer_init(FILE *f) {
    file = f;
    lstack_init(indent_stack);
}

void lstack_init(tLStack *stack) {
    if (!stack) {
        // print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na zásobník.");
        return;
    }
    stack = (tLStack *)malloc(sizeof(tLStack));
    if (stack == NULL) {
        // print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na zásobník.");
        return;
    }
    stack->top = NULL;
}

void lstack_push(tLStackElem *new, tLStack *stack) {
    if (!stack) {
        // print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu nebo zásobník.");
        return;
    }

    tLStackElem *new_scope = (tLStackElem *)malloc(sizeof(tLStackElem));
    if (new_scope == NULL) {
        // print_error(ERR_INTERNAL, -1, "Chyba alokace paměti pro nový uzel tabulky symbolů.");
        return;
    }

    new_scope->count = new->count;
    new_scope->next = stack->top;
    stack->top = new_scope;
}

void lstack_pop(tLStack *stack) {
    if (stack == NULL) {
        // print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na zásobník.");
        return;
    }

    if (stack->top) {
        tLStackElem *top_scope = stack->top;
        stack->top = top_scope->next;

        free(top_scope);
    }
}

tLStackElem *lstack_top(tLStack *stack) {
    if (stack == NULL) {
        return NULL;
    }
    return stack->top;
}

bool lstack_empty(tLStack *stack) {
    if (stack == NULL) {
        return true;
    }
    return stack->top == NULL;
}