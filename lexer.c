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

#include <errno.h>
#include <stdlib.h>

static FILE *file;
static tLStack *indent_stack;
static tFIFO *queue;
static int line;
static int parent_cnt;
static int block_com_cnt;
static int indent_cnt;
static bool starts_at_line;
static bool seen_newline;
static bool line_has_tokens;

/**
 * @brief Vypsání lexikální chyby
 * @param err_line Řádek, kde chyba nastala
 * @param msg Zpráva, která se vypíše
 * @return Chybový token TOK_ERROR
 */
static tToken make_lex_error(int err_line, const char *msg);

/**
 * @brief Vyhodnocení úrovně odsazení na začátku neprázdného řádku
 * @param token Token, kterému se předá informace (případně error)
 * @param pushback_char První znak tokenu po odsazení
 * @param prev_line Číslo předcházejícího řádku (pro TOK_NEWLINE)
 * @param had_tab Kontrola, zda byl použit tabulátor na začátku řádku
 * @return true, pokud byl vytvořen token k okamžitému vrácení, jinak false
 */
static bool check_line_indent(tToken *token, int pushback_char, int prev_line, bool had_tab);

void lexer_init(FILE *f) {
    file = f;
    lstack_init(&indent_stack);
    fifo_init(&queue);
    parent_cnt = 0;
    line = 1;
    block_com_cnt = 0;
    indent_cnt = 0;
    starts_at_line = true;
    seen_newline = false;
    line_has_tokens = false;
    lstack_push(indent_cnt, indent_stack);
}

