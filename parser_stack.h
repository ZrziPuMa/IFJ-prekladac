/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file parser_stack.h
 * @brief Interní rozhraní pro zásobník parseru
 * @author František Bilavčík (<xbilavf00>)
 *
 */

#ifndef PARSER_STACK_H
#define PARSER_STACK_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    unsigned char *data; ///< prvky uložené těsně za sebou, každý má elem_size bajtů
    size_t elem_size;    ///< velikost jednoho prvku v B
    size_t count;        ///< počet prvků; dno je index 0, vrchol index count - 1
    size_t cap;          ///< kolik prvků se vejde do pole, než se musí zvětšit
} ParserStack;

/**
 * @brief Inicializuje zásobník, nastaví elem_size, count = 0, cap = 0, data = NULL
 * @param s ukazatel na zásobník
 * @param elem_size velikost jednoho prvku v B
 * @return true = úspěch, false => elem_size = 0
 */
bool pstack_init(ParserStack *s, size_t elem_size);

/**
 * @brief Umístění prvku na vrchol zásobníku, zvětší kapacitu, pokud je potřeba. Pokud selže
 * alokace, vrací se false  a je to error 99.
 * @param s ukazatel na zásobník
 * @param elem ukazatel na prvek, který se má uložit
 * @return true = úspěch, false = selhání alokace
 */
bool pstack_push(ParserStack *s, const void *elem); // false = selhala alokace -> 99

/**
 * @brief Vrací ukazatel na vrchol zásobníku, NULL = prázdný. Ukazatel platí do doby, než se
 * zásobník změní (push/pop/insert/free)
 * @param s ukazatel na zásobník
 * @return ukazatel na vrchol zásobníku, NULL = prázdný
 */
void *pstack_top(ParserStack *s);

/**
 * @brief Odstraní vrchol zásobníku, pokud je prázdný vrací false
 * @param s ukazatel na zásobník
 * @param out ukazatel na prvek, do kterého se uloží odstraněný prvek. Smí být NULL, pokud se nechce
 * vracet odstraněný prvek.
 * @return true = úspěch, false = prázdný zásobník
 */
bool pstack_pop(ParserStack *s, void *out);

/**
 * @brief Uvolní zásobník, uvolní data, nastaví count = 0, cap = 0, data = NULL. Poznámka:
 * neuvolnuje to, na co prvky ukazují.
 * @param s ukazatel na zásobník
 */
void pstack_free(ParserStack *s);

/*=======================
 *  Pomocné funkce nad rámec klasického zásobníku pro precedenční SA
 * ======================
 */

/**
 * @brief Vrací ukazatel na prvek na indexu i, 0 = dno, count - 1 = vrchol. NULL = prázdný zásobník
 * nebo i >= count. Ukazatel platí do doby, než se zásobník změní (push/pop/insert/free)
 * @param s ukazatel na zásobník
 * @param i index prvku, 0 = dno, count - 1 = vrchol
 * @return ukazatel na prvek na indexu i, NULL = prázdný zásobník nebo i >= count
 */
void *pstack_at(ParserStack *s, size_t i); // potřeba pro zarážku ~

/**
 * @brief Vloží prvek na index i, prvky od i posune výš. Pokud selže alokace, vrací se false  a je
 * to error 99.
 * @param s ukazatel na zásobník
 * @param i index, kam se má vložit prvek, 0 = dno, count = vrchol + 1
 * @param elem ukazatel na prvek, který se má vložit
 * @return true = úspěch, false = selhání alokace nebo i > count
 */
bool pstack_insert(ParserStack *s, size_t i, const void *elem); // prvky od i posune výš

#endif // PARSER_STACK_H