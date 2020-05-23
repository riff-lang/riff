#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "lex.h"

#define next(x) (x->p++)

static void err(lexer_t *x, const char *msg) {
    fprintf(stderr, "Lexical error at or near line %d: %s\n", x->ln, msg);
    exit(1);
}

static bool valid_alpha(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c == '_');
}

// Incomplete
static void read_char(lexer_t *x) {
    char c;
    if (*x->p == '\\') {
        next(x);
        switch (*x->p) {
        case 'n': c = '\n'; break;
        default:  err(x, "Invalid escape sequence");
        }
    } else
        c = *x->p;
    x->tk.lexeme.c = c;
    next(x);
    if (*x->p != '\'')
        err(x, "Invalid character definition; "
               "Use double quotation marks (\"\") to define strings");
    next(x);
}


static int read_flt(lexer_t *x, const char *start, char *end) {
    double d = strtod(start, &end);
    x->p = end;
    x->tk.lexeme.f = d;
    return TK_FLT;
}

static int read_int(lexer_t *x, const char *start, char *end, int base) {
    int64_t i = strtoull(start, &end, base);
    x->p = end;
    x->tk.lexeme.i = i;
    return TK_INT;
}

// Incomplete, obvs
static int read_num(lexer_t *x) {
    const char *start = x->p - 1;
    char *end;

    // Quickly evaluate floating-point numbers with no integer part
    // before the decimal mark, e.g. `.12`
    if (*start == '.')
        return read_flt(x, start, end);

    int base;
    if (*start == '0') {
        if (*x->p == 'x' || *x->p == 'X') {
            base = 16;
            next(x);
        } else if (*x->p == 'b' || *x->p == 'B') {
            base   = 2;
            start += 2; // strtoull() handles 0x__ but not 0b__
            next(x);
        }
    } else
        base = 10;
    
    int c;
    while (1) {
        c = *x->p;
        // printf("%c\n", c);
        if (c == '.') {
            return read_flt(x, start, end);
        } else {
            switch (base) {
            case 2:
                // if (c < '0' || c > '1')
                    // num_err(x, "Invalid binary number");
                // else
                    break;
            case 10:
                if (!isdigit(c))
                    return read_int(x, start, end, base);
                else
                    break;
            case 16:
                if (!isxdigit(c))
                    return read_int(x, start, end, base);
                else
                    break;
            default: break;
            }
        }
        next(x);
    }
    return 0;
}

static int test2(lexer_t *x, int c, int t1, int t2) {
    if (*x->p == c) {
        next(x);
        return t1;
    } else
        return t2;
}

static int test3(lexer_t *x, int c1, int t1, int c2, int t2, int t3) {
    if (*x->p == c1) {
        next(x);
        return t1;
    } else if (*x->p == c2) {
        next(x);
        return t2;
    } else
        return t3;
}

static int
test4(lexer_t *x, int c1, int t1, int c2, int c3, int t2, int t3, int t4) {
    if (*x->p == c1) {
        next(x);
        return t1;
    } else if (*x->p == c2) {
        next(x);
        if (*x->p == c3) {
            next(x);
            return t2;
        } else
            return t3;
    } else
        return t4;
}

static int tokenize(lexer_t *x) {
    int c;
    while (1) {
        switch (c = *x->p++) {
        case '\n': case '\r': x->ln++;
        case ' ': case '\t': break;
        case '!': return test2(x, '=', TK_NEQ, '!');
        case '"':
            // Read string
            exit(1);
        case '%': return test2(x, '=', TK_MOD_ASSIGN, '%');
        case '&': return test3(x, '=', TK_AND_ASSIGN, '&', TK_AND, '&');
        case '\'':
            read_char(x);
            return TK_CHAR;
        case '*': return test4(x, '=', TK_MUL_ASSIGN, '*', 
                                  '=', TK_POW_ASSIGN, TK_POW, '*');
        case '+': return test3(x, '=', TK_ADD_ASSIGN, '+', TK_INC, '+');
        case '-': return test3(x, '=', TK_SUB_ASSIGN, '-', TK_DEC, '-');
        case '.':
            // ., .., ..., floating-point number
            if (isdigit(*x->p))
                return read_num(x);
            else
                exit(1);
        case '/':
            // /, /=, //
            exit(1);
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return read_num(x);
        case '<': return test4(x, '=', TK_LTE, '<',
                                  '=', TK_SHL_ASSIGN, TK_SHL, '<');
        case '=': return test2(x, '=', TK_EQ, '=');
        case '>': return test4(x, '=', TK_GTE, '>',
                                  '=', TK_SHR_ASSIGN, TK_SHR, '>');
        case '^': return test2(x, '=', TK_XOR_ASSIGN, '^');
        case '|': return test3(x, '=', TK_OR_ASSIGN, '|', TK_OR, '|');
        case '~': return test2(x, '=', TK_NOT_ASSIGN, '~');
        case '#': case '(': case ')': case ',': case ':':
        case ';': case '?': case ']': case '[': case '{':
        case '}': {
            return c;
        }
        default:
            exit(1);
        }
    }
}

int x_next(lexer_t *x) {
    // If a la token already exists (not always the case),
    // simply assign the current token to the la token.
    if (*x->p == '\0')
        return 1;
    if (x->la.token != 0) {
        x->tk = x->la;
        x->la.token = 0;
        x->la.lexeme.c = '\0';
    } else {
        x->tk.token = tokenize(x);
    }
    return 0;
}
