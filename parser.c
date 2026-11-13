/**
 * projekt: Implementace překladače imperativního jazyka IFJ26
 * @file parser.c
 * @brief Implementace syntaktického analyzátoru (parser) (Top-down, rekurzivní sestup)
 * @author František Bilavčík (<xbilavf00>)
 */

#include "parser.h"

#include "iparser.h"

#include <stdlib.h>
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

/**
 * @brief Parsuje skupiny argumentů, (args) nebo (args)(args)...
 * @param pstate Stav parseru.
 * @return Seznam skupin argumentů nebo NULL v případě chyby.
 */
static ASTNode *parse_arg_groups(ParserState *pstate) {
    ASTNode *head = NULL, *tail = NULL;
    // očekávaný levé závorky
    while (pstate->current_token.type == TOK_LPAR) {
        int line = pstate->current_token.line;
        next_token(pstate);
        // zparsujeme jedny závorky args
        ASTNode *items = parse_arguments(pstate);
        if (pstate->error_code != ERR_OK)
            goto fail;
        if (!expect_token(pstate, TOK_RPAR)) {
            ast_free(items);
            goto fail;
        }
        ASTNode *group = ast_create_arg_list(items, line);
        if (group == NULL) {
            ast_free(items);
            out_of_memory(pstate);
            goto fail;
        }
        // Přidá skupinu na konec seznamu
        if (tail)
            tail->next = group;
        else
            head = group;
        tail = group;
    }
    return head;
fail:
    ast_free(head);
    return NULL;
}

// Funkce pro parse TOP-DOWN

ASTNode *parse_while(ParserState *pstate) {
    // Inicializace uzlů
    ASTNode *cond = NULL;
    ASTNode *body = NULL;
    int line = pstate->current_token.line;

    if (!expect_token(pstate, TOK_WHILE))
        goto fail;
    cond = parse_op_expr(pstate);
    if (cond == NULL)
        goto fail;
    if (!expect_token(pstate, TOK_DO))
        goto fail;
    body = parse_branch(pstate);
    if (body == NULL)
        goto fail;

    ASTNode *node = ast_create_while(cond, body, line);
    // konstruktor nic nepřevzal
    if (node == NULL) {
        out_of_memory(pstate);
        goto fail;
    }
    return node; // cond a body teď patří uzlu

fail:
    ast_free(cond); // ast_free(NULL) nic nedělá
    ast_free(body);
    return NULL;
}

ASTNode *parse_literal(ParserState *pstate) {
    ASTNode *node = NULL;
    int line = pstate->current_token.line;

    switch (pstate->current_token.type) {
        case TOK_INT:
            node = ast_create_int_lit(pstate->current_token.val.i_val, line);
            break;
        case TOK_DOUBLE:
            node = ast_create_double_lit(pstate->current_token.val.d_val, line);
            break;
        case TOK_STRING:
            // konstruktor si řetězec zkopíruje, originál uvolní next_token
            node = ast_create_string_lit(pstate->current_token.val.str, line);
            break;
        case TOK_BOOL:
            node = ast_create_bool_lit(pstate->current_token.val.i_val != 0, line);
            break;
        default:
            syntax_error(pstate, "očekáván literál");
            return NULL;
    }

    if (node == NULL) {
        out_of_memory(pstate);
        return NULL;
    }
    next_token(pstate);
    return node;
}

ASTNode *parse_val_var_def(ParserState *pstate, bool is_mutable) {
    char *name = NULL;
    TypeInfo *type = NULL;
    ASTNode *init = NULL;
    int line = pstate->current_token.line;

    // Val nebo var už bylo
    next_token(pstate);
    if (pstate->current_token.type != TOK_ID) {
        syntax_error(pstate, "očekáván identifikátor");
        goto fail;
    }
    name = take_string(pstate);
    next_token(pstate);

    // Rekurzivní volání podčástí
    type = parse_ret_type(pstate);
    if (pstate->error_code != ERR_OK)
        goto fail;
    if (!expect_token(pstate, TOK_ASSIGN))
        goto fail;
    init = parse_body(pstate);
    if (init == NULL)
        goto fail;

    // Konstrukce AST uzlu
    ASTNode *node = ast_create_var_def(name, is_mutable, type, init, line);
    if (node == NULL) {
        out_of_memory(pstate);
        goto fail;
    }
    free(name);
    return node;

fail:
    free(name);
    type_info_free(type);
    ast_free(init);
    return NULL;
}

