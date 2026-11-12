/**
 * projekt: Implementace překladače imperativního jazyka IFJ26
 * @file parser.c
 * @brief Implementace syntaktického analyzátoru (parser) (Top-down, rekurzivní sestup)
 * @author František Bilavčík (<xbilavf00>)
 */

#include "parser.h"

#include "iparser.h"

#include <string.h>

void init_parser_state(ParserState *pstate) {
    // Inicializuje stav parseru a vynuluje ho
    *pstate = (ParserState){0};
    // Nastaví error_code na ERR_OK
    pstate->error_code = ERR_OK;
}

void free_parser_state(ParserState *pstate) {
    // Uvolní řetězce tokenů a vynuluje ahead_count
    token_free(&pstate->current_token);
    for (int i = 0; i < pstate->ahead_count; i++) {
        token_free(&pstate->ahead[i]);
    }
    pstate->ahead_count = 0;
}

void next_token(ParserState *pstate) {
    if (pstate->error_code != ERR_OK) // po chybě se token už nemění
        return;

    // Uvolní starý token
    token_free(&pstate->current_token);

    // Pokud jsou tokeny v ahead, vezme je odtamtud, jinak zavolá lexer
    if (pstate->ahead_count > 0) {
        pstate->current_token = pstate->ahead[0];
        // posune všechny tokeny v ahead doleva a sníží ahead_count
        for (int i = 1; i < pstate->ahead_count; i++)
            pstate->ahead[i - 1] = pstate->ahead[i];
        pstate->ahead_count--;
        // vymaže poslední token v ahead, aby se nevolal token_free na uvolněný token
        pstate->ahead[pstate->ahead_count] = (tToken){0};
    } else {
        // zavolá lexer
        pstate->current_token = give_token();
    }

    // Pokud je token typu TOK_ERR, nastaví pstate->error_code na val.i_val
    if (pstate->current_token.type == TOK_ERR)
        pstate->error_code = (ErrorCode)pstate->current_token.val.i_val;
}

void fill_ahead(ParserState *pstate) {
    // Naplní ahead, na 2 tokeny. Volá ji jen parse_expr na LPAR.
    while (pstate->ahead_count < 2) {

        const tToken *last;
        // Pokud je ahead_count > 0, vezme poslední token v ahead, jinak current_token
        if (pstate->ahead_count > 0)
            last = &pstate->ahead[pstate->ahead_count - 1]; // poslední token v bufferu
        // Jinak current_token
        else
            last = &pstate->current_token;
        // Pokud je poslední token TOK_ERR nebo TOK_EOF, nevolá lexer
        if (last->type == TOK_ERR || last->type == TOK_EOF)
            return;
        // Zavolá lexer a uloží token do ahead
        tToken t = give_token();
        pstate->ahead[pstate->ahead_count] = t;
        pstate->ahead_count++;
    }
}

bool expect_token(ParserState *pstate, tTokenType type) {
    // Očekává token typu type, pokud je jiný, nahlásí chybu 2. Pokud je správný, dá next_token()
    if (pstate->current_token.type != type) {
        syntax_error(pstate, "neočekávaný token");
        return false;
    }
    next_token(pstate);
    return true;
}

char *take_string(ParserState *pstate) {
    // Vezme pstate->current_token.val.str a nastaví ho na null
    char *str = pstate->current_token.val.str;
    pstate->current_token.val.str = NULL;
    return str;
}

bool is_id_text(const ParserState *pstate, const char *text) {
    const tToken *tok = &pstate->current_token;

    if (tok->type != TOK_ID)
        // není identifikátor
        return false;
    if (tok->val.str == NULL)
        // řetězec už převzala take_string
        return false;

    // strcmp vrací 0, když se texty shodují
    return strcmp(tok->val.str, text) == 0;
}

void syntax_error(ParserState *pstate, const char *msg) {
    // Chyba 2: error_report a pstate->error_code = 2, jen když pstate->error_code == 0. (tedy
    // nenastala lexikalni ani systemová (interni) chyba)
    if (pstate->error_code == ERR_OK) {
        print_error(.code = ERR_PARSE, .line = pstate->current_token.line, .msg = msg);
        pstate->error_code = ERR_PARSE;
    }
}

void out_of_memory(ParserState *pstate) {
    // Chyba 99: error_report a pstate->error_code = 99, jen když pstate->error_code == 0.
    if (pstate->error_code == ERR_OK) {
        print_error(.code = ERR_INTERNAL, .line = -1, .msg = "nedostatek paměti");
        pstate->error_code = ERR_INTERNAL;
    }
}
