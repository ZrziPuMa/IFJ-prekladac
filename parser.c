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

ASTNode *parse_arguments(ParserState *pstate) {
    // (), prázdné argumenty
    if (pstate->current_token.type == TOK_RPAR)
        return NULL;
    ASTNode *head = NULL, *tail = NULL;
    while (true) {
        ASTNode *item = parse_expr(pstate);
        if (item == NULL)
            goto fail;
        // Zřetězení přes next
        if (tail)
            tail->next = item;
        else
            head = item;
        tail = item;

        if (pstate->current_token.type != TOK_COMMA)
            break;
        next_token(pstate);
        // koncová čárka
        if (pstate->current_token.type == TOK_RPAR) {
            if (!pstate->current_token.newline_before) {
                syntax_error(pstate, "koncová čárka jen před ) na novém řádku");
                goto fail;
            }
            break;
        }
    }
    return head;
fail:
    ast_free(head);
    return NULL;
}

ASTNode *parse_generator_opt(ParserState *pstate, ASTNode *start_expr) {
    ASTNode *end = NULL;
    ASTNode *step = NULL;
    bool is_until;
    int line = start_expr->line;

    // TO nebo UNTIL, nemusí být ani jeden
    switch (pstate->current_token.type) {
        case TOK_TO:
            is_until = false;
            break;
        case TOK_UNTIL:
            is_until = true;
            break;
        default:
            return start_expr;
    }
    next_token(pstate);

    // <op_expr> je povinný
    end = parse_op_expr(pstate);
    if (end == NULL)
        goto fail;

    // Kdyby bylo TO, muže se použít i kroky BY
    if (!is_until && pstate->current_token.type == TOK_BY) {
        next_token(pstate);
        step = parse_op_expr(pstate);
        if (step == NULL)
            goto fail;
    }
    // Vytvoříme nový uzel pro range
    ASTNode *range = ast_create_range(start_expr, end, step, is_until, line);
    if (range == NULL) {
        out_of_memory(pstate);
        goto fail;
    }
    return range;

fail:
    ast_free(start_expr);
    ast_free(end);
    ast_free(step);
    return NULL;
}

ASTNode *parse_after_parenth(ParserState *pstate) {
    int line = pstate->current_token.line;

    // UNIT
    if (pstate->current_token.type == TOK_RPAR) {
        next_token(pstate);
        ASTNode *unit = ast_create_unit_lit(line);
        if (unit == NULL) {
            out_of_memory(pstate);
            return NULL;
        }
        return unit;
    }

    // EXPRESSION
    ASTNode *expr = parse_expr(pstate);
    if (expr == NULL)
        return NULL;
    // Nůže byt generator pro cyklus
    expr = parse_generator_opt(pstate, expr);
    if (expr == NULL)
        return NULL;
    if (!expect_token(pstate, TOK_RPAR)) {
        ast_free(expr);
        return NULL;
    }
    return expr;
}

// (AI) prompt: vytvoř mi pomocnou funkci pro Interpolované řetězce s cílem převodu expr na řetězce.
// Obalí výraz voláním toStr(expr); převezme expr, při chybě ho uvolní.
static ASTNode *wrap_to_str(ParserState *pstate, ASTNode *expr) {
    int line = expr->line;
    ASTNode *fn = ast_create_identifier("toStr", line);
    ASTNode *args = NULL;
    if (fn != NULL)
        args = ast_create_arg_list(expr, line);
    if (args == NULL) {
        ast_free(fn);
        ast_free(expr);
        out_of_memory(pstate);
        return NULL;
    }
    ASTNode *call = ast_create_func_call(fn, args, line);
    if (call == NULL) {
        ast_free(fn);
        ast_free(args);
        out_of_memory(pstate);
        return NULL;
    }
    return call;
}

