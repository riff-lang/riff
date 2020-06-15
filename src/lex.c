#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static bool valid_alphanum(char c) {
    return valid_alpha(c) || isdigit(c);
}

static int read_flt(lexer_t *x, const char *start) {
    char *end;
    double d = strtod(start, &end);
    x->p = end;
    x->tk.lexeme.f = d;
    return TK_FLT;
}

static int read_int(lexer_t *x, const char *start, int base) {
    char *end;
    int64_t i = strtoull(start, &end, base);
    x->p = end;
    x->tk.lexeme.i = i;
    return TK_INT;
}

// Incomplete
// - needs to properly handle binary numbers (e.g. 0b1101)
// - possibly allow unsigned integers with the `u` suffix.
static int read_num(lexer_t *x) {
    const char *start = x->p - 1;

    // Quickly evaluate floating-point numbers with no integer part
    // before the decimal mark, e.g. `.12`
    if (*start == '.')
        return read_flt(x, start);

    int base = 10;
    if (*start == '0') {
        if (*x->p == 'x' || *x->p == 'X') {
            base = 16;
            next(x);
        }
    }

    int c;
    while (1) {
        c = *x->p;
        if (c == '.') {
            return read_flt(x, start);
        } else {
            switch (base) {
            case 10:
                if (!isdigit(c)) {
                    if (valid_alpha(c))
                        err(x, "Malformed number");
                    else
                        return read_int(x, start, base);
                }
                else
                    break;
            case 16:
                if (!isxdigit(c)) {
                    if (valid_alpha(c))
                        err(x, "Malformed number");
                    else
                        return read_int(x, start, base);
                }
                else
                    break;
            default: break;
            }
        }
        next(x);
    }
    return 0;
}

// Possibly needs to ignore single backslashes
static int read_str(lexer_t *x) {
    const char *start = x->p;
    str_t s;
    size_t count = 0;
    int c;
    while (c != '"') {
        c = *x->p;
        switch(c) {
        case '\\': next(x); next(x); count += 2; break;
        case '"': 
            s_newstr(&s, start, count);
            next(x);
            x->tk.lexeme.s = &s;
            return TK_STR;
        case '\0':
            err(x, "Reached end of input; unterminated string");
        default: next(x); count++; break;
        }
    }
    return TK_STR;
}

static int check_kw(lexer_t *x, const char *s, int size) {
    int f = *(x->p + size); // Character immediately following
    return !memcmp(x->p, s, size) && !valid_alphanum(f);
}

static int read_id(lexer_t *x) {
    const char *start = x->p - 1;
    // Check for reserved keywords
    switch (*start) {
    case 'b':
        if (check_kw(x, "reak", 4)) {
            x->p += 4;
            return TK_BREAK;
        } else break;
    case 'e':
        switch (*x->p) {
        case 'l':
            if (check_kw(x, "lse", 3)) {
                x->p += 3;
                return TK_ELSE;
            } else break;
        case 'x':
            if (check_kw(x, "xit", 3)) {
                x->p += 3;
                return TK_EXIT;
            } else break;
        }
        break;
    case 'i':
        if (check_kw(x, "f", 1)) {
            x->p += 1;
            return TK_IF;
        } else break;
    case 'f':
        switch (*x->p) {
        case 'n':
            if (check_kw(x, "n", 1)) {
                x->p += 1;
                return TK_FN;
            } else break;
        case 'o':
            if (check_kw(x, "or", 2)) {
                x->p += 2;
                return TK_FOR;
            } else break;
        }
        break;
    case 'l':
        if (check_kw(x, "ocal", 4)) {
            x->p += 4;
            return TK_LOCAL;
        } else break;
    case 'p':
        if (check_kw(x, "rint", 4)) {
            x->p += 4;
            return TK_PRINT;
        } else break;
    case 'r':
        if (check_kw(x, "eturn", 5)) {
            x->p += 5;
            return TK_RETURN;
        } else break;
    case 'w':
        if (check_kw(x, "hile", 4)) {
            x->p += 4;
            return TK_WHILE;
        } else break;
    default: break;
    }

    // Otherwise, token is an identifier
    size_t count = 1;
    while (valid_alphanum(*x->p)) {
        count++;
        next(x);
    }
    str_t s;
    s_newstr(&s, start, count);
    x->tk.lexeme.s = &s;
    return TK_ID;
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
        case '\0': return 1;
        case '\n': case '\r': x->ln++;
        case ' ': case '\t': break;
        case '!': return test2(x, '=', TK_NE, '!');
        case '"': return read_str(x);
        case '%': return test2(x, '=', TK_MOD_ASSIGN, '%');
        case '&': return test3(x, '=', TK_AND_ASSIGN, '&', TK_AND, '&');
        case '*': return test4(x, '=', TK_MUL_ASSIGN, '*', 
                                  '=', TK_POW_ASSIGN, TK_POW, '*');
        case '+': return test3(x, '=', TK_ADD_ASSIGN, '+', TK_INC, '+');
        case '-': return test3(x, '=', TK_SUB_ASSIGN, '-', TK_DEC, '-');
        case '.': return isdigit(*x->p) ? read_num(x) : '.';
        case '/':
            if (*x->p == '/') {
                while (*x->p != '\n')
                    next(x);
                next(x);
                break;
            } else
                return test2(x, '=', TK_DIV_ASSIGN, '/');
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return read_num(x);
        case '<': return test4(x, '=', TK_LE, '<',
                                  '=', TK_SHL_ASSIGN, TK_SHL, '<');
        case '=': return test2(x, '=', TK_EQ, '=');
        case '>': return test4(x, '=', TK_GE, '>',
                                  '=', TK_SHR_ASSIGN, TK_SHR, '>');
        case '^': return test2(x, '=', TK_XOR_ASSIGN, '^');
        case '|': return test3(x, '=', TK_OR_ASSIGN, '|', TK_OR, '|');
        case ':':
            if (*x->p == ':') {
                next(x);
                return test2(x, '=', TK_CAT_ASSIGN, TK_CAT);
            } else
                return ':';
        case '#': case '(': case ')': case ',': case ';':
        case '?': case ']': case '[': case '{': case '}':
        case '~': 
            return c;
        default:
            if (valid_alpha(c)) {
                return read_id(x);
            } else {
                err(x, "Invalid token");
            }
        }
    }
}

int x_next(lexer_t *x) {
    // If a lookahead token already exists (not always the case),
    // simply assign the current token to the lookahead token.
    if (x->la.type != 0) {
        x->tk = x->la;
        x->la.type = 0;
        // x->la.lexeme.i = 0;
    } else if ((x->tk.type = tokenize(x)) == 1)
        return 1;
    return 0;
}

#undef next
