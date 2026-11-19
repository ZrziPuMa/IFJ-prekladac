/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file expression.c
 * @brief Implementace precedenční SA (Bottom-up)
 * @author František Bilavčík (<xbilavf00>)
 *
 * (AI) Použit Claude Opus 5.5 na převedení tabulky z png do C.
 */

#include "expression.h"

#include "parser.h"

// Převede token na symbol SA. expect_operand = true, když SA čeká operand (začátek výrazu,
// za operátorem); jinak čeká binární operátor nebo konec výrazu.
static PrecedentSymbol token_to_sym(const tToken *tok, bool expect_operand);

// Pro lepši čitelnost tabulky
#define L PREC_REL_LT
#define G PREC_REL_GT
#define X PREC_REL_ERROR
#define OK PREC_REL_OK

// Precedenční tabulka
// (AI) Použit Claude Opus 5.5 na převedení tabulky z png do C.
// prompt: Převeď tabulku na kod v c
// Tabulku jsme vytvořili my. Použili jsme Claude pouze k jejímu přepisu do C
static const PrecedentRelation prec_table[PREC__COUNT][PREC__COUNT] = {
    [PREC_UNARY_OP] = {L, L, G, G, G, G, G, G, G, G, G, G, G, G, G, L, G},
    [PREC_NOT] = {L, L, G, G, G, G, G, G, G, G, G, G, G, G, G, L, G},
    [PREC_MUL] = {L, L, G, G, G, G, G, G, G, G, G, G, G, G, G, L, G},
    [PREC_DIV] = {L, L, G, G, G, G, G, G, G, G, G, G, G, G, G, L, G},
    [PREC_PLUS] = {L, L, L, L, G, G, G, G, G, G, G, G, G, G, G, L, G},
    [PREC_MINUS] = {L, L, L, L, G, G, G, G, G, G, G, G, G, G, G, L, G},
    [PREC_CONCAT] = {L, L, L, L, G, G, G, G, G, G, G, G, G, G, G, L, G},
    [PREC_LT] = {L, L, L, L, L, L, L, X, X, X, X, G, G, G, G, L, G},
    [PREC_LE] = {L, L, L, L, L, L, L, X, X, X, X, G, G, G, G, L, G},
    [PREC_GT] = {L, L, L, L, L, L, L, X, X, X, X, G, G, G, G, L, G},
    [PREC_GE] = {L, L, L, L, L, L, L, X, X, X, X, G, G, G, G, L, G},
    [PREC_EQ] = {L, L, L, L, L, L, L, L, L, L, L, X, X, G, G, L, G},
    [PREC_NEQ] = {L, L, L, L, L, L, L, L, L, L, L, X, X, G, G, L, G},
    [PREC_AND] = {L, L, L, L, L, L, L, L, L, L, L, L, L, G, G, L, G},
    [PREC_OR] = {L, L, L, L, L, L, L, L, L, L, L, L, L, L, G, L, G},
    [PREC_I] = {X, X, G, G, G, G, G, G, G, G, G, G, G, G, G, X, G},
    [PREC_END] = {L, L, L, L, L, L, L, L, L, L, L, L, L, L, L, L, OK},
};

#undef L
#undef G
#undef X
#undef OK

static PrecedentSymbol token_to_sym(const tToken *tok, bool expect_operand) {
    if (expect_operand) {
        switch (tok->type) {
            case TOK_PLUS:
                return PREC_UNARY_OP;
            case TOK_MINUS:
                return PREC_UNARY_OP;
            case TOK_NOT:
                return PREC_NOT;
            case TOK_ID:
            case TOK_INT:
            case TOK_DOUBLE:
            case TOK_STRING:
            case TOK_BOOL:
            case TOK_LIST:
            case TOK_LPAR:
            case TOK_INTERP_BEGIN:
                return PREC_I;
            default:
                return PREC__INVALID;
        }
    } else {
        switch (tok->type) {
            case TOK_PLUS:
                return PREC_PLUS;
            case TOK_MINUS:
                return PREC_MINUS;
            case TOK_MUL:
                return PREC_MUL;
            case TOK_DIV:
                return PREC_DIV;
            case TOK_CONCAT:
                return PREC_CONCAT;
            case TOK_LESSTHEN:
                return PREC_LT;
            case TOK_LESSEQ:
                return PREC_LE;
            case TOK_GREATERTHEN:
                return PREC_GT;
            case TOK_GREATEREQ:
                return PREC_GE;
            case TOK_EQ:
                return PREC_EQ;
            case TOK_NOTEQ:
                return PREC_NEQ;
            case TOK_AND:
                return PREC_AND;
            case TOK_OR:
                return PREC_OR;
            default:
                return PREC_END;
        }
    }
}