ASTNode *parse_interpol(ParserState *pstate) {
    int line = pstate->current_token.line;
    ASTNode *result = NULL;
    ASTNode *part = NULL;

    if (!expect_token(pstate, TOK_INTERP_BEGIN))
        return NULL;

    while (pstate->current_token.type != TOK_INTERP_END) {
        switch (pstate->current_token.type) {
            // Stringová čast
            case TOK_INTERP_STRING_PART: {
                if (strcmp(pstate->current_token.val.str, "") == 0) {
                    next_token(pstate);
                    continue;
                }
                part = ast_create_string_lit(pstate->current_token.val.str, line);
                if (part == NULL) {
                    out_of_memory(pstate);
                    goto fail;
                }
                next_token(pstate);
                break;
            }
            // Výrazová čast
            case TOK_INTERP_EXPR_BEGIN: {
                next_token(pstate);
                ASTNode *expr = parse_expr(pstate);
                if (expr == NULL)
                    goto fail;
                if (!expect_token(pstate, TOK_INTERP_EXPR_END)) {
                    ast_free(expr);
                    goto fail;
                }
                // obalíme výraz voláním toStr(expr)
                part = wrap_to_str(pstate, expr);
                if (part == NULL)
                    goto fail;
                break;
            }
            default:
                syntax_error(pstate, "neočekávaný token v interpolaci");
                goto fail;
        }

        if (result == NULL)
            result = part;
        // Spojení přes operátor + (konkatenace)
        else {
            ASTNode *concat = ast_create_binop(TOK_PLUS, result, part, line);
            if (concat == NULL) {
                out_of_memory(pstate);
                goto fail;
            }
            result = concat;
        }
        part = NULL;
    }
    next_token(pstate);
    if (result == NULL) {
        // prázdný interpolovaný řetězec
        result = ast_create_string_lit("", line);
        if (result == NULL) {
            out_of_memory(pstate);
            goto fail;
        }
    }
    return result;

fail:
    ast_free(result);
    ast_free(part);
    return NULL;
}

ASTNode *parse_for(ParserState *pstate) {
    ASTNode *iter_var = NULL;
    ASTNode *source = NULL;
    ASTNode *guards = NULL;
    ASTNode *guards_tail = NULL;
    ASTNode *body = NULL;
    int line = pstate->current_token.line;

    if (!expect_token(pstate, TOK_FOR))
        goto fail;

    // Zjištění, zda se jedná o stavbu FOR s víceřádkovou podmínkou
    bool multiline = (pstate->current_token.type == TOK_NEWLINE);
    if (multiline) {
        next_token(pstate);
        if (!expect_token(pstate, TOK_INDENT))
            goto fail;
    }

    if (pstate->current_token.type != TOK_ID) {
        syntax_error(pstate, "očekáván identifikátor");
        goto fail;
    }
    // iterační proměnná
    iter_var = ast_create_var_def(pstate->current_token.val.str, false, NULL, NULL, line);
    if (iter_var == NULL) {
        out_of_memory(pstate);
        goto fail;
    }

    // Zpracování přiřazení iterátoru a zdroje
    next_token(pstate);
    if (!expect_token(pstate, TOK_LEFT_ARROW)) {
        goto fail;
    }
    source = parse_op_expr(pstate);
    if (source == NULL)
        goto fail;

    source = parse_generator_opt(pstate, source);
    if (source == NULL)
        goto fail;

    // Guards
    while (pstate->current_token.type == TOK_IF) {
        next_token(pstate);
        ASTNode *guard = parse_op_expr(pstate);
        if (guard == NULL)
            goto fail;

        if (guards_tail)
            guards_tail->next = guard;
        else
            guards = guard;
        guards_tail = guard;
    }

    // Část, která je, pokud je podmínka víceřádková
    if (multiline) {
        if (!expect_token(pstate, TOK_NEWLINE))
            goto fail;
        while (pstate->current_token.type == TOK_IF) {
            next_token(pstate);
            ASTNode *guard = parse_op_expr(pstate);
            if (guard == NULL)
                goto fail;

            if (guards_tail)
                guards_tail->next = guard;
            else
                guards = guard;
            guards_tail = guard;
            if (!expect_token(pstate, TOK_NEWLINE))
                goto fail;
        }
        if (!expect_token(pstate, TOK_DEDENT))
            goto fail;
    }

    // Část DO
    if (!expect_token(pstate, TOK_DO))
        goto fail;

    // Tělo cyklu
    body = parse_branch(pstate);
    if (body == NULL)
        goto fail;
    ASTNode *node = ast_create_for(iter_var, source, guards, body, line);
    if (node == NULL) {
        out_of_memory(pstate);
        goto fail;
    }
    return node;

fail:
    ast_free(iter_var);
    ast_free(source);
    ast_free(guards);
    ast_free(body);
    return NULL;
}

