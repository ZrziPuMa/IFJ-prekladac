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
int line;
int parent_cnt;

void lexer_init(FILE *f) {
    file = f;
    lstack_init(indent_stack);
    parent_cnt = 0;
    line = 1;
}

tToken get_token() {
    int c;
    sStr str;
    if (string_init(&str)) {
        // print_error(ERR_INTERNAL, -1, "Chyba: Chyba inicializace stringu.");
    }

    tToken token;
    token.type = TOK_EMPTY;
    token.state = state_start;
    int indent_cnt = 0;
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
                            // print_error(ERR_LEX, line, "");
                        }
                        break;
                }

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

                if (c == '-') {
                    token.type = TOK_LEFT_ARROW;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_LESSTHEN;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_com_line:
                if (c == '\n') {
                    token.type = TOK_NEWLINE;
                    token.state = state_eol;
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

void lstack_push(tLStackElem *new_elem, tLStack *stack) {
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

    new_scope->count = new_elem->count;
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