ASTNode *parse_element(ParserState *pstate) {
    int line = pstate->current_token.line;

    switch (pstate->current_token.type) {
        case TOK_ID: {
            ASTNode *node = ast_create_identifier(pstate->current_token.val.str, line);
            if (node == NULL) {
                out_of_memory(pstate);
                return NULL;
            }
            next_token(pstate);
            return node;
        }
        case TOK_INT:
        case TOK_DOUBLE:
        case TOK_STRING:
        case TOK_BOOL:
            return parse_literal(pstate);
        case TOK_LIST: {
            next_token(pstate);
            // LIST(<arguments>)
            if (pstate->current_token.type == TOK_LPAR) {
                next_token(pstate);
                // Kontrola na NULL zde není, NULL je platný
                ASTNode *items = parse_arguments(pstate);
                if (pstate->error_code != ERR_OK)
                    return NULL;
                if (!expect_token(pstate, TOK_RPAR)) {
                    ast_free(items);
                    return NULL;
                }
                ASTNode *node = ast_create_list_literal(items, line);
                if (node == NULL) {
                    ast_free(items);
                    out_of_memory(pstate);
                    return NULL;
                }
                return node;
            }
            // LIST.<id>
            else if (pstate->current_token.type == TOK_DOT) {
                // Po tečce je očekáván ID
                next_token(pstate);
                if (pstate->current_token.type != TOK_ID) {
                    syntax_error(pstate, "za List. je očekáván identifikátor");
                    return NULL;
                }
                // Vytvoření name pro spojení "List." + member
                const char *member = pstate->current_token.val.str;
                size_t len = strlen("List.") + strlen(member) + 1;
                char *name = malloc(len);
                if (name == NULL) {
                    out_of_memory(pstate);
                    return NULL;
                }
                // Spojení "List." + member do name
                snprintf(name, len, "List.%s", member);
                ASTNode *node = ast_create_identifier(name, line);
                free(name); // konstruktor si jméno zkopíroval
                if (node == NULL) {
                    out_of_memory(pstate);
                    return NULL;
                }
                next_token(pstate);
                return node;
            }
            syntax_error(pstate, "za List je očekáváno ( nebo .");
            return NULL;
        }
        case TOK_LPAR:
            next_token(pstate);
            return parse_after_parenth(pstate);
        case TOK_INTERP_BEGIN: // 53.9 <interpol>
            return parse_interpol(pstate);
        default:
            syntax_error(pstate, "očekáván operand");
            return NULL;
    }
}

ASTNode *parse_term(ParserState *pstate) {
    ASTNode *node = parse_element(pstate);
    if (node == NULL)
        return NULL;

    while (true) {
        int line = pstate->current_token.line;

        if (pstate->current_token.type == TOK_DOT) {
            // Za tečkou očekáván identifikátor
            next_token(pstate);
            if (pstate->current_token.type != TOK_ID) {
                syntax_error(pstate, "za . je očekáván identifikátor");
                goto fail;
            }
            // vytvoří část idečka .ID
            ASTNode *member = ast_create_member_access(node, pstate->current_token.val.str, line);

            if (member == NULL) {
                out_of_memory(pstate);
                goto fail;
            }
            node = member;
            next_token(pstate);
            // Pokud je přijdou závorky, pracujeme s argumenty a volání funkce
        } else if (pstate->current_token.type == TOK_LPAR) {
            ASTNode *groups = parse_arg_groups(pstate);
            if (groups == NULL)
                goto fail;
            ASTNode *call = ast_create_func_call(node, groups, line);
            if (call == NULL) {
                ast_free(groups);
                out_of_memory(pstate);
                goto fail;
            }
            node = call;
        } else {
            return node;
        }
    }
fail:
    ast_free(node);
    return NULL;
}