tToken give_token(void) {
    if (queue->first) {
        return fifo_get(queue);
    }
    int c;
    sStr str;
    bool com_nl = false;
    if (string_init(&str)) {
        print_error(ERR_INTERNAL, -1, "Chyba: Chyba inicializace stringu.");
    }

    tToken token = {0};
    token.type = TOK_EMPTY;
    token.line = line;
    token.val.str = NULL;
    token.state = state_start;
    token.newline_before = false;

    bool had_tab = false;
    bool check_indent = false;
    int nl_line = line;
    indent_cnt = 0;

    if (starts_at_line) {
        token.state = state_eol;
        starts_at_line = false;
    }

    bool completed = false;

    while (!completed) {
        c = getc(file);
        switch (token.state) {
            case state_start:
                if (check_indent && parent_cnt == 0 && c != ' ' && c != '\t' && c != '\r' &&
                    c != '\n' && c != EOF && c != '/') {
                    check_indent = false;
                    if (check_line_indent(&token, c, nl_line, had_tab)) {
                        completed = true;
                        break;
                    }
                }
                switch (c) {
                    case EOF: {
                        check_indent = false;
                        bool has_open_blocks =
                            (bool)(!lstack_empty(indent_stack) && lstack_top(indent_stack) > 0);

                        if (!line_has_tokens && !has_open_blocks) {
                            token.type = TOK_EOF;
                            token.line = line;
                            completed = true;
                            break;
                        }

                        if (line_has_tokens) {
                            token.type = TOK_NEWLINE;
                            token.line = line;
                            line_has_tokens = false;
                        } else {
                            lstack_pop(indent_stack);
                            token.type = TOK_DEDENT;
                            token.line = line;

                            tToken nl_tok = {0};
                            nl_tok.type = TOK_NEWLINE;
                            nl_tok.line = line;
                            fifo_add(&nl_tok, &queue->first);
                        }

                        while (!lstack_empty(indent_stack) && lstack_top(indent_stack) > 0) {
                            lstack_pop(indent_stack);

                            tToken ded_tok = {0};
                            ded_tok.type = TOK_DEDENT;
                            ded_tok.line = line;
                            fifo_add(&ded_tok, &queue->first);

                            tToken nl_tok = {0};
                            nl_tok.type = TOK_NEWLINE;
                            nl_tok.line = line;
                            fifo_add(&nl_tok, &queue->first);
                        }

                        tToken eof_tok = {0};
                        eof_tok.type = TOK_EOF;
                        eof_tok.line = line;
                        fifo_add(&eof_tok, &queue->first);

                        completed = true;
                        break;
                    }

                    case '\n':
                        seen_newline = true;
                        if (parent_cnt > 0) {
                            line++;
                            while (true) {
                                int p_spaces = 0;
                                bool p_tab = false;
                                int pch = getc(file);
                                while (pch == ' ' || pch == '\t' || pch == '\r') {
                                    if (pch == ' ') {
                                        p_spaces++;
                                    } else if (pch == '\t') {
                                        p_tab = true;
                                    }
                                    pch = getc(file);
                                }
                                if (pch == '\n') {
                                    line++;
                                    continue;
                                }
                                if (pch == EOF) {
                                    break;
                                }
                                if (pch == '/') {
                                    int pnext = getc(file);
                                    if (pnext == '/') {
                                        while (pch != '\n' && pch != EOF) {
                                            pch = getc(file);
                                        }
                                        if (pch == '\n') {
                                            line++;
                                            continue;
                                        }
                                        break;
                                    }
                                    if (pnext != EOF) {
                                        ungetc(pnext, file);
                                    }
                                }
                                int base_indent = lstack_top(indent_stack);
                                if (p_tab || p_spaces < base_indent ||
                                    (pch != ')' && pch != ']' && p_spaces <= base_indent)) {
                                    token =
                                        make_lex_error(line, "Chyba: Neplatné odsazení "
                                                             "pokračujícího řádku v závorkách.");
                                    completed = true;
                                    break;
                                }
                                ungetc(pch, file);
                                break;
                            }
                            break;
                        } else {
                            nl_line = line;
                            line++;
                            indent_cnt = 0;
                            had_tab = false;
                            check_indent = false;
                            token.state = state_eol;
                            break;
                        }

                    case '@':
                        token.state = state_at;
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
                        if (parent_cnt > 0) {
                            parent_cnt--;
                        }
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
                        if (parent_cnt > 0) {
                            parent_cnt--;
                        }
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
                    case '\t':
                    case '\r':
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
                            token = make_lex_error(line, "Chyba: Neočekávaný znak.");
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
                    token = make_lex_error(line, "Chyba: Neočekávaný znak.");
                    if (c != EOF) {
                        ungetc(c, file);
                    }
                    completed = true;
                    break;
                }

            case state_main1:
                if (c == 'a') {
                    token.type = TOK_AT_MAIN;
                    token.state = state_main2;
                    break;
                } else {
                    token = make_lex_error(line, "Chyba: Neočekávaný znak.");
                    if (c != EOF) {
                        ungetc(c, file);
                    }
                    completed = true;
                    break;
                }

            case state_main2:
                if (c == 'i') {
                    token.type = TOK_AT_MAIN;
                    token.state = state_main3;
                    break;
                } else {
                    token = make_lex_error(line, "Chyba: Neočekávaný znak.");
                    if (c != EOF) {
                        ungetc(c, file);
                    }
                    completed = true;
                    break;
                }

            case state_main3:
                if (c == 'n') {
                    int next = getc(file);
                    if (isalnum(next) || next == '_') {
                        token = make_lex_error(line, "Chyba: Neplatné zapsání @main.");
                        completed = true;
                        break;
                    }
                    if (next != EOF) {
                        ungetc(next, file);
                    }
                    token.type = TOK_AT_MAIN;
                    token.line = line;
                    completed = true;
                    break;
                } else {
                    token = make_lex_error(line, "Chyba: Neočekávaný znak.");
                    if (c != EOF) {
                        ungetc(c, file);
                    }
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
                    token = make_lex_error(line, "Chyba: Neočekávaný znak.");
                    completed = true;
                    break;
                }

            case state_slash:
                if (c == '*') {
                    block_com_cnt = 1;
                    com_nl = false;
                    token.state = state_com_block;
                    break;
                } else if (c == '/') {
                    token.state = state_com_line;
                    break;
                } else {
                    if (c != EOF) {
                        ungetc(c, file);
                    }
                    if (check_indent && parent_cnt == 0) {
                        check_indent = false;
                        if (check_line_indent(&token, EOF, nl_line, had_tab)) {
                            if (token.type == TOK_NEWLINE) {
                                tToken div_tok = {0};
                                div_tok.type = TOK_DIV;
                                div_tok.line = line;
                                fifo_add(&div_tok, &queue->first);
                            }
                            completed = true;
                            break;
                        }
                    }
                    token.type = TOK_DIV;
                    token.line = line;
                    completed = true;
                    break;
                }

            case state_eol:
                if (c == ' ') {
                    indent_cnt++;
                } else if (c == '\t') {
                    had_tab = true;
                } else if (c == '\r') {
                } else if (c == '\n') {
                    seen_newline = true;
                    nl_line = line;
                    line++;
                    indent_cnt = 0;
                    had_tab = false;
                } else {
                    if (c != EOF) {
                        ungetc(c, file);
                    }
                    check_indent = true;
                    token.state = state_start;
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
                    token = make_lex_error(
                        line,
                        "Chyba: Řetězcový literál je neukončený nebo obsahuje nepovolený znak.");
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
                    token = make_lex_error(
                        line,
                        "Chyba: Řetězcový literál je neukončený nebo obsahuje nepovolený znak.");
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
                    if (c != EOF) {
                        ungetc(c, file);
                    }
                    token.type = TOK_STRING;
                    token.line = line;
                    token.val.str = str.string;
                    str.string = NULL;
                    completed = true;
                    break;
                }

            case state_string_esc:
                switch (c) {
                    case '\\':
                        string_add(&str, '\\');
                        token.state = state_string;
                        break;
                    case '"':
                        string_add(&str, '"');
                        token.state = state_string;
                        break;
                    case 'n':
                        string_add(&str, '\n');
                        token.state = state_string;
                        break;
                    case 'r':
                        string_add(&str, '\r');
                        token.state = state_string;
                        break;
                    case 't':
                        string_add(&str, '\t');
                        token.state = state_string;
                        break;
                    default:
                        token = make_lex_error(line, "Chyba: Neplatná escape sekvence v řetězci.");
                        completed = true;
                        break;
                }
                break;

            case state_chain_lit:
                if (c == '"') {
                    token.state = state_cl1;
                    break;
                } else if (c == EOF || c < 0 || c > 127) {
                    token = make_lex_error(line, "Chyba: Neukončený víceřádkový literál.");
                    completed = true;
                    break;
                } else {
                    if (c == '\n') {
                        line++;
                    }
                    string_add(&str, (char)c);
                    break;
                }

            case state_cl1:
                if (c == '"') {
                    token.state = state_cl2;
                    break;
                } else if (c == EOF || c < 0 || c > 127) {
                    token = make_lex_error(line, "Chyba: Neukončený víceřádkový literál.");
                    completed = true;
                    break;
                } else {
                    string_add(&str, '"');
                    if (c == '\n') {
                        line++;
                    }
                    string_add(&str, (char)c);
                    token.state = state_chain_lit;
                    break;
                }

            case state_cl2:
                if (c == '"') {
                    int next = getc(file);
                    if (next == '"') { // Pro případ 4. uvozovky
                        string_add(&str, '"');
                    } else {
                        if (next != EOF) {
                            ungetc(next, file);
                        }
                        token.state = state_string_end;
                    }
                    break;
                } else if (c == EOF || c < 0 || c > 127) {
                    token = make_lex_error(line, "Chyba: Neukončený víceřádkový literál.");
                    completed = true;
                    break;
                } else {
                    string_add(&str, '"');
                    string_add(&str, '"');
                    if (c == '\n') {
                        line++;
                    }
                    string_add(&str, (char)c);
                    token.state = state_chain_lit;
                    break;
                }

            case state_string_end:
                token.type = TOK_STRING;
                token.val.str = str.string;
                if (c != EOF) {
                    ungetc(c, file);
                }
                str.string = NULL;
                completed = true;
                break;

            case state_underscore:
                if (isalnum(c) || c == '_') {
                    string_add(&str, '_');
                    string_add(&str, (char)c);
                    token.state = state_ID;
                    break;
                } else {
                    token.type = TOK_UNDERSCORE;
                    token.line = line;
                    if (c != EOF) {
                        ungetc(c, file);
                    }
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
                    ungetc('\n', file);
                    token.state = state_start;
                    break;
                } else if (c == EOF) {
                    token.state = state_start;
                    break;
                } else if (c < 0 || c > 127) {
                    token = make_lex_error(line, "Chyba: Nepovolený znak v komentáři.");
                    completed = true;
                    break;
                } else {
                    break;
                }

            case state_com_block:
                if (c == '*') {
                    token.state = state_com_block_2;
                    break;
                } else if (c == '/') {
                    int next = getc(file);
                    if (next == '*') {
                        block_com_cnt++;
                    } else if (next == EOF) {
                        token = make_lex_error(line, "Chyba: Neukončený blokový komentář.");
                        completed = true;
                    } else {
                        ungetc(next, file);
                    }
                    break;
                } else if (c == EOF || c < 0 || c > 127) {
                    token =
                        make_lex_error(line, "Chyba: Neukončený nebo neplatný blokový komentář.");
                    completed = true;
                    break;
                } else {
                    if (c == '\n') {
                        line++;
                        com_nl = true;
                    }
                    break;
                }

            case state_com_block_2:
                if (c == '*') {
                    break;
                } else if (c == '/') {
                    block_com_cnt--;
                    if (block_com_cnt == 0) {
                        if (com_nl) {
                            int after = getc(file);
                            while (after == ' ' || after == '\t' || after == '\r') {
                                after = getc(file);
                            }
                            if (after == '/') {
                                int after2 = getc(file);
                                if (after2 == '/') {
                                    while (after != '\n' && after != EOF) {
                                        after = getc(file);
                                    }
                                } else {
                                    token =
                                        make_lex_error(line, "Chyba: Za víceřádkovým komentářem "
                                                             "nesmí následovat token.");
                                    completed = true;
                                    break;
                                }
                            }
                            if (after != '\n' && after != EOF) {
                                token = make_lex_error(
                                    line,
                                    "Chyba: Za víceřádkovým komentářem nesmí následovat token.");
                                completed = true;
                                break;
                            }
                            if (after == '\n') {
                                ungetc(after, file);
                            }
                            com_nl = false;
                        }
                        token.state = state_start;
                    } else {
                        token.state = state_com_block;
                    }
                    break;
                } else if (c == EOF || c < 0 || c > 127) {
                    token =
                        make_lex_error(line, "Chyba: Neukončený nebo neplatný blokový komentář.");
                    completed = true;
                    break;
                } else {
                    if (c == '\n') {
                        line++;
                        com_nl = true;
                    }
                    token.state = state_com_block;
                    break;
                }

            case state_zero:
                if (isdigit(c)) {
                    token = make_lex_error(
                        line, "Chyba: Číslo nesmí začínat 0, pokud to není jeho hodnota.");
                    completed = true;
                    break;
                } else if (c == '.') {
                    token.state = state_dot;
                    string_add(&str, (char)c);
                    break;
                } else if (c == 'e' || c == 'E') {
                    token.state = state_exponent;
                    string_add(&str, (char)c);
                    break;
                } else {
                    token.type = TOK_INT;
                    token.line = line;
                    token.val.i_val = 0;
                    if (c != EOF) {
                        ungetc(c, file);
                    }
                    completed = true;
                    break;
                }

            case state_dot:
                if (isdigit(c)) {
                    token.state = state_decimal;
                    token.type = TOK_DOUBLE;
                    string_add(&str, (char)c);
                    break;
                } else if (str.length == 1) { // Samostatná tečka (např. a.b nebo import ifj26.*)
                    token.type = TOK_DOT;
                    token.line = line;
                    if (c != EOF) {
                        ungetc(c, file);
                    }
                    completed = true;
                    break;
                } else {
                    str.string[--str.length] = '\0'; // Odstraníme tečku ze stringu čísla
                    token.type = TOK_INT;
                    token.line = line;
                    token.val.i_val = strtoll(str.string, NULL, 10);

                    tToken dot_tok = {0};
                    dot_tok.type = TOK_DOT;
                    dot_tok.line = line;
                    fifo_add(&dot_tok, &queue->first);

                    if (c != EOF) {
                        ungetc(c, file);
                    }
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
                    token.val.i_val = strtoll(str.string, NULL, 10);
                    if (c != EOF) {
                        ungetc(c, file);
                    }
                    completed = true;
                    break;
                }

            case state_int_unders:
                if (isdigit(c)) {
                    token.state = state_int;
                    string_add(&str, (char)c);
                    break;
                } else {
                    token = make_lex_error(line, "Chyba: Neočekávaný znak.");
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
                    token = make_lex_error(line, "Chyba: Neočekávaný znak.");
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_exp_unders:
                if (isdigit(c)) {
                    token.state = state_exp_end;
                    string_add(&str, (char)c);
                    break;
                } else {
                    token = make_lex_error(line, "Chyba: Neočekávaný znak.");
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
                } else if (c == 'd' || c == 'D') {
                    token.state = state_exp_d;
                    break;
                } else {
                    token.type = TOK_DOUBLE;
                    token.line = line;
                    token.val.d_val = strtod(str.string, NULL);
                    if (c != EOF) {
                        ungetc(c, file);
                    }
                    completed = true;
                    break;
                }

            case state_exp_d:
                if (isalnum(c) || c == '_') {
                    token = make_lex_error(line, "Chyba: Neplatná přípona desetinného čísla.");
                    completed = true;
                    break;
                }
                token.type = TOK_DOUBLE;
                token.line = line;
                token.val.d_val = strtod(str.string, NULL);
                if (c != EOF) {
                    ungetc(c, file);
                }
                completed = true;
                break;

            case state_dec_unders:
                if (isdigit(c)) {
                    token.state = state_decimal;
                    string_add(&str, (char)c);
                    break;
                } else {
                    token = make_lex_error(line, "Chyba: Neočekávaný znak.");
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            default:
                break;
        }
    }
    if (token.type != TOK_NEWLINE && token.type != TOK_INDENT && token.type != TOK_DEDENT &&
        token.type != TOK_EOF && token.type != TOK_ERROR) {
        token.newline_before = seen_newline;
        seen_newline = false;
        line_has_tokens = true;
    } else if (token.type == TOK_NEWLINE) {
        line_has_tokens = false;
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

static tToken make_lex_error(int err_line, const char *msg) {
    print_error(ERR_LEX, err_line, msg);
    tToken err = {0};
    err.state = state_start;
    err.type = TOK_ERROR;
    err.val.i_val = 1;
    err.line = err_line;
    return err;
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
    if (stack == NULL || stack->top == NULL) {
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
            *first = NULL;
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

static bool check_line_indent(tToken *token, int pushback_char, int prev_line, bool had_tab) {
    if (had_tab) {
        *token = make_lex_error(line, "Chyba: Tabulátor na začátku neprázdného řádku.");
        return true;
    }

    int last_indent = lstack_top(indent_stack);

    if (indent_cnt == last_indent) {
        if (line_has_tokens) {
            if (pushback_char != EOF) {
                ungetc(pushback_char, file);
            }
            token->type = TOK_NEWLINE;
            token->line = prev_line;
            return true;
        }
        return false;
    } else if (indent_cnt > last_indent) {
        if (!line_has_tokens) {
            *token = make_lex_error(line, "Chyba: Neplatné odsazení na začátku souboru.");
            return true;
        }
        if (pushback_char != EOF) {
            ungetc(pushback_char, file);
        }
        lstack_push(indent_cnt, indent_stack);
        token->type = TOK_NEWLINE;
        token->line = prev_line;

        tToken indent_tok = {0};
        indent_tok.type = TOK_INDENT;
        indent_tok.val.i_val = indent_cnt;
        indent_tok.line = line;
        fifo_add(&indent_tok, &queue->first);
        return true;
    } else {
        if (pushback_char != EOF) {
            ungetc(pushback_char, file);
        }
        while (!lstack_empty(indent_stack) && indent_cnt < lstack_top(indent_stack)) {
            lstack_pop(indent_stack);

            tToken ded_tok = {0};
            ded_tok.type = TOK_DEDENT;
            ded_tok.line = line;
            fifo_add(&ded_tok, &queue->first);

            tToken nl_tok = {0};
            nl_tok.type = TOK_NEWLINE;
            nl_tok.line = line;
            fifo_add(&nl_tok, &queue->first);
        }
        if (lstack_empty(indent_stack) || lstack_top(indent_stack) != indent_cnt) {
            *token = make_lex_error(line, "Chyba: Neplatná úroveň odsazení.");
            return true;
        }
        token->type = TOK_NEWLINE;
        token->line = prev_line;
        return true;
    }
}
