#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "lex.h"

#define next(l) (l->p++)

static void lex_err(lexer_t *l, const char *msg) {
    fprintf(stderr, "lexical error at or near line %d: %s\n", l->line, msg);
    exit(1);
}

static bool valid_alpha(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c == '_');
}

// Incomplete
static void read_char(lexer_t *l) {
    char c;
    if (*l->p == '\\') {
        next(l);
        switch (*l->p) {
        case 'n': c = '\n'; break;
        default:  lex_err(l, "Invalid escape sequence");
        }
    } else
        c = *l->p;
    l->token.lexeme.c = c;
    next(l);
    if (*l->p != '\'')
        lex_err(l, "Invalid character definition; Use double quotation marks (\"\") to define strings");
    next(l);
}

// Incomplete, obvs
static int read_num(lexer_t *l, int first) {
    if (first == '0') {
        if (*l->p == 'x' || *l->p == 'X') {
        } else if (*l->p == 'b' || *l->p == 'B') {
        }
    }
    return 0;
}

static int test2(lexer_t *l, int c, int t1, int t2) {
    if (*l->p == c) {
        next(l);
        return t1;
    } else
        return t2;
}

static int test3(lexer_t *l, int c1, int t1, int c2, int t2, int t3) {
    if (*l->p == c1) {
        next(l);
        return t1;
    } else if (*l->p == c2) {
        next(l);
        return t2;
    } else
        return t3;
}

static int
test4(lexer_t *l, int c1, int t1, int c2, int c3, int t2, int t3, int t4) {
    if (*l->p == c1) {
        next(l);
        return t1;
    } else if (*l->p == c2) {
        next(l);
        if (*l->p == c3) {
            next(l);
            return t2;
        } else
            return t3;
    } else
        return t4;
}

static int tokenize(lexer_t *l) {
    int c;
    while (1) {
        switch (c = *l->p++) {
        case '\n': case '\r': l->line++;
        case ' ': case '\t': break;
        case '!': return test2(l, '=', TK_NEQ, '!');
        case '"':
            // Read string
        case '%': return test2(l, '=', TK_MOD_ASSIGN, '%');
        case '&': return test3(l, '=', TK_AND_ASSIGN, '&', TK_AND, '&');
        case '\'':
            read_char(l);
            return TK_CHAR;
        case '*': return test4(l, '=', TK_MUL_ASSIGN, '*', 
                                  '=', TK_POW_ASSIGN, TK_POW, '*');
        case '+': return test3(l, '=', TK_ADD_ASSIGN, '+', TK_INC, '+');
        case '-': return test3(l, '=', TK_SUB_ASSIGN, '-', TK_DEC, '-');
        case '.':
            // ., .., ..., floating-point number
        case '/':
            // /, /=, //
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return read_num(l, c);
            // Read number
        case '<': return test4(l, '=', TK_LTE, '<',
                                  '=', TK_SHL_ASSIGN, TK_SHL, '<');
        case '=': return test2(l, '=', TK_EQ, '=');
        case '>': return test4(l, '=', TK_GTE, '>',
                                  '=', TK_SHR_ASSIGN, TK_SHR, '>');
        case '^': return test2(l, '=', TK_XOR_ASSIGN, '^');
        case '|': return test3(l, '=', TK_OR_ASSIGN, '|', TK_OR, '|');
        case '~': return test2(l, '=', TK_NOT_ASSIGN, '~');
        case '#': case '(': case ')': case ',': case ':':
        case ';': case '?': case ']': case '[': case '{':
        case '}': {
            return *l->p;
        }
        default:
            break;
        }
    }
}

int lex_next(lexer_t *l) {
    // If a lookahead token already exists (not always the case),
    // simply assign the current token to the lookahead token.
    if (*l->p == '\0')
        return 1;
    if (l->lookahead.token != 0) {
        l->token = l->lookahead;
        l->lookahead.token = 0;
    } else {
        l->token.token = tokenize(l);
    }
    return 0;
}