// Index nejvyššího terminálu (PI_TERM) na zásobníku; přeskakuje E i ~. $ na dně je vždy terminál.
static size_t top_terminal(ParserStack *st) {
    for (size_t i = st->count; i > 0; i--) {
        PrecedentItem *item = pstack_at(st, i - 1);
        if (item->kind == PI_TERM) {
            return i - 1;
        }
    }
    return 0; // dno zásobníku $
}

// Najde nejvyšší zarážku ~. Index uloží do *out a vrátí true; když ~ na zásobníku není, vrátí
// false.
static bool top_handle(ParserStack *st, size_t *out) {
    for (size_t i = st->count; i > 0; i--) {
        PrecedentItem *item = pstack_at(st, i - 1);
        if (item->kind == PI_HANDLE) {
            *out = i - 1;
            return true;
        }
    }
    return false;
}

// Provede jednu redukci nad nejvyšší ~. false = chyba (už nahlášená přes syntax_error /
// out_of_memory).
/**
 * @brief Provede jednu redukci nad nejvyšší ~. false = chyba (už nahlášená přes syntax_error /
 * out_of_memory)
 * @param pstate Ukazatel na stav parseru
 * @param st Ukazatel na zásobník
 * @return true = úspěch, false = chyba (už nahlášená přes syntax_error / out_of_memory)
 */
static bool reduce(ParserState *pstate, ParserStack *st) {
    // najít první zarážku ~ od vrcholu zásobníku
    size_t handle_index;
    if (!top_handle(st, &handle_index)) {
        syntax_error(pstate, "redukovat nelze, chybí zarážka");
        return false;
    }

    // položky nad zarážkou, max 3
    size_t n = st->count - handle_index - 1;
    PrecedentItem *a = pstack_at(st, handle_index + 1); // NULL, když n < 1
    PrecedentItem *b = pstack_at(st, handle_index + 2); // NULL, když n < 2
    PrecedentItem *c = pstack_at(st, handle_index + 3); // NULL, když n < 3

    // nový uzel ast
    ASTNode *result = NULL;
    int line = 0;

    if (n == 1 && a->kind == PI_TERM && a->sym == PREC_I) {
        // E -> i : uzel z položky i se jen převezme
        result = a->node;
        line = a->line;
    } else if (n == 2 && a->kind == PI_TERM && (a->sym == PREC_UNARY_OP || a->sym == PREC_NOT) &&
               b->kind == PI_NONTERM) {
        // E -> u E  /  E -> ! E
        result = ast_create_unary_op(a->op_token, b->node, a->line);
        line = a->line;

    } else if (n == 3 && a->kind == PI_NONTERM && b->kind == PI_TERM && b->sym >= PREC_MUL &&
               b->sym <= PREC_OR && c->kind == PI_NONTERM) {
        // E -> E op E
        result = ast_create_binop(b->op_token, a->node, c->node, b->line);
        line = b->line;

    } else {
        // Nebylo použito žádné pravidlo a tedy je neplatný výraz.
        syntax_error(pstate, "neplatný výraz");
        return false;
    }

    // není platný astnode, došlo k chybě při jeho vytváření
    if (result == NULL) {
        out_of_memory(pstate);
        return false;
    }

    // Odstraní n + 1 prvků z vrcholu zásobníku (včetně zarážky ~)
    for (size_t k = 0; k < n + 1; k++)
        pstack_pop(st, NULL);

    // Přidá na zásobník nový uzel E (Neterminál)
    PrecedentItem e = {.sym = PREC__INVALID, .kind = PI_NONTERM, .node = result, .line = line};
    // Umístí se Neterminál E na vrchol zásobníku a pokud dojde k chybě, uvolní se ast a nahlásí se
    // chyba.
    if (pstack_push(st, &e) == false) {
        ast_free(result);
        out_of_memory(pstate);
        return false;
    }
    return true;
}

/**
 * @brief Uvolní uzly (ast_free) ve všech položkách zásobníku; samotný zásobník neuvolní.
 * @param st Ukazatel na zásobník
 */