ASTNode *parse_if(ParserState *pstate) {
    ASTNode *cond = NULL;
    ASTNode *then_branch = NULL;
    ASTNode *else_branch = NULL;
    int line = pstate->current_token.line;

    // Parse IF
    if (!expect_token(pstate, TOK_IF))
        goto fail;
    cond = parse_op_expr(pstate);
    if (cond == NULL)
        goto fail;
    if (!expect_token(pstate, TOK_THEN))
        goto fail;
    then_branch = parse_branch(pstate);
    if (then_branch == NULL)
        goto fail;

    // ELSE je volitelný
    if (pstate->current_token.type == TOK_ELSE) {
        next_token(pstate);
        else_branch = parse_branch(pstate);
        if (else_branch == NULL)
            goto fail;
    }

    // Vytvoření AST uzlu
    ASTNode *node = ast_create_if_expr(cond, then_branch, else_branch, line);
    if (node == NULL) {
        out_of_memory(pstate);
        goto fail;
    }
    return node;

fail:
    ast_free(cond);
    ast_free(then_branch);
    ast_free(else_branch);
    return NULL;
}

ASTNode *parse_branch(ParserState *pstate) {
    int line = pstate->current_token.line;

    // Newline vede na parse block
    if (pstate->current_token.type == TOK_NEWLINE)
        return parse_block(pstate);

    // Jinak to muže byt line expression
    ASTNode *expr = parse_line_expr(pstate);
    if (expr == NULL)
        return NULL;
    ASTNode *block = ast_wrap_block(expr, line);
    if (block == NULL) {
        ast_free(expr);
        out_of_memory(pstate);
        return NULL;
    }
    return block;
}

ASTNode *parse_block(ParserState *pstate) {
    int line = pstate->current_token.line;
    ASTNode *head = NULL;

    if (!expect_token(pstate, TOK_NEWLINE))
        return NULL;
    if (!expect_token(pstate, TOK_INDENT))
        return NULL;

    ASTNode *tail = NULL;
    do {
        ASTNode *statement = parse_statement(pstate);
        if (statement == NULL)
            goto fail;
        if (head == NULL)
            head = statement;
        else
            tail->next = statement;
        tail = statement;

        if (pstate->current_token.type == TOK_SEMICOLON) {
            next_token(pstate);
            if (pstate->current_token.type == TOK_NEWLINE) {
                next_token(pstate);
            }
        } else if (pstate->current_token.type == TOK_NEWLINE) {
            next_token(pstate);
        } else {
            syntax_error(pstate, "očekáván ; nebo nový řádek");
            goto fail;
        }
    } while (pstate->current_token.type != TOK_DEDENT);
    if (!expect_token(pstate, TOK_DEDENT)) {
        goto fail;
    }

    // Vytvoří AST_BLOCK uzel
    ASTNode *block = ast_create_block(head, line);
    if (block == NULL) {
        out_of_memory(pstate);
        goto fail;
    }
    return block;

fail:
    ast_free(head);
    return NULL;
}

ASTNode *parse_statement(ParserState *pstate) {
    switch (pstate->current_token.type) {
        case TOK_WHILE:
            return parse_while(pstate);
        case TOK_FOR:
            return parse_for(pstate);
        case TOK_VAL:
            return parse_val_var_def(pstate, false);
        case TOK_VAR:
            return parse_val_var_def(pstate, true);
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_NOT:
            syntax_error(pstate, "Statement nemuže začínat unárním operátorem");
            return NULL;
        default:
            return parse_line_expr(pstate);
    }
}

ASTNode *parse_line_expr(ParserState *pstate) {
    if (pstate->current_token.type == TOK_RETURN)
        return parse_return(pstate);

    ASTNode *expr = parse_expr(pstate);
    if (expr == NULL)
        return NULL;

    if (pstate->current_token.type != TOK_ASSIGN)
        return expr;

    if (expr->type != AST_IDENTIFIER) {
        syntax_error(pstate, "Nalevo od = smí být jen identifikátor");
        ast_free(expr);
        return NULL;
    }
    int line = expr->line;
    next_token(pstate);

    ASTNode *body = parse_body(pstate);
    if (body == NULL) {
        ast_free(expr);
        return NULL;
    }
    ASTNode *assign = ast_create_assignment(expr->data.identifier.name, body, line);
    if (assign == NULL) {
        ast_free(expr);
        ast_free(body);
        out_of_memory(pstate);
        return NULL;
    }
    ast_free(expr);

    return assign;
}

