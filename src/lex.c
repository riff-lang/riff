#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"
#include "mem.h"
#include "util.h"

#define adv (++x->p)

static void err(rf_lexer *x, const char *msg) {
    fprintf(stderr, "riff: [lex] line %d: %s\n", x->ln, msg);
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

static int read_flt(rf_lexer *x, rf_token *tk, const char *start, int base) {
    char *end;
    tk->lexeme.f = u_str2d(start, &end, base);
    x->p = end;
    return TK_FLT;
}

static int read_int(rf_lexer *x, rf_token *tk, const char *start, int base) {
    char *end;
    errno = 0;
    int64_t i = u_str2i64(start, &end, base);
    // Interpret as float in the event of overflow
    if (errno == ERANGE || isdigit(*end))
        return read_flt(x, tk, start, base);
    if (*end == '.') {
        if ((base == 10 && isdigit(end[1])) ||
            (base == 16 && isxdigit(end[1])))
            return read_flt(x, tk, start, base);
    } else if (base == 10 && (*end == 'e' || *end == 'E')) {
        if (end[1] == '+' || end[1] == '-' || isdigit(end[1]))
            return read_flt(x, tk, start, base);
    } else if (base == 16 && (*end == 'p' || *end == 'P')) {
        if (end[1] == '+' || end[1] == '-' || isxdigit(end[1]))
            return read_flt(x, tk, start, base);
    }

    x->p = end;
    tk->lexeme.i = i;
    return TK_INT;
}

static int read_num(rf_lexer *x, rf_token *tk) {
    const char *start = x->p - 1;

    // Quickly evaluate floating-point numbers with no integer part
    // before the decimal mark, e.g. `.12`
    if (*start == '.')
        return read_flt(x, tk, start, 0);

    int base = 10;
    if (*start == '0') {
        if (*x->p == 'x' || *x->p == 'X') {
            base = 16;
        } else if (*x->p == 'b' || *x->p == 'B') {
            base = 2;
        }
    }

    return read_int(x, tk, start, base);
}

// TODO?
// Currently reads a maximum of two hex digits (like Lua). C reads hex
// digits until it reaches a non-hex digit.
static int hex_esc(rf_lexer *x) {
    if (!isxdigit(*x->p))
        err(x, "expected hexadecimal digit");
    int e = u_hexval(*x->p++);
    if (isxdigit(*x->p)) {
        e <<= 4;
        e += u_hexval(*x->p++);
    }
    return e;
}

// Reads a maximum of three decimal digits. Throws an error if
// resulting number > 255.
static int dec_esc(rf_lexer *x) {
    int e = u_decval(*x->p++);
    for (int i = 0; i < 2; ++i) {
        if (!isdigit(*x->p))
            break;
        e *= 10;
        e += u_decval(*x->p++);
    }
    if (e > UCHAR_MAX)
        err(x, "invalid decimal escape");
    return e;
}

static int read_charint(rf_lexer *x, rf_token *tk) {
    int c = *x->p;
    switch (c) {
    case '\\':
        adv;
        switch (*x->p) {
        case 'a':  adv; c = '\a';       break;
        case 'b':  adv; c = '\b';       break;
        case 'e':  adv; c = 0x1b;       break; // Escape char
        case 'f':  adv; c = '\f';       break;
        case 'n':  adv; c = '\n';       break;
        case 'r':  adv; c = '\r';       break;
        case 't':  adv; c = '\t';       break;
        case 'v':  adv; c = '\v';       break;
        case 'x':  adv; c = hex_esc(x); break;
        case '\\': adv; c = '\\';       break; 
        case '\'': adv; c = '\'';       break; 
        default:        c = dec_esc(x); break;
        }
        break;
    default:
        if (c & 0x80) {
            char *end;
            c = u_utf82unicode(x->p, &end);
            x->p = end;
            if (c < 0)
                err(x, "invalid unicode character");
        } else {
            adv;
        }
        break;
    }
    if (*x->p++ != '\'')
        err(x, "expected `'` following character literal");
    tk->lexeme.i = c;
    return TK_INT;
}

static void unicode_esc(rf_lexer *x, int len) {
    uint32_t c = 0;
    for (int i = 0; i < len && isxdigit(*x->p); ++i) {
        c <<= 4;
        c += u_hexval(*x->p++);
    }
    char buf[8];
    int n = 8 - u_unicode2utf8(buf, c);
    for (; n < 8; ++n) {
        m_growarray(x->buf.c, x->buf.n, x->buf.cap, x->buf.c);
        x->buf.c[x->buf.n++] = buf[n];
    }
}

// TODO handling of multi-line string literals
static int read_str(rf_lexer *x, rf_token *tk, int d) {
    x->buf.n = 0;
    int c;
str_start:
    while ((c = *x->p) != d) {
        switch(c) {
        case '\\':
            adv;
            switch (*x->p) {
            case 'a': adv; c = '\a';       break;
            case 'b': adv; c = '\b';       break;
            case 'e': adv; c = 0x1b;       break; // Escape char
            case 'f': adv; c = '\f';       break;
            case 'n': adv; c = '\n';       break;
            case 'r': adv; c = '\r';       break;
            case 't': adv; c = '\t';       break;
            case 'v': adv; c = '\v';       break;
            case 'x': adv; c = hex_esc(x); break;

            case 'u': adv; unicode_esc(x, 4); goto str_start;
            case 'U': adv; unicode_esc(x, 8); goto str_start;

            // Ignore newlines following `\`
            case '\n': case '\r':
                ++x->ln;
                adv;
                goto str_start;
            case '\\': case '\'': case '"':
                c = *x->p;
                adv;
                break;
            default:
                c = dec_esc(x);
                break;
            }
            break;

        // Newlines inside quoted string literal
        case '\n': case '\r':
            ++x->ln;
            c = '\n';
            adv;
            break;
        case '\0':
            err(x, "reached end of input with unterminated string");
        default:
            adv;
            break;
        }
        m_growarray(x->buf.c, x->buf.n, x->buf.cap, x->buf.c);
        x->buf.c[x->buf.n++] = c;
    }

    rf_str *s = s_newstr(x->buf.c, x->buf.n, 1);
    adv;
    tk->lexeme.s = s;
    return TK_STR;
}

static int read_re(rf_lexer *x, rf_token *tk, int d) {
    x->buf.n = 0;
    int c;
re_start:
    while ((c = *x->p) != d) {
        switch(c) {
        case '\\':
            adv;
            switch (*x->p) {
            // Notably absent escape sqeunces:
            //   b     (word boundary)
            //   x     (hex escapes)
            //   <nnn> (octal escapes/backreferences)
            //
            // Even though Riff supports decimal escapes in the style
            // of `\nnn` in string/char literals, PCRE2 parses them as
            // octal (Perl). Unfortunately there's no way to override
            // this and hardcode decimal escape parsing without
            // potentially breaking a backreference provided by the
            // user. The only workaround might be to modify PCRE2
            // source code.
            //
            // NOTE: PCRE2 can lex the following as well
            case 'a': adv; c = '\a'; break;
            case 'e': adv; c = 0x1b; break; // Escape char
            case 'f': adv; c = '\f'; break;
            case 'n': adv; c = '\n'; break;
            case 'r': adv; c = '\r'; break;
            case 't': adv; c = '\t'; break;
            case 'v': adv; c = '\v'; break;

            case 'u': adv; unicode_esc(x, 4); goto re_start;
            case 'U': adv; unicode_esc(x, 8); goto re_start;
            // Ignore newlines following `\`
            case '\n': case '\r':
                ++x->ln;
                adv;
                goto re_start;
            default:
                m_growarray(x->buf.c, x->buf.n, x->buf.cap, x->buf.c);
                x->buf.c[x->buf.n++] = c;
                if (*x->p == d) {
                    m_growarray(x->buf.c, x->buf.n, x->buf.cap, x->buf.c);
                    x->buf.c[x->buf.n++] = d;
                    adv;
                }
                goto re_start;
            }
            break;

        // Newlines inside regex literal
        case '\n': case '\r':
            ++x->ln;
            c = '\n';
            adv;
            break;
        case '\0':
            err(x, "reached end of input with unterminated regular expression");
        default:
            adv;
            break;
        }
        m_growarray(x->buf.c, x->buf.n, x->buf.cap, x->buf.c);
        x->buf.c[x->buf.n++] = c;
        adv;
    }
    // Null terminate the RE string
    x->buf.c[x->buf.n] = 0;
    int flags = 0;
    adv;

    // Parse regex options (i,m,x)
    while (*x->p == 'i' || *x->p == 'm' || *x->p == 's' ||*x->p == 'x') {
        switch (*x->p) {
        case 'i': flags |= RE_ICASE;     adv; break;
        case 'm': flags |= RE_MULTILINE; adv; break;
        case 's': flags |= RE_DOTALL;    adv; break;
        // Superfluous, but valid
        case 'x': flags |= RE_EXTENDED; adv; break;
        }
    }
    rf_re *r = re_compile(x->buf.c, flags);
    tk->lexeme.r = r;
    return TK_RE;
}

static int check_kw(rf_lexer *x, const char *s, int size) {
    int f = size;     // Character immediately following
    int i = 0;
    for (; i < size; ++i)
        if (x->p[i] != s[i] || x->p[i] == '\0')
            break;
    size -= i;
    return !size && !valid_alphanum(x->p[f]);
}

static int read_id(rf_lexer *x, rf_token *tk) {
    const char *start = x->p - 1;
    // Check for reserved keywords
    switch (*start) {
    case 'b':
        if (check_kw(x, "reak", 4)) {
            x->p += 4;
            return TK_BREAK;
        } else break;
    case 'c':
        if (check_kw(x, "ontinue", 7)) {
            x->p += 7;
            return TK_CONT;
        } else break;
    case 'd':
        if (check_kw(x, "o", 1)) {
            x->p += 1;
            return TK_DO;
        } else break;
    case 'e':
        switch (*x->p) {
        case 'l':
            if (check_kw(x, "lif", 3)) {
                x->p += 3;
                return TK_ELIF;
            } else if (check_kw(x, "lse", 3)) {
                x->p += 3;
                return TK_ELSE;
            } else
                break;
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
        } else if (check_kw(x, "n", 1)) {
            x->p += 1;
            return TK_IN;
        } else break;
    case 'l':
        if (check_kw(x, "ocal", 4)) {
            x->p += 4;
            return TK_LOCAL;
        } else if (check_kw(x, "oop", 3)) {
            x->p += 3;
            return TK_LOOP;
        } else break;
    case 'n':
        if (check_kw(x, "ull", 3)) {
            x->p += 3;
            return TK_NULL;
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
        ++count;
        adv;
    }
    rf_str *s = s_newstr(start, count, 1);
    tk->lexeme.s = s;
    return TK_ID;
}

static int test2(rf_lexer *x, int c, int t1, int t2) {
    if (*x->p == c) {
        adv;
        return t1;
    } else
        return t2;
}

static int test3(rf_lexer *x, int c1, int t1, int c2, int t2, int t3) {
    if (*x->p == c1) {
        adv;
        return t1;
    } else if (*x->p == c2) {
        adv;
        return t2;
    } else
        return t3;
}

static int
test4(rf_lexer *x, int c1, int t1, int c2, int c3, int t2, int t3, int t4) {
    if (*x->p == c1) {
        adv;
        return t1;
    } else if (*x->p == c2) {
        adv;
        if (*x->p == c3) {
            adv;
            return t2;
        } else
            return t3;
    } else
        return t4;
}

static void line_comment(rf_lexer *x) {
    while (1) {
        if (*x->p == '\n' || *x->p == '\0')
            break;
        else
            adv;
    }
}

static void block_comment(rf_lexer *x) {
    while (1) {
        if (*x->p == '\n')
            x->ln++;
        if (*x->p == '\0')
            err(x, "reached end of input with unterminated block comment");
        else if (*x->p == '*') {
            adv;
            if (*x->p == '/') {
                adv;
                break;
            }
        }
        else adv;
    }
}

static int tokenize(rf_lexer *x, rf_token *tk) {
    int c;
    while (1) {
        switch (c = *x->p++) {
        case '\0': return 1;
        case '\n': case '\r': ++x->ln;
        case ' ': case '\t': break;
        case '!':
            if (x->mode) {
                return test3(x, '=', TK_NE, '~', TK_NMATCH, '!');
            } else {
                return test2(x, '=', TK_NE, '!');
            }
        case '"': return read_str(x, tk, c);
        case '#': return test2(x, '=', TK_CATX, '#');
        case '%': return test2(x, '=', TK_MODX, '%');
        case '&': return test3(x, '=', TK_ANDX, '&', TK_AND, '&');
        case '*': return test4(x, '=', TK_MULX, '*', 
                                  '=', TK_POWX, TK_POW, '*');
        case '+':
            if (x->mode)
                return test3(x, '=', TK_ADDX, '+', TK_INC, '+');
            else {
                if (isdigit(*x->p) || *x->p == '.')
                    return read_num(x, tk);
                else
                    return test3(x, '=', TK_ADDX, '+', TK_INC, '+');
            }
            break;
        case '-':
            if (x->mode)
                return test3(x, '=', TK_SUBX, '-', TK_DEC, '-');
            else {
                if (isdigit(*x->p) || *x->p == '.')
                    return read_num(x, tk);
                else
                    return test3(x, '=', TK_SUBX, '-', TK_DEC, '-');
            }
            break;
        case '.':
            if (isdigit(*x->p))
                return read_num(x, tk);
            else if ((*x->p) == '.') {
                adv;
                return TK_DOTS;
            } else
                return '.';
        case '/':
            switch (*x->p) {
            case '/': adv; line_comment(x);  break;
            case '*': adv; block_comment(x); break;
            default: 
                if (x->mode)
                    return test2(x, '=', TK_DIVX, '/');
                else
                    return read_re(x, tk, c);
                break;
            }
            break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return read_num(x, tk);
        case '<': return test4(x, '=', TK_LE, '<',
                                  '=', TK_SHLX, TK_SHL, '<');
        case '=': return test2(x, '=', TK_EQ, '=');
        case '>': return test4(x, '=', TK_GE, '>',
                                  '=', TK_SHRX, TK_SHR, '>');
        case '^': return test2(x, '=', TK_XORX, '^');
        case '|': return test3(x, '=', TK_ORX, '|', TK_OR, '|');
        case '\'': return read_charint(x, tk);
        case '$': case '(': case ')': case ',': case ':':
        case ';': case '?': case ']': case '[': case '{':
        case '}': case '~':
            return c;
        default:
            if (valid_alpha(c)) {
                return read_id(x, tk);
            } else {
                err(x, "invalid token");
            }
        }
    }
}

int x_init(rf_lexer *x, const char *src) {
    x->mode    = 0;
    x->ln      = 1;
    x->p       = src;
    x->tk.kind = 0;
    x->la.kind = 0;
    x->buf.n   = 0;
    x->buf.cap = 0;
    x->buf.c   = NULL;
    x_adv(x);
    return 0;
}

// Lexer itself should be stack-allocated, but the string buffer is
// heap-allocated
void x_free(rf_lexer *x) {
    if (x->buf.c != NULL)
        free(x->buf.c);
}

int x_adv(rf_lexer *x) {

    // Free previous string object
    if (x->tk.kind == TK_STR || x->tk.kind == TK_ID) {
        m_freestr(x->tk.lexeme.s);
    } else if (x->tk.kind == TK_EOI)
        return 1;

    // If a lookahead token already exists, assign it to the current
    // token
    if (x->la.kind != 0) {
        x->tk.kind   = x->la.kind;
        x->tk.lexeme = x->la.lexeme;
        x->la.kind   = 0;
    } else if ((x->tk.kind = tokenize(x, &x->tk)) == 1) {
        x->tk.kind = TK_EOI;
        return 1;
    }
    return 0;
}

// Populate the lookahead rf_token, leaving the current rf_token
// unchanged
int x_peek(rf_lexer *x) {
    if ((x->la.kind = tokenize(x, &x->la)) == 1) {
        x->la.kind = TK_EOI;
        return 1;
    }
    return 0;
}

#undef adv
