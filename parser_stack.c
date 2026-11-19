/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file parser_stack.c
 * @brief Implementace funkcí pro práci se zásobníkem parseru
 * @author František Bilavčík (<xbilavf00>)
 *
 */

#include "parser_stack.h"

#include <stdlib.h>
#include <string.h>

// Pomocná funkce pro vkládání//
/**
 * @brief Zvětší kapacitu zásobníku, pokud je potřeba. Pokud selže alokace, vrací se false  a je to
 * error 99.
 * @param s ukazatel na zásobník
 * @return true = úspěch, false = selhání alokace
 */
static bool pstack_grow(ParserStack *s) {
    if (s->count < s->cap)
        return true;
    size_t new_cap = (s->cap == 0) ? 1 : s->cap * 2;
    unsigned char *new_data = realloc(s->data, new_cap * s->elem_size);
    if (!new_data)
        return false; // selhala alokace
    s->data = new_data;
    s->cap = new_cap;
    return true;
}

bool pstack_init(ParserStack *s, size_t elem_size) {
    if (elem_size == 0)
        return false;
    s->data = NULL;
    s->elem_size = elem_size;
    s->count = 0;
    s->cap = 0;
    return true;
}

bool pstack_push(ParserStack *s, const void *elem) {
    if (!pstack_grow(s))
        return false;
    memcpy(s->data + s->count * s->elem_size, elem, s->elem_size);
    s->count++;
    return true;
}

void *pstack_top(ParserStack *s) {
    if (s->count == 0)
        return NULL;
    return s->data + (s->count - 1) * s->elem_size;
}

bool pstack_pop(ParserStack *s, void *out) {
    if (s->count == 0)
        return false;
    s->count--;
    if (out == NULL)
        return true;
    memcpy(out, s->data + s->count * s->elem_size, s->elem_size);
    return true;
}

void pstack_free(ParserStack *s) {
    free(s->data);
    s->data = NULL;
    s->count = 0;
    s->cap = 0;
}

void *pstack_at(ParserStack *s, size_t i) {
    if (i >= s->count)
        return NULL;
    return s->data + i * s->elem_size;
}

bool pstack_insert(ParserStack *s, size_t i, const void *elem) {
    if (i > s->count)
        return false;
    if (!pstack_grow(s))
        return false;
    // Posunutí prvků výš od indexu i o jeden prvek
    memmove(s->data + (i + 1) * s->elem_size, s->data + i * s->elem_size,
            (s->count - i) * s->elem_size);
    memcpy(s->data + i * s->elem_size, elem, s->elem_size);
    s->count++;
    return true;
}