// (AI) prompt: Napiš mi pomocnou funkci static bool is_expr_start(tTokenType t).
// Zjistí, zda tokenem t může začínat výraz, tj. zda t patří do FIRST(<expr>).
static bool is_expr_start(tTokenType t) {
    switch (t) {
        case TOK_ID:  // identifikátor, volání funkce
        case TOK_INT: // literály
        case TOK_DOUBLE:
        case TOK_STRING:
        case TOK_BOOL:
        case TOK_LIST:         // List(...), List.range
        case TOK_LPAR:         // závorka, () a lambda
        case TOK_INTERP_BEGIN: // interpolovaný řetězec
        case TOK_PLUS:         // unární operátory
        case TOK_MINUS:
        case TOK_NOT:
        case TOK_IF: // if jako výraz (42.1)
            return true;
        default:
            return false;
    }
}

ASTNode *parse_return(ParserState *pstate) {
    int line = pstate->current_token.line;
    next_token(pstate);
    ASTNode *value = NULL;
    if (is_expr_start(pstate->current_token.type)) {
        value = parse_expr(pstate);
        if (value == NULL)
            return NULL;
    }

    ASTNode *ret = ast_create_return(value, line);
    if (ret == NULL) {
        ast_free(value);
        out_of_memory(pstate);
        return NULL;
    }
    return ret;
}

ASTNode *parse_expr(ParserState *pstate) {
    if (pstate->current_token.type == TOK_IF)
        return parse_if(pstate);

    // Konflikt, který potřebuje nadhled na dva tokeny navíc, tedy porušení LL1 na LL3
    if (pstate->current_token.type == TOK_LPAR) {
        fill_ahead(pstate);
        if (pstate->ahead_count == 2) {
            tTokenType a = pstate->ahead[0].type;
            tTokenType b = pstate->ahead[1].type;
            if ((a == TOK_ID && b == TOK_COLON) || (a == TOK_RPAR && b == TOK_RIGHT_ARROW)) {
                return parse_lambda(pstate);
            }
        }
    }

    ASTNode *expr = parse_op_expr(pstate);
    if (expr == NULL)
        return NULL;

    if (pstate->current_token.type == TOK_MATCH)
        return parse_match(pstate, expr);

    return expr;
}

ASTNode *parse_body(ParserState *pstate) {
    // blok, nebo expression
    if (pstate->current_token.type == TOK_NEWLINE)
        return parse_block(pstate);
    return parse_expr(pstate);
}

TypeInfo *parse_ret_type(ParserState *pstate) {
    // return
    if (pstate->current_token.type != TOK_COLON)
        return NULL;
    next_token(pstate);
    // return: type
    TypeInfo *type = parse_type(pstate);
    if (type == NULL)
        return NULL;
    return type;
}

TypeInfo *parse_simple_type(ParserState *pstate) {
    TypeInfo *type = NULL;
    switch (pstate->current_token.type) {
        // custom
        case TOK_ID:
            type = type_create_custom(pstate->current_token.val.str);
            if (type == NULL) {
                out_of_memory(pstate);
                return NULL;
            }
            next_token(pstate);
            return type;
            // list
        case TOK_LIST:
            next_token(pstate);
            if (!expect_token(pstate, TOK_LBRACKET)) {
                return NULL;
            }
            TypeInfo *elem_type = parse_type(pstate);
            if (elem_type == NULL)
                return NULL;
            if (!expect_token(pstate, TOK_RBRACKET)) {
                type_info_free(elem_type);
                return NULL;
            }
            type = type_create_list(elem_type);
            if (type == NULL) {
                type_info_free(elem_type);
                out_of_memory(pstate);
                return NULL;
            }
            return type;
        // základní typy
        case TOK_TYPE: {
            const char *name = pstate->current_token.val.str;
            TypeClass type_class;
            if (name == NULL) {
                syntax_error(pstate, "Typ není podporován");
                return NULL;
            }
            if (strcmp(name, "Int") == 0)
                type_class = TYPE_INT;
            else if (strcmp(name, "Double") == 0)
                type_class = TYPE_DOUBLE;
            else if (strcmp(name, "String") == 0)
                type_class = TYPE_STRING;
            else if (strcmp(name, "Unit") == 0)
                type_class = TYPE_UNIT;
            else if (strcmp(name, "Boolean") == 0)
                type_class = TYPE_BOOLEAN;
            else {
                syntax_error(pstate, "Typ není podporován");
                return NULL;
            }
            type = type_create_basic(type_class);
            if (type == NULL) {
                out_of_memory(pstate);
                return NULL;
            }
            next_token(pstate);
            return type;
        }
        default:
            syntax_error(pstate, "očekáván typ");
            return NULL;
    }
}

