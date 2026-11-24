/**
 * Implementace překladače imperativního jazyka IFJ26
 * @file tokdump.c
 * @brief Výpis proudu tokenů z lexeru přes token_dump (pro testy lexeru, není součástí překladače)
 *
 * Program čte zdrojový kód ze stdin a každý token vypíše funkcí token_dump z token.c.
 * Končí prvním tokenem EOF nebo ERROR. Návratový kód: 0, nebo kód chyby z TOK_ERROR (1, 99).
 *
 * Překlad: make tokdump
 * Použití: ./tokdump < program.ifj
 *
 * [AI] Claude (Opus 5.5), prompt: „chci napsat nové testy, které pokryjí e2e testy i tok_dump
 * by používaly pro výpis, máme li implementaci v token.c“
 */

#include "ilexer.h"
#include "token.h"

#include <stdio.h>

int main(void) {
    int rc = 0;

    lexer_init(stdin);
    while (1) {
        tToken t = give_token();
        token_dump(&t, stdout);

        const tTokenType type = t.type;
        if (type == TOK_ERROR) {
            rc = (int)t.val.i_val;
        }
        token_free(&t);

        if (type == TOK_EOF || type == TOK_ERROR) {
            break;
        }
    }
    lexer_free();

    return rc;
}
