/**
 * Implemetnace lexikálního analyzátoru pro jazyk IFJ26
 * @file lexer.c
 * @brief Zdrojový soubor lexikálního analyzátoru
 * @author Daniela Sladká, xsladkd00
 */

#include "lexer.h"

#include "idynamic_string.h"
#include "ierror.h"
#include "ilexer.h"

#include <stdlib.h>

FILE *file;
tLStack *indent_stack;
tFIFO *queue;
int line;
int parent_cnt;
int block_com_cnt;
int indent_cnt;

void lexer_init(FILE *f) {
    file = f;
    lstack_init(&indent_stack);
    fifo_init(&queue);
    parent_cnt = 0;
    line = 1;
    block_com_cnt = 0;
    indent_cnt = 0;
    lstack_push(indent_cnt, indent_stack);
}

tToken give_token() {
    if (queue->first) {
        return fifo_get(queue);
    }
    int c;
    sStr str;
    bool com_nl = false;
    if (string_init(&str)) {
        print_error(ERR_INTERNAL, -1, "Chyba: Chyba inicializace stringu.");
    }

    tToken token;
    token.type = TOK_EMPTY;
    token.line = line;
    token.val.str = NULL;
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
                        token.line = line;
                        break;

                    case '\n':
                        token.type = TOK_NEWLINE;
                        token.line = line;
                        token.state = state_eol;
                        break;

                    case '@':
                        token.state = state_at;
                        string_add(&str, (char)c);
                        break;

                    case '*':
                        token.type = TOK_MUL;
                        token.line = line;
                        completed = true;
                        break;

                    case '-':
                        token.type = TOK_MINUS;
                        token.line = line;
                        completed = true;
                        break;

                    case ']':
                        token.type = TOK_RBRACKET;
                        token.line = line;
                        parent_cnt--;
                        completed = true;
                        break;

                    case '[':
                        token.type = TOK_LBRACKET;
                        token.line = line;
                        parent_cnt++;
                        completed = true;
                        break;

                    case '(':
                        token.type = TOK_LPAR;
                        token.line = line;
                        parent_cnt++;
                        completed = true;
                        break;

                    case ')':
                        token.type = TOK_RPAR;
                        token.line = line;
                        parent_cnt--;
                        completed = true;
                        break;

                    case ':':
                        token.type = TOK_COLON;
                        token.line = line;
                        completed = true;
                        break;

                    case ';':
                        token.type = TOK_SEMICOLON;
                        token.line = line;
                        completed = true;
                        break;

                    case ',':
                        token.type = TOK_COMMA;
                        token.line = line;
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
                        string_add(&str, (char)c);
                        break;

                    case '0':
                        token.state = state_zero;
                        string_add(&str, (char)c);
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
                            print_error(ERR_LEX, line, "Chyba: Neočekávaný znak.");
                            token.type = TOK_ERROR;
                            token.line = line;
                            token.val.i_val = 1;
                            completed = true;
                        }
                        break;
                }
                break;

            case state_plus:
                if (c == '+') {
                    token.type = TOK_CONCAT;
                    token.line = line;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_PLUS;
                    token.line = line;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_greater_than:
                if (c == '=') {
                    token.type = TOK_GREATEREQ;
                    token.line = line;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_GREATERTHEN;
                    token.line = line;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_exclam:
                if (c == '=') {
                    token.type = TOK_NOTEQ;
                    token.line = line;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_NOT;
                    token.line = line;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_pipe:
                if (c == '|') {
                    token.type = TOK_OR;
                    token.line = line;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_PIPE;
                    token.line = line;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_lesser_than:
                if (c == '-') {
                    token.type = TOK_LEFT_ARROW;
                    token.line = line;
                    completed = true;
                    break;
                } else if (c == '=') {
                    token.type = TOK_LESSEQ;
                    token.line = line;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_LESSTHEN;
                    token.line = line;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_equal:
                if (c == '=') {
                    token.type = TOK_EQ;
                    token.line = line;
                    completed = true;
                    break;
                } else if (c == '>') {
                    token.type = TOK_RIGHT_ARROW;
                    token.line = line;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_ASSIGN;
                    token.line = line;
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
                    print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    token.line = line;
                    token.val.i_val = 1;
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
                    print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    token.line = line;
                    token.val.i_val = 1;
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
                    print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    token.line = line;
                    token.val.i_val = 1;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_main3:
                if (c == 'n') {
                    token.type = TOK_AT_MAIN;
                    token.line = line;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_ERROR;
                    print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    token.line = line;
                    token.val.i_val = 1;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_ampersand:
                if (c == '&') {
                    token.type = TOK_AND;
                    token.line = line;
                    completed = true;
                    break;
                } else {
                    token.type = TOK_ERROR;
                    print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    token.line = line;
                    token.val.i_val = 1;
                    completed = true;
                    break;
                }

            case state_slash:
                if (c == '*') {
                    block_com_cnt++;
                    token.state = state_com_block;
                    break;
                } else if (c == '/') {
                    token.state = state_com_line;
                    break;
                } else {
                    token.type = TOK_DIV;
                    token.line = line;
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
                    print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    token.line = line;
                    token.val.i_val = 1;
                    completed = true;
                } else if (c == ' ') {
                    indent_cnt++;
                } else {
                    ungetc(c, file);
                    int last_indent = lstack_top(indent_stack);

                    if (last_indent == indent_cnt) {
                        token.type = TOK_NEWLINE;
                        token.line = line;
                        completed = true;
                        break;
                    } else if (last_indent < indent_cnt) {
                        lstack_push(indent_cnt, indent_stack);
                        token.type = TOK_NEWLINE;
                        token.line = line;
                        tToken indent_tok;
                        indent_tok.type = TOK_INDENT;
                        indent_tok.line = line;
                        fifo_add(&indent_tok, &queue->first);
                        completed = true;
                        break;
                    } else {
                        lstack_pop(indent_stack);
                        token.type = TOK_NEWLINE;
                        token.line = line;
                        tToken dedent_tok;
                        dedent_tok.type = TOK_DEDENT;
                        dedent_tok.line = line;
                        fifo_add(&dedent_tok, &queue->first);
                        while (indent_cnt < last_indent) {
                            fifo_add(&token, &queue->first);
                            fifo_add(&dedent_tok, &queue->first);
                            lstack_pop(indent_stack);
                        }
                        if (last_indent < indent_cnt) {
                            print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                            token.type = TOK_ERROR;
                            token.val.i_val = 1;
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
                } else if (c == '\\') {
                    token.state = state_string_esc;
                    break;
                } else if (c == '\n' || c == EOF || c < 0 || c > 127) {
                    print_error(
                        ERR_LEX, line,
                        "Chyba: Řetězcový literál je neukončený nebo obsahuje nepovolený znak.");
                    token.type = TOK_ERROR;
                    token.line = line;
                    token.val.i_val = 1;
                    completed = true;
                    break;
                } else {
                    token.state = state_string;
                    string_add(&str, (char)c);
                    break;
                }

            case state_string:
                if (c == '"') {
                    token.state = state_string_end;
                    break;
                } else if (c == '\\') {
                    token.state = state_string_esc;
                    break;
                } else if (c == '\n' || c == EOF || c < 0 || c > 127) {
                    print_error(
                        ERR_LEX, line,
                        "Chyba: Řetězcový literál je neukončený nebo obsahuje nepovolený znak.");
                    token.type = TOK_ERROR;
                    token.line = line;
                    token.val.i_val = 1;
                    completed = true;
                    break;
                } else {
                    string_add(&str, (char)c);
                    break;
                }

            case state_empty_str:
                if (c == '"') {
                    token.state = state_chain_lit;
                    break;
                } else {
                    token.type = TOK_STRING;
                    token.val.str = "";
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_chain_lit:
                if (c == '"') {
                    token.state = state_cl1;
                    break;
                } else if (c == '\n' || c == '\t' || c == '\\' || c == '\r' || c == '\"') {
                    if (c == '\n') {
                        line++;
                    }
                    string_add(&str, '\\');
                    string_add(&str, (char)c);
                    break;
                } else if (c == '\\') {
                    string_add(&str, '\\');
                    string_add(&str, '\\');
                    break;
                } else if (c == EOF) {
                    token.type = TOK_ERROR;
                    token.line = line;
                    token.val.i_val = 1;
                    print_error(ERR_LEX, line, "Chyba: Konec souboru před ukončením stringu.");
                    completed = true;
                    break;
                } else {
                    break;
                }

            case state_cl1:
                if (c == '"') {
                    token.state = state_cl2;
                    break;
                } else if (c == EOF) {
                    token.type = TOK_ERROR;
                    token.line = line;
                    token.val.i_val = 1;
                    print_error(ERR_LEX, line, "Chyba: Konec souboru před ukončením stringu.");
                    completed = true;
                    break;
                } else {
                    token.state = state_chain_lit;
                    ungetc(c, file);
                    string_add(&str, '"');
                    break;
                }

            case state_cl2:
                if (c == '"') {
                    token.state = state_string_end;
                    break;
                } else if (c == EOF) {
                    token.type = TOK_ERROR;
                    token.line = line;
                    token.val.i_val = 1;
                    print_error(ERR_LEX, line, "Chyba: Konec souboru před ukončením stringu.");
                    completed = true;
                    break;
                } else {
                    token.state = state_chain_lit;
                    ungetc(c, file);
                    string_add(&str, '"');
                    string_add(&str, '"');
                    break;
                }

            case state_string_end:
                token.type = TOK_STRING;
                token.line = line;
                token.val.str = str.string;
                str.string = NULL;
                completed = true;
                break;

            case state_underscore:
                if (isalnum(c) || c == '_') {
                    string_add(&str, '_');
                    string_add(&str, (char)c);
                    token.state = state_ID;
                    break;
                } else if (c == ' ') {
                    token.type = TOK_UNDERSCORE;
                    token.line = line;
                    ungetc(c, file);
                    completed = true;
                    break;
                } else {
                    token.type = TOK_ERROR;
                    print_error(ERR_LEX, line, "Chyba: Neočekávaný znak");
                    token.line = line;
                    token.val.i_val = 1;
                    completed = true;
                    break;
                }

            case state_ID:
                if (isalnum(c) || c == '_') {
                    string_add(&str, (char)c);
                    break;
                } else {
                    token.type = TOK_ID;
                    token.val.str = NULL;
                    token.line = line;
                    ungetc(c, file);
                    check_keyword(&token, &str);
                    if (token.type == TOK_ID) {
                        check_var_type(&token, &str);
                    }
                    if (token.type == TOK_ID || token.type == TOK_TYPE) {
                        token.val.str = str.string;
                        str.string = NULL;
                    }
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

            case state_zero:
                if (c == ' ') {
                    token.type = TOK_INT;
                    token.val.i_val = 0;
                    completed = true;
                    break;
                } else if (isdigit(c)) {
                    token.type = TOK_ERROR;
                    print_error(ERR_LEX, line,
                                "Chyba: Číslo nesmí začínat 0, pokud to není jeho hodnota.");
                    token.line = line;
                    token.val.i_val = 1;
                    completed = true;
                    break;
                } else if (c == '.') {
                    token.state = state_dot;
                    string_add(&str, (char)c);
                    break;
                } else {
                    token.type = TOK_INT;
                    token.line = line;
                    token.val.i_val = 0;
                    completed = true;
                    break;
                }

            case state_dot:
                if (c == ' ') {
                    token.type = TOK_INT;
                    token.val.i_val = (int)strtol(str.string, NULL, 10);
                    completed = true;
                    break;
                } else if (isdigit(c)) {
                    token.state = state_decimal;
                    token.type = TOK_DOUBLE;
                    string_add(&str, (char)c);
                    completed = true;
                    break;
                } else if (c == 't' || c == 'f' || c == 'l') {
                    token.type = TOK_INT;
                    token.line = line;
                    token.val.i_val = (int)strtol(str.string, NULL, 10);
                    fifo_add(&token, &queue->first);
                    tToken dot_tok;
                    dot_tok.type = TOK_DOT;
                    dot_tok.line = line;
                    fifo_add(&dot_tok, &queue->first);
                    ungetc(c, file);
                    break;
                } else {
                    token.type = TOK_ERROR;
                    print_error(ERR_INTERNAL, -1, "Chyba: Neočekávaný znak.");
                    token.line = line;
                    token.val.i_val = 1;
                    completed = true;
                    break;
                }

            case state_int:
                if (c == '_') {
                    token.state = state_int_unders;
                    break;
                } else if (isdigit(c)) {
                    string_add(&str, (char)c);
                    break;
                } else if (c == 'e' || c == 'E') {
                    token.state = state_exponent;
                    string_add(&str, (char)c);
                    break;
                } else if (c == '.') {
                    token.state = state_dot;
                    string_add(&str, (char)c);
                    break;
                } else {
                    token.type = TOK_INT;
                    token.line = line;
                    token.val.i_val = (int)strtol(str.string, NULL, 10);
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_int_unders:
                if (isdigit(c)) {
                    token.state = state_int;
                    string_add(&str, (char)c);
                    break;
                } else {
                    token.type = TOK_ERROR;
                    token.line = line;
                    token.val.i_val = 1;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_exponent:
                if (c == '+' || c == '-' || isdigit(c)) {
                    token.state = state_exp_end;
                    string_add(&str, (char)c);
                    break;
                } else {
                    token.type = TOK_ERROR;
                    token.line = line;
                    token.val.i_val = 1;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_exp_unders:
                if (isdigit(c)) {
                    token.state = state_exponent;
                    string_add(&str, (char)c);
                    break;
                } else {
                    token.type = TOK_ERROR;
                    token.line = line;
                    token.val.i_val = 1;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_decimal:
                if (c == '_') {
                    token.state = state_dec_unders;
                    break;
                } else if (isdigit(c)) {
                    string_add(&str, (char)c);
                    break;
                } else if (c == 'e' || c == 'E') {
                    token.state = state_exponent;
                    string_add(&str, (char)c);
                    break;
                } else if (c == 'd' || c == 'D') {
                    token.state = state_exp_d;
                    break;
                } else {
                    token.type = TOK_DOUBLE;
                    token.line = line;
                    token.val.d_val = strtod(str.string, NULL);
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_exp_end:
                if (c == '_') {
                    token.state = state_exp_unders;
                    break;
                } else if (isdigit(c)) {
                    string_add(&str, (char)c);
                    break;
                } else {
                    token.type = TOK_DOUBLE;
                    token.line = line;
                    token.val.d_val = strtod(str.string, NULL);
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_exp_d:
                token.type = TOK_DOUBLE;
                token.line = line;
                token.val.d_val = strtod(str.string, NULL);
                ungetc(c, file);
                completed = true;
                break;

            case state_dec_unders:
                if (isdigit(c)) {
                    token.state = state_decimal;
                    string_add(&str, (char)c);
                    break;
                } else {
                    token.type = TOK_ERROR;
                    token.line = line;
                    token.val.i_val = 1;
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            default:
                break;
        }
    }
    string_free(&str);
    return token;
}

void lexer_free() {
    fifo_clean(queue);
    queue = NULL;
    while (indent_stack != NULL && !lstack_empty(indent_stack)) {
        lstack_pop(indent_stack);
    }
    free(indent_stack);
    indent_stack = NULL;
}

void lstack_init(tLStack **stack) {
    if (!stack) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na zásobník.");
        return;
    }
    *stack = (tLStack *)malloc(sizeof(tLStack));
    if ((*stack) == NULL) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na zásobník.");
        return;
    }
    (*stack)->top = NULL;
}

void lstack_push(int count, tLStack *stack) {
    if (!stack) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu nebo zásobník.");
        return;
    }

    tLStackElem *new_scope = (tLStackElem *)malloc(sizeof(tLStackElem));
    if (new_scope == NULL) {
        print_error(ERR_INTERNAL, -1, "Chyba alokace paměti pro nový uzel tabulky symbolů.");
        return;
    }

    new_scope->count = count;
    new_scope->next = stack->top;
    stack->top = new_scope;
}

void lstack_pop(tLStack *stack) {
    if (stack == NULL) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na zásobník.");
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

void fifo_init(tFIFO **queue) {
    if (!queue) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na frontu.");
        return;
    }
    *queue = (tFIFO *)malloc(sizeof(tFIFO));
    if ((*queue) == NULL) {
        print_error(ERR_INTERNAL, -1, "Chyba: Selhání alokace.");
        return;
    }
    (*queue)->first = NULL;
}

void fifo_add(tToken *token, tFIFO_Elem **first) {
    if (first != NULL && (*first) == NULL) {
        (*first) = (tFIFO_Elem *)malloc(sizeof(tFIFO_Elem));
        if ((*first) == NULL) {
            print_error(ERR_INTERNAL, -1, "Chyba: Selhání alokace.");
            return;
        }
        (*first)->token = (tToken *)malloc(sizeof(tToken));
        if ((*first)->token == NULL) {
            print_error(ERR_INTERNAL, -1, "Chyba: Selhání alokace.");
            free(*first);
            *first == NULL;
            return;
        }
        *((*first)->token) = *token;
        (*first)->next = NULL;
        return;
    }
    fifo_add(token, &(*first)->next);
}

tToken fifo_get(tFIFO *queue) {
    tToken out;
    if (!queue) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na frontu.");
        out.type = TOK_ERROR;
        out.val.i_val = 99;
        return out;
    }

    tFIFO_Elem *temp = queue->first;
    out = *queue->first->token;
    queue->first = temp->next;
    free(temp->token);
    free(temp);
    return out;
}

void clean_elem(tFIFO_Elem *elem) {
    if (elem == NULL) {
        return;
    }
    clean_elem(elem->next);
    free(elem->token);
    free(elem);
}

void fifo_clean(tFIFO *queue) {
    if (queue == NULL) {
        return;
    }
    clean_elem(queue->first);
    free(queue);
}