TypeInfo *parse_type(ParserState *pstate) {
    if (pstate->current_token.type != TOK_LPAR) {
        TypeInfo *type = parse_simple_type(pstate);
        if (type == NULL)
            return NULL;
        return parse_union_rest(pstate, type);
    }

    next_token(pstate);

    TypeInfo *type = parse_type_list(pstate);
    if (type == NULL && pstate->error_code != ERR_OK)
        return NULL;

    if (!expect_token(pstate, TOK_RPAR)) {
        type_info_free(type);
        return NULL;
    }

    if (pstate->current_token.type == TOK_RIGHT_ARROW) {
        next_token(pstate);
        TypeInfo *ret_type = parse_type(pstate);
        if (ret_type == NULL) {
            type_info_free(type);
            return NULL;
        }
        TypeInfo *func_type = type_create_function(type, ret_type);
        if (func_type == NULL) {
            type_info_free(type);
            type_info_free(ret_type);
            out_of_memory(pstate);
            return NULL;
        }
        return func_type;
    }

    if (type == NULL || type->next != NULL) {
        syntax_error(
            pstate,
            "Pokud použijeme více typů v závorkách, musí být následován => pro funkční typ");
        type_info_free(type);
        return NULL;
    }

    return parse_union_rest(pstate, type);
}

TypeInfo *parse_union_rest(ParserState *pstate, TypeInfo *type) {
    if (pstate->current_token.type != TOK_PIPE)
        return type;

    next_token(pstate);

    TypeInfo *next_type = parse_union_member(pstate);
    if (next_type == NULL) {
        type_info_free(type);
        return NULL;
    }

    TypeInfo *union_type = type_create_union(type, next_type);
    if (union_type == NULL) {
        type_info_free(type);
        type_info_free(next_type);
        out_of_memory(pstate);
        return NULL;
    }

    return parse_union_rest(pstate, union_type);
}

TypeInfo *parse_union_member(ParserState *pstate) {
    if (pstate->current_token.type != TOK_LPAR) {
        return parse_simple_type(pstate);
    }
    next_token(pstate);
    TypeInfo *type = parse_type(pstate);
    if (type == NULL)
        return NULL;

    if (!expect_token(pstate, TOK_RPAR)) {
        type_info_free(type);
        return NULL;
    }

    return type;
}

TypeInfo *parse_type_list(ParserState *pstate) {
    TypeInfo *head = NULL;
    TypeInfo *tail = NULL;

    if (pstate->current_token.type == TOK_RPAR)
        return NULL;

    while (true) {
        TypeInfo *type = parse_type(pstate);
        if (type == NULL) {
            type_info_free(head);
            return NULL;
        }

        if (tail) {
            tail->next = type;
        } else {
            head = type;
        }
        tail = type;

        if (pstate->current_token.type != TOK_COMMA)
            break;

        next_token(pstate);
    }
    return head;
}

ASTNode *parse_param_list(ParserState *pstate) {
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    while (pstate->current_token.type == TOK_LPAR) {
        int line = pstate->current_token.line;
        next_token(pstate);
        ASTNode *params = parse_params(pstate);
        if (pstate->error_code != ERR_OK) {
            goto fail;
        }
        if (!expect_token(pstate, TOK_RPAR)) {
            ast_free(params);
            goto fail;
        }

        ASTNode *group = ast_create_param_list(params, line);
        if (group == NULL) {
            ast_free(params);
            out_of_memory(pstate);
            goto fail;
        }

        if (tail) {
            tail->next = group;
        } else {
            head = group;
        }
        tail = group;
    }
    return head;

fail:
    ast_free(head);
    return NULL;
}

