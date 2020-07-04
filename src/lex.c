#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"

#define adv(x) (x->p++)

static void err(lexer_t *x, const char *msg) {
    fprintf(stderr, "Lexical error at or near line %d: %s\n", x->ln, msg);
    exit(1);
}

static int valid_alpha(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c == '_');
}

static int valid_alphanum(char c) {
    return valid_alpha(c) || isdigit(c);
}

static int read_flt(lexer_t *x, token_t *tk, const char *start, int base) {
    char *end;
    if (base == 16)
        start -= 2;
    double d = strtod(start, &end);
    if (valid_alphanum(*end))
        err(x, "Invalid numeral");
    x->p = end;
    tk->lexeme.f = d;
    return TK_FLT;
}

static int read_int(lexer_t *x, token_t *tk, const char *start, int base) {
    char *end;
    uint64_t i = strtoull(start, &end, base);
    if (*end == '.') {
        if (base != 2)
            return read_flt(x, tk, start, base);
        else
            err(x, "Invalid numeral");
    }
    else if (valid_alphanum(*end))
        err(x, "Invalid numeral");

    // Interpret as float if base-10 int exceeds INT64_MAX, or if
    // there's overflow in general.
    // This is a hacky way of handling the base-10 INT64_MIN with a
    // leading unary minus sign, e.g. `-9223372036854775808`
    if ((base == 10 && i > INT64_MAX) || (errno == ERANGE))
        return read_flt(x, tk, start, base);
    x->p = end;
    tk->lexeme.i = i;
    return TK_INT;
}

// TODO Handle binary float literals? e.g. 0b1101.101
// TODO Allow underscores in numeric literals e.g. 123_456_789
static int read_num(lexer_t *x, token_t *tk) {
    const char *start = x->p - 1;

    // Quickly evaluate floating-point numbers with no integer part
    // before the decimal mark, e.g. `.12`
    if (*start == '.')
        return read_flt(x, tk, start, 10);

    int base = 10;
    if (*start == '0') {
        if (*x->p == 'x' || *x->p == 'X') {
            base   = 16;
            start += 2;
        }
        else if (*x->p == 'b' || *x->p == 'B') {
            base   = 2;
            start += 2;
        }
    }
    return read_int(x, tk, start, base);
}

// Possibly needs to ignore single backslashes
static int read_str(lexer_t *x, token_t *tk) {
    const char *start = x->p;
    size_t count = 0;
    int c = 0;
    while (c != '"') {
        c = *x->p;
        switch(c) {
        case '\\': adv(x); adv(x); count += 2; break;
        case '"': {
            str_t *s = s_newstr(start, count);
            adv(x);
            tk->lexeme.s = s;
            return TK_STR;
        }
        case '\0':
            err(x, "Reached end of input; unterminated string");
        default: adv(x); count++; break;
        }
    }
    return TK_STR;
}

static int check_kw(lexer_t *x, const char *s, int size) {
    int f = *(x->p + size); // Character immediately following
    return !memcmp(x->p, s, size) && !valid_alphanum(f);
}

static int read_id(lexer_t *x, token_t *tk) {
    const char *start = x->p - 1;
    // Check for reserved keywords
    switch (*start) {
    case 'b':
        if (check_kw(x, "reak", 4)) {
            x->p += 4;
            return TK_BREAK;
        } else break;
    case 'd':
        if (check_kw(x, "o", 1)) {
            x->p += 1;
            return TK_DO;
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
    case 'i':
        if (check_kw(x, "f", 1)) {
            x->p += 1;
            return TK_IF;
        } else break;
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
        adv(x);
    }
    str_t *s = s_newstr(start, count);
    tk->lexeme.s = s;
    return TK_ID;
}

static int test2(lexer_t *x, int c, int t1, int t2) {
    if (*x->p == c) {
        adv(x);
        return t1;
    } else
        return t2;
}

static int test3(lexer_t *x, int c1, int t1, int c2, int t2, int t3) {
    if (*x->p == c1) {
        adv(x);
        return t1;
    } else if (*x->p == c2) {
        adv(x);
        return t2;
    } else
        return t3;
}

static int
test4(lexer_t *x, int c1, int t1, int c2, int c3, int t2, int t3, int t4) {
    if (*x->p == c1) {
        adv(x);
        return t1;
    } else if (*x->p == c2) {
        adv(x);
        if (*x->p == c3) {
            adv(x);
            return t2;
        } else
            return t3;
    } else
        return t4;
}

static int tokenize(lexer_t *x, token_t *tk) {
    int c;
    while (1) {
        switch (c = *x->p++) {
        case '\0': return 1;
        case '\n': case '\r': x->ln++;
        case ' ': case '\t': break;
        case '!': return test2(x, '=', TK_NE, '!');
        case '"': return read_str(x, tk);
        case '%': return test2(x, '=', TK_MODASG, '%');
        case '&': return test3(x, '=', TK_ANDASG, '&', TK_AND, '&');
        case '*': return test4(x, '=', TK_MULASG, '*', 
                                  '=', TK_POWASG, TK_POW, '*');
        case '+': return test3(x, '=', TK_ADDASG, '+', TK_INC, '+');
        case '-': return test3(x, '=', TK_SUBASG, '-', TK_DEC, '-');
        case '.': return isdigit(*x->p) ? read_num(x, tk) : '.';
        case '/':
            if (*x->p == '/') {
                while (*x->p != '\n')
                    adv(x);
                adv(x);
                break;
            } else
                return test2(x, '=', TK_DIVASG, '/');
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return read_num(x, tk);
        case '<': return test4(x, '=', TK_LE, '<',
                                  '=', TK_SHLASG, TK_SHL, '<');
        case '=': return test2(x, '=', TK_EQ, '=');
        case '>': return test4(x, '=', TK_GE, '>',
                                  '=', TK_SHRASG, TK_SHR, '>');
        case '^': return test2(x, '=', TK_XORASG, '^');
        case '|': return test3(x, '=', TK_ORASG, '|', TK_OR, '|');
        case ':':
            if (*x->p == ':') {
                adv(x);
                return test2(x, '=', TK_CATASG, TK_CAT);
            } else
                return ':';
        case '#': case '(': case ')': case ',': case ';':
        case '?': case ']': case '[': case '{': case '}':
        case '~': 
            return c;
        default:
            if (valid_alpha(c)) {
                return read_id(x, tk);
            } else {
                err(x, "Invalid token");
            }
        }
    }
}

int x_init(lexer_t *x, const char *src) {
    x->ln = 1;
    x->p  = src;
    x->tk.kind = 0;
    x->la.kind = 0;
    x_adv(x);
    return 0;
}

int x_adv(lexer_t *x) {
    // TODO make sure this works properly
    // Free previous string object
    if (x->tk.kind == TK_STR || x->tk.kind == TK_ID)
        free(x->tk.lexeme.s);
    else if (x->tk.kind == TK_EOI)
        return 1;
    // If a lookahead token already exists, assign it to the current
    // token
    if (x->la.kind != 0) {
        x->tk = x->la;
        x->la.kind = 0;
        // x->la.lexeme.i = 0;
    } else if ((x->tk.kind = tokenize(x, &x->tk)) == 1) {
        x->tk.kind = TK_EOI;
        return 1;
    }
    return 0;
}

// Populate the lookahead token_t, leaving the current token_t
// unchanged
int x_peek(lexer_t *x) {
    if ((x->la.kind = tokenize(x, &x->la)) == 1) {
        x->la.kind = TK_EOI;
        return 1;
    }
    return 0;
}

#undef adv