static void free_items(ParserStack *st) {
    for (size_t i = 0; i < st->count; i++) {
        PrecedentItem *item = pstack_at(st, i);

        ast_free(item->node);
        item->node = NULL;
    }
}

ASTNode *parse_op_expr(ParserState *pstate) {
    ASTNode *result = NULL;
    ParserStack st;
    // Chyba v zásadě nemůže nastat, ale pro jistotu
    if (pstack_init(&st, sizeof(PrecedentItem)) == false) {
        out_of_memory(pstate);
        return NULL;
    }

    // Umístění na zásobník zarážku "$" (PREC_END)
    PrecedentItem end = {.sym = PREC_END, .kind = PI_TERM};
    if (pstack_push(&st, &end) == false) {
        out_of_memory(pstate);
        goto fail;
    }

    bool expect_operand = true;

    while (true) {
        // Pokud je error, jde se na fail
        if (pstate->error_code != ERR_OK) {
            goto fail;
        }

        // Získá symbol z aktuálního tokenu
        PrecedentSymbol input = token_to_sym(&pstate->current_token, expect_operand);
        // Pokud je symbol neplatný, jde se na fail
        if (input == PREC__INVALID) {
            syntax_error(pstate, "očekáván operand nebo unární operátor");
            goto fail; // východ 1: chyba
        }

        // Získá index nejvyššího terminálu na zásobníku a jeho symbol
        size_t t = top_terminal(&st);
        PrecedentItem *top = pstack_at(&st, t);
        PrecedentRelation rel = prec_table[top->sym][input];

        // Testování typu relace, (AI) Použil jsem Claude Opus 5.5, abych ověřil precedenční analýzu
        // v relačních precedenčních operátorech.
        switch (rel) {
            // Posun
            case PREC_REL_LT: {
                // Umístění zarážky ~ těsně nad nejvyšší terminál (pod případné E na vrcholu)
                PrecedentItem handle = {.sym = PREC__INVALID, .kind = PI_HANDLE};
                if (!pstack_insert(&st, t + 1, &handle)) {
                    out_of_memory(pstate);
                    goto fail;
                }
                // (AI) část kodu byla převzata od Claude Opus 5.5, když jsem ověřoval chyby při
                // spuštění. Pokud přijde operand, zavolá se parse_term
                if (input == PREC_I) {
                    int line = pstate->current_token.line;
                    ASTNode *node = parse_term(pstate);
                    if (node == NULL)
                        goto fail;
                    // nový prvek a umístění na stack, (AI) prvky item jsou z Claude Opus 5.5, když
                    // jsem ověřoval chyby při spuštění.
                    PrecedentItem item = {
                        .sym = PREC_I, .kind = PI_TERM, .node = node, .line = line};
                    if (pstack_push(&st, &item) == false) {
                        ast_free(node);
                        out_of_memory(pstate);
                        goto fail;
                    }
                    expect_operand = false;
                    // Pokud přijde operátor, zapamatuje se typ tokenu a posune se na další token
                } else {
                    // (AI) část kodu byla převzata od Claude Opus 5.5, když jsem ověřoval chyby při
                    // spuštění.Zde přesněji opět prvky itemu.
                    PrecedentItem item = {.sym = input,
                                          .kind = PI_TERM,
                                          .op_token = pstate->current_token.type,
                                          .line = pstate->current_token.line};
                    if (!pstack_push(&st, &item)) {
                        out_of_memory(pstate);
                        goto fail;
                    }
                    next_token(pstate);
                    expect_operand = true;
                }
            } break;
            // redukce
            case PREC_REL_GT:
                if (reduce(pstate, &st) == false) {
                    goto fail;
                }
                break;
            // $ E a na vstupu $
            case PREC_REL_OK: {
                // na zásobníku musí být přesně $ E
                PrecedentItem *e = pstack_at(&st, 1);
                // pokud na zásobníku není přesně $ E, jde se na fail
                if (st.count != 2 || e->kind != PI_NONTERM) {
                    syntax_error(pstate, "neplatný výraz");
                    goto fail;
                }
                // přesun uzlu AST
                result = e->node;
                e->node = NULL;
                // Uvolnění zásobníku a návrat
                pstack_free(&st);
                return result;
            }
            // Chyba
            case PREC_REL_ERROR:
                syntax_error(pstate, "nepovolená kombinace operátorů");
                goto fail; // východ 1: chyba
        }
    }

fail:
    free_items(&st);
    pstack_free(&st);
    return NULL;
}