/**
 * @brief Parsuje jeden parametr funkce.
 * @param pstate Stav parseru.
 * @return AST uzel pro parametr nebo NULL v případě chyby.
 */
static ASTNode *parse_param(ParserState *pstate) {
    int line = pstate->current_token.line;

    if (pstate->current_token.type != TOK_ID) {
        syntax_error(pstate, "očekáván název parametru");
        return NULL;
    }

    char *name = take_string(pstate);
    next_token(pstate);

    if (!expect_token(pstate, TOK_COLON)) {
        free(name);
        return NULL;
    }

    TypeInfo *type = parse_type(pstate);
    if (type == NULL) {
        free(name);
        return NULL;
    }
    ASTNode *param = ast_create_var_def(name, false, type, NULL, line);

    free(name);

    if (param == NULL) {
        type_info_free(type);
        out_of_memory(pstate);
        return NULL;
    }
    return param;
}

ASTNode *parse_params(ParserState *pstate) {
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    if (pstate->current_token.type == TOK_RPAR)
        return NULL;

    while (true) {
        ASTNode *param = parse_param(pstate);
        if (param == NULL) {
            ast_free(head);
            return NULL;
        }

        if (tail) {
            tail->next = param;
        } else {
            head = param;
        }
        tail = param;

        if (pstate->current_token.type != TOK_COMMA)
            break;

        next_token(pstate);
        if (pstate->current_token.type == TOK_RPAR) {
            if (!pstate->current_token.newline_before) {
                syntax_error(pstate, "koncová čárka jen před ) na novém řádku");
                ast_free(head);
                return NULL;
            }
            break;
        }
    }
    return head;
}

ASTNode *parse_func_def(ParserState *pstate, bool is_main) {
    ASTNode *params = NULL;
    TypeInfo *ret_type = NULL;
    ASTNode *body = NULL;
    char *name = NULL;
    int line = pstate->current_token.line;

    if (!expect_token(pstate, TOK_DEF))
        goto fail;

    if (pstate->current_token.type != TOK_ID) {
        syntax_error(pstate, "očekáván název funkce");
        goto fail;
    }

    name = take_string(pstate);
    next_token(pstate);

    params = parse_param_list(pstate);
    if (pstate->error_code != ERR_OK)
        goto fail;

    ret_type = parse_ret_type(pstate);
    if (pstate->error_code != ERR_OK)
        goto fail;

    if (!expect_token(pstate, TOK_ASSIGN)) {
        goto fail;
    }

    body = parse_body(pstate);
    if (body == NULL)
        goto fail;

    ASTNode *blk = ast_wrap_block(body, line);
    if (blk == NULL) {
        out_of_memory(pstate);
        goto fail;
    }
    body = blk;

    ASTNode *node = ast_create_func_def(name, is_main, params, ret_type, body, line);
    if (node == NULL) {
        out_of_memory(pstate);
        goto fail;
    }

    free(name);
    return node;

fail:
    ast_free(params);
    type_info_free(ret_type);
    ast_free(body);
    free(name);
    return NULL;
}

void parse_prolog(ParserState *pstate) {
    if (pstate->current_token.type != TOK_IMPORT) {
        syntax_error(pstate, "očekáváno import v prologu");
        return;
    }
    next_token(pstate);

    if (pstate->current_token.type != TOK_ID) {
        syntax_error(pstate, "očekáváno ifj26 v prologu");
        return;
    }

    if (!is_id_text(pstate, "ifj26")) {
        syntax_error(pstate, "očekáváno ifj26 v prologu");
        return;
    }
    next_token(pstate);

    if (pstate->current_token.type != TOK_DOT) {
        syntax_error(pstate, "očekáváno . v prologu");
        return;
    }
    next_token(pstate);

    if (pstate->current_token.type != TOK_MUL) {
        syntax_error(pstate, "očekáváno * v prologu");
        return;
    }
    next_token(pstate);

    if (pstate->current_token.type != TOK_NEWLINE) {
        syntax_error(pstate, "očekáván nový řádek po prologu");
        return;
    }
    next_token(pstate);
}

