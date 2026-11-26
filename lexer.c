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
#include <math.h>
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
    if (!queue || !indent_stack || lstack_empty(indent_stack)) {
        return make_internal_error(-1, "Chyba: Neinicializovaný lexikální analyzátor.");
    }
    if (queue->first) {
        return fifo_get(queue);
    }
    int c;
    sStr str;
    bool com_nl = false;
    if (string_init(&str)) {
        return make_internal_error(-1, "Chyba: Chyba inicializace stringu.");
    }

    tToken token = {0};
    token.type = TOK_EMPTY;
    token.line = line;
    token.val.str = NULL;
    token.state = state_start;
    token.newline_before = false;

    bool had_tab = false;
    bool check_indent = false;
    bool leading_zero = false;
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
                            token.line = nl_line;
                            line_has_tokens = false;
                        } else {
                            lstack_pop(indent_stack);
                            token.type = TOK_DEDENT;
                            token.line = line;

                            tToken nl_tok = {0};
                            nl_tok.type = TOK_NEWLINE;
                            nl_tok.line = line;
                            if (!fifo_add(&nl_tok, &queue->first)) {
                                token = make_internal_error(line, "Chyba: Selhání alokace paměti.");
                                completed = true;
                                break;
                            }
                        }

                        while (!lstack_empty(indent_stack) && lstack_top(indent_stack) > 0) {
                            lstack_pop(indent_stack);

                            tToken ded_tok = {0};
                            ded_tok.type = TOK_DEDENT;
                            ded_tok.line = line;

                            tToken nl_tok = {0};
                            nl_tok.type = TOK_NEWLINE;
                            nl_tok.line = line;

                            if (!fifo_add(&ded_tok, &queue->first) ||
                                !fifo_add(&nl_tok, &queue->first)) {
                                token = make_internal_error(line, "Chyba: Selhání alokace paměti.");
                                completed = true;
                                break;
                            }
                        }

                        if (!completed) {
                            tToken eof_tok = {0};
                            eof_tok.type = TOK_EOF;
                            eof_tok.line = line;
                            if (!fifo_add(&eof_tok, &queue->first)) {
                                token = make_internal_error(line, "Chyba: Selhání alokace paměti.");
                            }
                            completed = true;
                        }
                        break;
                    }

                    case '\n':
                        seen_newline = true;
                        if (parent_cnt > 0) {
                            line++;
                            while (true) {
                                int p_spaces = 0;
                                bool p_tab = false;
                                bool has_nl_com = false;
                                bool started_slash = false;
                                int pch = getc(file);
                                while (pch == ' ' || pch == '\t' || pch == '\r') {
                                    if (pch == ' ') {
                                        p_spaces++;
                                    } else if (pch == '\t') {
                                        p_tab = true;
                                    }
                                    pch = getc(file);
                                }

                                while (pch == '/') {
                                    int pnext = getc(file);
                                    if (pnext == '/') {
                                        while (pch != '\n' && pch != EOF) {
                                            pch = getc(file);
                                            if (pch != '\n' && pch != EOF &&
                                                (pch < 0 || pch > 127)) {
                                                token = make_lex_error(
                                                    line, "Chyba: Nepovolený znak v komentáři.");
                                                completed = true;
                                                break;
                                            }
                                        }
                                        break;
                                    } else if (pnext == '*') {
                                        int depth = 1;
                                        while (depth > 0 && !completed) {
                                            int bc = getc(file);
                                            if (bc == EOF || bc < 0 || bc > 127) {
                                                token = make_lex_error(
                                                    line, "Chyba: Neukončený nebo neplatný "
                                                          "blokový komentář.");
                                                completed = true;
                                            } else if (bc == '\n') {
                                                line++;
                                                has_nl_com = true;
                                            } else if (bc == '/') {
                                                int bc2 = getc(file);
                                                if (bc2 == '*') {
                                                    depth++;
                                                } else if (bc2 != EOF) {
                                                    ungetc(bc2, file);
                                                }
                                            } else if (bc == '*') {
                                                int bc2 = getc(file);
                                                if (bc2 == '/') {
                                                    depth--;
                                                } else if (bc2 != EOF) {
                                                    ungetc(bc2, file);
                                                }
                                            }
                                        }
                                        if (completed) {
                                            break;
                                        }
                                        pch = getc(file);
                                        while (pch == ' ' || pch == '\t' || pch == '\r') {
                                            pch = getc(file);
                                        }
                                    } else {
                                        if (pnext != EOF) {
                                            ungetc(pnext, file);
                                        }
                                        started_slash = true;
                                        break;
                                    }
                                }

                                if (completed) {
                                    break;
                                }
                                if (pch == '\n') {
                                    line++;
                                    continue;
                                }
                                if (pch == EOF) {
                                    break;
                                }
                                if (has_nl_com) {
                                    token =
                                        make_lex_error(line, "Chyba: Za víceřádkovým komentářem "
                                                             "nesmí následovat token.");
                                    completed = true;
                                    break;
                                }

                                int base_indent = lstack_top(indent_stack);
                                if (p_tab || p_spaces <= base_indent) {
                                    token =
                                        make_lex_error(line, "Chyba: Neplatné odsazení "
                                                             "pokračujícího řádku v závorkách.");
                                    completed = true;
                                    break;
                                }
                                if (started_slash) {
                                    token.state = state_slash;
                                } else {
                                    ungetc(pch, file);
                                }
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
                        append_char(&str, &token, &completed, (char)c);
                        break;

                    case '0':
                        token.state = state_zero;
                        append_char(&str, &token, &completed, (char)c);
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
                            append_char(&str, &token, &completed, (char)c);
                        } else if (isdigit(c)) {
                            token.state = state_int;
                            token.type = TOK_INT;
                            append_char(&str, &token, &completed, (char)c);
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
                                if (!fifo_add(&div_tok, &queue->first)) {
                                    token =
                                        make_internal_error(line, "Chyba: Selhání alokace paměti.");
                                }
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
                    token = make_lex_error(line, "Chyba: Řetězcový literál je neukončený "
                                                 "nebo obsahuje nepovolený znak.");
                    completed = true;
                    break;
                } else {
                    token.state = state_string;
                    append_char(&str, &token, &completed, (char)c);
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
                    token = make_lex_error(line, "Chyba: Řetězcový literál je neukončený "
                                                 "nebo obsahuje nepovolený znak.");
                    completed = true;
                    break;
                } else {
                    append_char(&str, &token, &completed, (char)c);
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
                        append_char(&str, &token, &completed, '\\');
                        token.state = state_string;
                        break;
                    case '"':
                        append_char(&str, &token, &completed, '"');
                        token.state = state_string;
                        break;
                    case 'n':
                        append_char(&str, &token, &completed, '\n');
                        token.state = state_string;
                        break;
                    case 'r':
                        append_char(&str, &token, &completed, '\r');
                        token.state = state_string;
                        break;
                    case 't':
                        append_char(&str, &token, &completed, '\t');
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
                    append_char(&str, &token, &completed, (char)c);
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
                    append_char(&str, &token, &completed, '"');
                    if (c == '\n') {
                        line++;
                    }
                    append_char(&str, &token, &completed, (char)c);
                    token.state = state_chain_lit;
                    break;
                }

            case state_cl2:
                if (c == '"') {
                    int next = getc(file);
                    if (next == '"') { // Pro případ 4. uvozovky
                        append_char(&str, &token, &completed, '"');
                        ungetc(next, file);
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
                    append_char(&str, &token, &completed, '"');
                    append_char(&str, &token, &completed, '"');
                    if (c == '\n') {
                        line++;
                    }
                    append_char(&str, &token, &completed, (char)c);
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
                    append_char(&str, &token, &completed, '_');
                    append_char(&str, &token, &completed, (char)c);
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
                    append_char(&str, &token, &completed, (char)c);
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
                                } else if (after2 == '*') {
                                    block_com_cnt = 1;
                                    token.state = state_com_block;
                                    break;
                                } else {
                                    token =
                                        make_lex_error(line, "Chyba: Za víceřádkovým komentářem "
                                                             "nesmí následovat token.");
                                    completed = true;
                                    break;
                                }
                            }
                            if (after != '\n' && after != EOF) {
                                token = make_lex_error(line, "Chyba: Za víceřádkovým komentářem "
                                                             "nesmí následovat token.");
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
                    leading_zero = true;
                    token.state = state_int;
                    append_char(&str, &token, &completed, (char)c);
                    break;
                } else if (c == '_') {
                    leading_zero = true;
                    token.state = state_int_unders;
                    break;
                } else if (c == '.') {
                    token.state = state_dot;
                    append_char(&str, &token, &completed, (char)c);
                    break;
                } else if (c == 'e' || c == 'E') {
                    token.state = state_exponent;
                    append_char(&str, &token, &completed, (char)c);
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
                    append_char(&str, &token, &completed, (char)c);
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
                    if (leading_zero) {
                        token =
                            make_lex_error(line, "Chyba: Číslo nesmí začínat nevýznamnou nulou.");
                        if (c != EOF) {
                            ungetc(c, file);
                        }
                        completed = true;
                        break;
                    }
                    str.string[--str.length] = '\0'; // Odstraní tečku ze stringu čísla
                    token.type = TOK_INT;
                    token.line = line;
                    token.val.i_val = strtoll(str.string, NULL, 10);

                    tToken dot_tok = {0};
                    dot_tok.type = TOK_DOT;
                    dot_tok.line = line;
                    if (!fifo_add(&dot_tok, &queue->first)) {
                        token = make_internal_error(line, "Chyba: Selhání alokace paměti.");
                    }

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
                    append_char(&str, &token, &completed, (char)c);
                    break;
                } else if (c == 'e' || c == 'E') {
                    token.state = state_exponent;
                    append_char(&str, &token, &completed, (char)c);
                    break;
                } else if (c == '.') {
                    token.state = state_dot;
                    append_char(&str, &token, &completed, (char)c);
                    break;
                } else {
                    if (leading_zero) {
                        token =
                            make_lex_error(line, "Chyba: Číslo nesmí začínat nevýznamnou nulou.");
                        if (c != EOF) {
                            ungetc(c, file);
                        }
                        completed = true;
                        break;
                    }

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
                    append_char(&str, &token, &completed, (char)c);
                    break;
                } else {
                    token = make_lex_error(line, "Chyba: Neočekávaný znak.");
                    ungetc(c, file);
                    completed = true;
                    break;
                }

            case state_exponent:
                if (c == '+' || c == '-') {
                    token.state = state_exp_sign;
                    append_char(&str, &token, &completed, (char)c);
                    break;
                } else if (isdigit(c)) {
                    token.state = state_exp_end;
                    append_char(&str, &token, &completed, (char)c);
                    break;
                } else {
                    token = make_lex_error(line, "Chyba: Neočekávaný znak.");
                    if (c != EOF) {
                        ungetc(c, file);
                    }
                    completed = true;
                    break;
                }

            case state_exp_sign:
                if (isdigit(c)) {
                    token.state = state_exp_end;
                    append_char(&str, &token, &completed, (char)c);
                    break;
                } else {
                    token = make_lex_error(line, "Chyba: Neočekávaný znak.");
                    if (c != EOF) {
                        ungetc(c, file);
                    }
                    completed = true;
                    break;
                }

            case state_exp_unders:
                if (isdigit(c)) {
                    token.state = state_exp_end;
                    append_char(&str, &token, &completed, (char)c);
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
                    append_char(&str, &token, &completed, (char)c);
                    break;
                } else if (c == 'e' || c == 'E') {
                    token.state = state_exponent;
                    append_char(&str, &token, &completed, (char)c);
                    break;
                } else if (c == 'd' || c == 'D') {
                    token.state = state_exp_d;
                    break;
                } else {
                    finish_double(&token, &str, c);
                    completed = true;
                    break;
                }

            case state_exp_end:
                if (c == '_') {
                    token.state = state_exp_unders;
                    break;
                } else if (isdigit(c)) {
                    append_char(&str, &token, &completed, (char)c);
                    break;
                } else if (c == 'd' || c == 'D') {
                    token.state = state_exp_d;
                    break;
                } else {
                    finish_double(&token, &str, c);
                    completed = true;
                    break;
                }

            case state_exp_d:
                if (isalnum(c) || c == '_') {
                    token = make_lex_error(line, "Chyba: Neplatná přípona desetinného čísla.");
                    completed = true;
                    break;
                }
                finish_double(&token, &str, c);
                completed = true;
                break;

            case state_dec_unders:
                if (isdigit(c)) {
                    token.state = state_decimal;
                    append_char(&str, &token, &completed, (char)c);
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

tToken make_lex_error(int err_line, const char *msg) {
    print_error(ERR_LEX, err_line, msg);
    tToken err = {0};
    err.state = state_start;
    err.type = TOK_ERROR;
    err.val.i_val = 1;
    err.line = err_line;
    return err;
}

tToken make_internal_error(int err_line, const char *msg) {
    print_error(ERR_INTERNAL, err_line, msg);
    tToken err = {0};
    err.state = state_start;
    err.type = TOK_ERROR;
    err.val.i_val = 99;
    err.line = err_line;
    return err;
}

void append_char(sStr *str, tToken *token, bool *completed, char ch) {
    if (!(*completed) && string_add(str, ch)) {
        *token = make_internal_error(line, "Chyba: Selhání alokace paměti.");
        *completed = true;
    }
}

void finish_double(tToken *token, sStr *str, int pushback_char) {
    if (pushback_char != EOF) {
        ungetc(pushback_char, file);
    }
    errno = 0;
    double val = strtod(str->string, NULL);
    if (errno == ERANGE && (val == HUGE_VAL || val == -HUGE_VAL || val == 0.0)) {
        *token = make_lex_error(line, "Chyba: Desetinný literál je mimo rozsah IEEE 754 binary64.");
        return;
    }
    token->type = TOK_DOUBLE;
    token->line = line;
    token->val.d_val = val;
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

bool lstack_push(int count, tLStack *stack) {
    if (!stack) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu nebo zásobník.");
        return false;
    }

    tLStackElem *new_scope = (tLStackElem *)malloc(sizeof(tLStackElem));
    if (new_scope == NULL) {
        print_error(ERR_INTERNAL, -1, "Chyba alokace paměti pro nový uzel tabulky symbolů.");
        return false;
    }

    new_scope->count = count;
    new_scope->next = stack->top;
    stack->top = new_scope;
    return true;
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

bool fifo_add(tToken *token, tFIFO_Elem **first) {
    if (first == NULL) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na prvek fronty.");
        return false;
    }
    if ((*first) == NULL) {
        (*first) = (tFIFO_Elem *)malloc(sizeof(tFIFO_Elem));
        if ((*first) == NULL) {
            print_error(ERR_INTERNAL, -1, "Chyba: Selhání alokace.");
            return false;
        }
        (*first)->token = (tToken *)malloc(sizeof(tToken));
        if ((*first)->token == NULL) {
            print_error(ERR_INTERNAL, -1, "Chyba: Selhání alokace.");
            free(*first);
            *first = NULL;
            return false;
        }
        *(*first)->token = *token;
        (*first)->next = NULL;
        return true;
    }
    return fifo_add(token, &(*first)->next);
}

tToken fifo_get(tFIFO *queue) {
    tToken out = {0};
    if (!queue || !queue->first) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na frontu.");
        out.type = TOK_ERROR;
        out.val.i_val = ERR_INTERNAL;
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

bool check_line_indent(tToken *token, int pushback_char, int prev_line, bool had_tab) {
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
            return false;
        }
        if (pushback_char != EOF) {
            ungetc(pushback_char, file);
        }
        if (!lstack_push(indent_cnt, indent_stack)) {
            *token = make_internal_error(line, "Chyba: Selhání alokace paměti.");
            return true;
        }
        token->type = TOK_NEWLINE;
        token->line = prev_line;

        tToken indent_tok = {0};
        indent_tok.type = TOK_INDENT;
        indent_tok.val.i_val = indent_cnt;
        indent_tok.line = line;
        if (!fifo_add(&indent_tok, &queue->first)) {
            *token = make_internal_error(line, "Chyba: Selhání alokace paměti.");
        }
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

            tToken nl_tok = {0};
            nl_tok.type = TOK_NEWLINE;
            nl_tok.line = line;

            if (!fifo_add(&ded_tok, &queue->first) || !fifo_add(&nl_tok, &queue->first)) {
                *token = make_internal_error(line, "Chyba: Selhání alokace paměti.");
                return true;
            }
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
