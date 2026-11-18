/**
 * Implemetnace lexikálního analyzátoru pro jazyk IFJ26
 * @file lexer.c
 * @brief Zdrojový soubor lexikálního analyzátoru
 * @author Daniela Sladká, xsladkd00
 */

// #include "ierror.h"
#include "lexer.h"

#include "idynamic_string.h"

FILE *file;
tLStack *indent_stack;
tFIFO *queue;
int line;
int parent_cnt;
int block_com_cnt;
int indent_cnt;

void lexer_init(FILE *f) {
    file = f;
    lstack_init(indent_stack);
    fifo_init(queue);
    parent_cnt = 0;
    line = 1;
    block_com_cnt = 0;
    indent_cnt = 0;
    lstack_push(indent_cnt, indent_stack);
}

tToken give_token() {
    int c;
    sStr str;
    bool com_nl = false;
    if (string_init(&str)) {
        // print_error(ERR_INTERNAL, -1, "Chyba: Chyba inicializace stringu.");
    }

    tToken token;
    token.type = TOK_EMPTY;
    token.state = state_start;
    token.newline_before = false;

    bool completed = false;

    while (!completed) {
        c = getc(file);
        switch (token.state) {
            case state_start:
                switch (c) {
                    case EOF:
                        completed = true;
                        token.type = TOK_EOF;
                        break;

                    case '\n':
                        token.type = TOK_NEWLINE;
                        token.state = state_eol;
                        break;

                    case '@':
                        token.state = state_at;
                        string_add(&str, (char)c);
                        break;

                    case '*':
                        token.type = TOK_MUL;
                        completed = true;
                        break;

                    case '-':
                        token.type = TOK_MINUS;
                        completed = true;
                        break;

                    case ']':
                        token.type = TOK_RBRACKET;
                        parent_cnt--;
                        completed = true;
                        break;

                    case '[':
                        token.type = TOK_LBRACKET;
                        parent_cnt++;
                        completed = true;
                        break;

                    case '(':
                        token.type = TOK_LPAR;
                        parent_cnt++;
                        completed = true;
                        break;

                    case ')':
                        token.type = TOK_RPAR;
                        parent_cnt--;
                        completed = true;
                        break;

                    case ':':
                        token.type = TOK_COLON;
                        completed = true;
                        break;

                    case ';':
                        token.type = TOK_SEMICOLON;
                        completed = true;
                        break;

                    case ',':
                        token.type = TOK_COMMA;
                        completed = true;
                        break;

                    case '>':
                        token.state = state_greater_than;
                        break;

                    case '<':
                        token.state = state_lesser_than;
                        break;

                    case '=':
                        token.state = state_equal;
                        break;

                    case '!':
                        token.state = state_exclam;
                        break;

                    case '+':
                        token.state = state_plus;
                        break;

                    case '|':
                        token.state = state_pipe;
                        break;

                    case '&':
                        token.state = state_ampersand;
                        break;

                    case '/':
                        token.state = state_slash;
                        break;

                    case '_':
                        token.state = state_underscore;
                        break;

                    case '.':
                        token.state = state_dot;
                        break;

                    case '0':
                        token.state = state_zero;
                        break;

                    case ' ':
                        break;

                    case '"':
                        token.state = state_string_check;
                        break;

                    default:
                        if (isalpha(c)) {
                            token.state = state_ID;
                            token.type = TOK_ID;
                            string_add(&str, (char)c);
                        } else if (isdigit(c)) {
                            token.state = state_int;
                            token.type = TOK_INT;
                            string_add(&str, (char)c);
                        } else {
                            string_free(&str);
                            // print_error(ERR_LEX, line, "Chyba: Neočekávaný znak.");
                            token.type = TOK_ERROR;
                        }
                        break;
                }
                break;

            case state_plus:
                if (c == '+') {
                    token.type = TOK_CONCAT;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_PLUS;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_greater_than:
                if (c == '=') {
                    token.type = TOK_GREATEREQ;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_GREATERTHEN;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_exclam:
                if (c == '=') {
                    token.type = TOK_NOTEQ;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_NOT;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_pipe:
                if (c == '|') {
                    token.type = TOK_OR;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_PIPE;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_lesser_than:
                if (c == '-') {
                    token.type = TOK_LEFT_ARROW;
                    completed = true;
                    break;
                } else if (c == '=') {
                    token.type = TOK_LESSEQ;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_LESSTHEN;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_equal:
                if (c == '=') {
                    token.type = TOK_EQ;
                    completed = true;
                    break;
                } else if (c == '>') {
                    token.type = TOK_RIGHT_ARROW;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_ASSIGN;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_at:
                if (c == 'm') {
                    token.type = TOK_AT_MAIN;
                    token.state = state_main1;
                    break;
                } else {
                    token.type = TOK_ERROR;
                    // print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_main1:
                if (c == 'a') {
                    token.type = TOK_AT_MAIN;
                    token.state = state_main2;
                    break;
                } else {
                    token.type = TOK_ERROR;
                    // print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_main2:
                if (c == 'i') {
                    token.type = TOK_AT_MAIN;
                    token.state = state_main3;
                    break;
                } else {
                    token.type = TOK_ERROR;
                    // print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_main3:
                if (c == 'n') {
                    token.type = TOK_AT_MAIN;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_ERROR;
                    // print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_ampersand:
                if (c == '&') {
                    token.type = TOK_AND;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_ERROR;
                    // print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    completed = true;
                    break;
                }

            case state_slash:
                if (c == '*') {
                    token.state = state_com_block;
                    break;
                } else if (c == '/') {
                    token.state = state_com_line;
                    break;
                } else {
                    token.type = TOK_DIV;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_eol:
                token.newline_before = true;
                line++;

                if (parent_cnt) {
                    token.state = state_start;
                } else if (c == '\t') {
                    token.type = TOK_ERROR;
                    // print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    completed = true;
                } else if (c == ' ') {
                    indent_cnt++;
                } else {
                    ungetc(c, file);
                    int last_indent = lstack_top(indent_stack);

                    if (last_indent == indent_cnt) {
                        token.type = TOK_NEWLINE;
                        completed = true;
                        break;
                    } else if (last_indent < indent_cnt) {
                        lstack_push(indent_cnt, indent_stack);
                        token.type = TOK_NEWLINE;
                        tToken indent_tok;
                        indent_tok.type = TOK_INDENT;
                        fifo_add(&indent_tok, &queue->first);
                        completed = true;
                        break;
                    } else {
                        lstack_pop(indent_stack);
                        token.type = TOK_NEWLINE;
                        tToken dedent_tok;
                        dedent_tok.type = TOK_DEDENT;
                        fifo_add(&dedent_tok, &queue->first);
                        while (indent_cnt < last_indent) {
                            fifo_add(&token, &queue->first);
                            fifo_add(&dedent_tok, &queue->first);
                            lstack_pop(indent_stack);
                        }
                        if (last_indent < indent_cnt) {
                            // print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                            token.type = TOK_ERROR;
                        }
                        completed = true;
                        break;
                    }
                }
                break;

            case state_string_check:
                if (c == '"') {
                    token.state = state_empty_str;
                    break;
                } else if (c == '\n') {
                    // print_error(ERR_LEX, line, "Chyba: (Jednořádkový) Řetězcový literál nesmí
                    // obsahovat konec řádku");
                    token.type = TOK_ERROR;
                    break;
                } else if (__isascii(c)) {
                    token.state = state_string;
                    string_add(&str, (char)c);
                    break;
                } else {
                    string_free(&str);
                    token.type = TOK_ERROR;
                    break;
                }

            case state_empty_str:
                if (c == '"') {
                    token.state = state_chain_lit;
                    break;
                } else {
                    string_free(&str);
                    token.type = TOK_STRING;
                    token.val.str = "\0";
                    break;
                }

            case state_chain_lit:
                if (c == '"') {
                    token.state = state_cl1;
                    break;
                } else if (c == '\n') {
                    break;
                } else {
                    string_add(&str, (char)c);
                    token.type = TOK_ERROR;
                    break;
                }

            case state_underscore:
                if (isalnum(c) || c == '_') {
                    string_add(&str, '_');
                    string_add(&str, (char)c);
                    token.state = state_ID;
                    break;
                } else if (c == ' ') {
                    token.type = TOK_UNDERSCORE;
                    ungetc(c, file);
                    completed = true;
                    break;
                } else {
                    token.type = TOK_ERROR;
                    // print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    completed = true;
                    break;
                }

            case state_ID:
                if (isalnum(c) || c == '_') {
                    string_add(&str, (char)c);
                    break;
                } else {
                    token.type = TOK_ID;
                    token.val.str = str.string;
                    ungetc(c, file);
                    check_keyword(&token, &str);
                    check_var_type(&token, &str);
                    completed = true;
                    break;
                }

            case state_com_line:
                if (c == '\n') {
                    token.type = TOK_NEWLINE;
                    token.state = state_eol;
                    break;
                } else {
                    break;
                }

            case state_com_block:
                block_com_cnt++;
                if (c == '*') {
                    token.state = state_com_block_2;
                    break;
                } else {
                    if (c == '\n') {
                        if (com_nl) {
                            line++;
                        }
                        com_nl = true;
                        break;
                    }
                }

            case state_com_block_2:
                if (c == '/') {
                    block_com_cnt--;
                    if (block_com_cnt == 0) {
                        if (com_nl) {
                            token.state = state_eol;
                        } else {
                            token.state = state_start;
                        }
                    } else {
                        token.state = state_com_block;
                    }
                } else {
                    token.state = state_com_block;
                }
                break;

            default:
                break;
        }
    }
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

void lstack_push(int count, tLStack *stack) {
    if (!stack) {
        // print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu nebo
        // zásobník.");
        return;
    }

    tLStackElem *new_scope = (tLStackElem *)malloc(sizeof(tLStackElem));
    if (new_scope == NULL) {
        // print_error(ERR_INTERNAL, -1, "Chyba alokace paměti pro nový uzel tabulky symbolů.");
        return;
    }

    new_scope->count = count;
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

int lstack_top(tLStack *stack) {
    if (stack == NULL) {
        return -1;
    }
    return stack->top->count;
}

bool lstack_empty(tLStack *stack) {
    if (stack == NULL) {
        return true;
    }
    return stack->top == NULL;
}

void fifo_init(tFIFO *queue) {
    if (!queue) {
        // print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na frontu.");
        return;
    }
    queue = (tFIFO *)malloc(sizeof(tFIFO));
    if (queue == NULL) {
        // print_error(ERR_INTERNAL, -1, "Chyba: Selhání alokace.");
        return;
    }
    queue->first = NULL;
}

void fifo_add(tToken *token, tFIFO_Elem **first) {
    if (first == NULL) {
        (*first) = (tFIFO_Elem *)malloc(sizeof(tFIFO_Elem));
        if (first == NULL) {
            // print_error(ERR_INTERNAL, -1, "Chyba: Selhání alokace.");
            return;
        }
        (*first)->token = token;
        (*first)->next = NULL;
        return;
    }
    fifo_add(token, &(*first)->next);
}

tToken fifo_get(tFIFO *queue) {
    tToken out;
    if (!queue) {
        // print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na frontu.");
        out.type = TOK_ERROR;
        return out;
    }

    tFIFO_Elem *temp = queue->first;
    out = *queue->first->token;
    queue->first = temp->next;
    free(temp);
    return out;
}

void clean_elem(tFIFO_Elem *elem) {
    if (elem == NULL) {
        return;
    }
    clean_elem(elem->next);
    free(elem);
}

void fifo_clean(tFIFO *queue) {
    if (queue == NULL) {
        return;
    }
    clean_elem(queue->first);
    free(queue);
}