ASTNode *parse_declaration(ParserState *pstate) {
    switch (pstate->current_token.type) {
        case TOK_DEF:
            return parse_func_def(pstate, false);
        case TOK_AT_MAIN: {
            next_token(pstate);
            if (pstate->current_token.type == TOK_NEWLINE) {
                next_token(pstate);
            }
            return parse_func_def(pstate, true);
        }
        case TOK_TYPE_LIT:
            return parse_type_def(pstate);
        case TOK_CASE:
            return parse_case_class(pstate);
        case TOK_EXTENSION:
            return parse_extension(pstate);
        default:
            syntax_error(pstate, "očekávána deklarace funkce, type, case class nebo extension");
            return NULL;
    }
}

ASTNode *parse_type_def(ParserState *pstate) {
    // type již bylo kontrolováno v parse_declaration
    int line = pstate->current_token.line;
    next_token(pstate);

    if (pstate->current_token.type != TOK_ID) {
        syntax_error(pstate, "očekáván název typu");
        return NULL;
    }
    char *name = take_string(pstate);
    next_token(pstate);

    if (!expect_token(pstate, TOK_ASSIGN)) {
        free(name);
        return NULL;
    }

    TypeInfo *type = parse_type(pstate);
    if (type == NULL) {
        free(name);
        return NULL;
    }

    ASTNode *node = ast_create_type_alias(name, type, line);
    if (node == NULL) {
        type_info_free(type);
        free(name);
        out_of_memory(pstate);
        return NULL;
    }

    free(name);
    return node;
}

ASTNode *parse_case_class(ParserState *pstate) {
    // case již bylo kontrolováno v parse_declaration
    int line = pstate->current_token.line;
    next_token(pstate);

    if (!expect_token(pstate, TOK_CLASS)) {
        return NULL;
    }

    if (pstate->current_token.type != TOK_ID) {
        syntax_error(pstate, "očekáván název case class");
        return NULL;
    }
    char *name = take_string(pstate);
    next_token(pstate);

    if (!expect_token(pstate, TOK_LPAR)) {
        free(name);
        return NULL;
    }

    ASTNode *params = parse_params(pstate);
    if (pstate->error_code != ERR_OK) {
        free(name);
        return NULL;
    }

    if (!expect_token(pstate, TOK_RPAR)) {
        ast_free(params);
        free(name);
        return NULL;
    }

    ASTNode *node = ast_create_class_def(name, params, line);
    if (node == NULL) {
        ast_free(params);
        free(name);
        out_of_memory(pstate);
        return NULL;
    }

    free(name);
    return node;
}

ASTNode *parse_extension(ParserState *pstate) {
    // extension již bylo kontrolováno v parse_declaration
    int line = pstate->current_token.line;
    ASTNode *reciever = NULL;
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    next_token(pstate);
    if (!expect_token(pstate, TOK_LPAR)) {
        goto fail;
    }

    reciever = parse_param(pstate);
    if (reciever == NULL) {
        goto fail;
    }

    if (!expect_token(pstate, TOK_RPAR)) {
        goto fail;
    }

    if (pstate->current_token.type == TOK_NEWLINE) {
        next_token(pstate);
        if (!expect_token(pstate, TOK_INDENT)) {
            goto fail;
        }
        do {
            ASTNode *func_def = parse_func_def(pstate, false);
            if (func_def == NULL) {
                goto fail;
            }
            if (tail) {
                tail->next = func_def;
            } else {
                head = func_def;
            }
            tail = func_def;

            if (!expect_token(pstate, TOK_NEWLINE)) {
                goto fail;
            }
        } while (pstate->current_token.type != TOK_DEDENT);
        if (!expect_token(pstate, TOK_DEDENT)) {
            goto fail;
        }

    } else {
        ASTNode *func_def = parse_func_def(pstate, false);
        if (func_def == NULL) {
            goto fail;
        }
        head = func_def;
    }

    ASTNode *node = ast_create_extension_def(reciever, head, line);
    if (node == NULL) {
        out_of_memory(pstate);
        goto fail;
    }
    return node;

fail:
    ast_free(reciever);
    ast_free(head);
    return NULL;
}

ASTNode *parse_lambda(ParserState *pstate) {
    int line = pstate->current_token.line;
    next_token(pstate);

    ASTNode *params = parse_params(pstate);
    if (pstate->error_code != ERR_OK)
        return NULL;

    if (!expect_token(pstate, TOK_RPAR)) {
        ast_free(params);
        return NULL;
    }

    ASTNode *param_list = ast_create_param_list(params, line);
    if (param_list == NULL) {
        ast_free(params);
        out_of_memory(pstate);
        return NULL;
    }

    if (!expect_token(pstate, TOK_RIGHT_ARROW)) {
        ast_free(param_list);
        return NULL;
    }

    ASTNode *body = parse_body(pstate);
    if (body == NULL) {
        ast_free(param_list);
        return NULL;
    }

    ASTNode *blk = ast_wrap_block(body, line);
    if (blk == NULL) {
        ast_free(body);
        ast_free(param_list);
        out_of_memory(pstate);
        return NULL;
    }
    body = blk;

    ASTNode *node = ast_create_lambda(param_list, body, line);
    if (node == NULL) {
        ast_free(body);
        ast_free(param_list);
        out_of_memory(pstate);
        return NULL;
    }
    return node;
}

ASTNode *parse_match(ParserState *pstate, ASTNode *expr) {
    int line = pstate->current_token.line;
    ASTNode *head = NULL;
    ASTNode *tail = NULL;
    next_token(pstate);

    if (!expect_token(pstate, TOK_NEWLINE))
        goto fail;

    if (!expect_token(pstate, TOK_INDENT))
        goto fail;

    do {
        ASTNode *branch = parse_case_branch(pstate);
        if (branch == NULL) {
            goto fail;
        }
        if (tail) {
            tail->next = branch;
        } else {
            head = branch;
        }
        tail = branch;

        if (!expect_token(pstate, TOK_NEWLINE)) {
            goto fail;
        }
    } while (pstate->current_token.type != TOK_DEDENT);
    if (!expect_token(pstate, TOK_DEDENT)) {
        goto fail;
    }

    ASTNode *node = ast_create_match(expr, head, line);
    if (node == NULL) {
        out_of_memory(pstate);
        goto fail;
    }
    return node;

fail:
    ast_free(head);
    ast_free(expr);
    return NULL;
}

ASTNode *parse_case_branch(ParserState *pstate) {
    int line = pstate->current_token.line;
    ASTNode *pattern = NULL;
    ASTNode *body = NULL;
    ASTNode *guard = NULL;

    if (!expect_token(pstate, TOK_CASE)) {
        goto fail;
    }

    pattern = parse_pattern(pstate);
    if (pattern == NULL) {
        goto fail;
    }

    if (pstate->current_token.type == TOK_IF) {
        next_token(pstate);
        guard = parse_op_expr(pstate);
        if (guard == NULL) {
            goto fail;
        }
    }

    if (!expect_token(pstate, TOK_RIGHT_ARROW)) {
        goto fail;
    }

    body = parse_branch(pstate);
    if (body == NULL)
        goto fail;

    ASTNode *node = ast_create_case_branch(pattern, guard, body, line);
    if (node == NULL) {
        out_of_memory(pstate);
        goto fail;
    }
    return node;

fail:
    ast_free(pattern);
    ast_free(body);
    ast_free(guard);
    return NULL;
}

ASTNode *parse_simple_pattern(ParserState *pstate) {
    // TODO!!
}

ASTNode *parse_pattern(ParserState *pstate) {
    // TODO!!
}

ASTNode *parse_sub_patterns(ParserState *pstate) {
    // TODO!!
}

ErrorCode parse_program(ASTNode **root) {
    *root = NULL;

    ParserState pstate;
    init_parser_state(&pstate);

    next_token(&pstate);
    int line = pstate.current_token.line;
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    parse_prolog(&pstate);
    if (pstate.error_code != ERR_OK) {
        goto fail;
    }

    while (pstate.current_token.type != TOK_EOF) {
        ASTNode *decl = parse_declaration(&pstate);
        if (decl == NULL) {
            goto fail;
        }
        if (tail) {
            tail->next = decl;
        } else {
            head = decl;
        }
        tail = decl;

        if (!expect_token(&pstate, TOK_NEWLINE)) {
            goto fail;
        }
    }

    ASTNode *program = ast_create_program(head, line);
    if (program == NULL) {
        out_of_memory(&pstate);
        goto fail;
    }
    *root = program;
    free_parser_state(&pstate);
    return ERR_OK;

fail:
    ast_free(head);
    free_parser_state(&pstate);
    return pstate.error_code;
}
