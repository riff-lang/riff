#include "lex.h"

#include "mem.h"
#include "string.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define advance() (++x->p)
#define decval(c) (c - '0')
#define hexval(c) (isdigit(c) ? decval(c) : tolower(c) - 'a' + 10)

static void err(riff_lexer *x, const char *msg) {
    fprintf(stderr, "riff: [lex] line %d: %s\n", x->ln, msg);
    exit(1);
}

static int isoctal(char c) {
    return c >= '0' && c <= '7';
}

static int valid_alpha(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c == '_');
}

static int valid_alphanum(char c) {
    return valid_alpha(c) || isdigit(c);
}

static int valid_regex_flag(char c) {
    return riff_strchr("ADJUimnsux", c);
}

static int float_literal(riff_lexer *x, riff_token *tk, const char *start, int base) {
    char *end;
    tk->f = riff_strtod(start, &end, base);
    x->p = end;
    return TK_FLOAT;
}

static int int_literal(riff_lexer *x, riff_token *tk, const char *start, int base) {
    char *end;
    errno = 0;
    int64_t i = riff_strtoll(start, &end, base);

    // Interpret as float in the event of overflow
    if (errno == ERANGE || isdigit(*end)) {
        return float_literal(x, tk, start, base);
    }

    // Extra float conditions
    if (*end == '.') {
        if (end[1] != '.') {
            return float_literal(x, tk, start, base);
        }
    } else if (base == 10 && (*end == 'e' || *end == 'E')) {
        if (end[1] == '+' || end[1] == '-' || isdigit(end[1])) {
            return float_literal(x, tk, start, base);
        }
    } else if (base == 16 && (*end == 'p' || *end == 'P')) {
        if (end[1] == '+' || end[1] == '-' || isxdigit(end[1])) {
            return float_literal(x, tk, start, base);
        }
    }
    x->p = end;
    tk->i = i;
    return TK_INT;
}

static int num_literal(riff_lexer *x, riff_token *tk) {
    const char *start = x->p - 1;

    // Quickly evaluate floating-point numbers with no integer part before the
    // decimal mark, e.g. `.12`
    if (*start == '.') {
        return float_literal(x, tk, start, 0);
    }

    int base = 10;
    if (*start == '0') {
        if (*x->p == 'x' || *x->p == 'X') {
            base = 16;
        } else if (*x->p == 'b' || *x->p == 'B') {
            base = 2;
        }
    } else if (*start == '+' || *start == '-') {
        if (start[1] == '0') {
            if (start[2] == 'x' || start[2] == 'X') {
                base = 16;
            } else if (start[2] == 'b' || start[2] == 'B') {
                base = 2;
            }
        }
    }
    return int_literal(x, tk, start, base);
}

// TODO?
// Currently reads a maximum of two hex digits (like Lua). C reads hex digits
// until it reaches a non-hex digit.
static int hex_esc(riff_lexer *x) {
    if (!isxdigit(*x->p)) {
        err(x, "expected hexadecimal digit");
    }
    int e = hexval(*x->p++);
    if (isxdigit(*x->p)) {
        e <<= 4;
        e += hexval(*x->p++);
    }
    return e;
}

// Reads a maximum of three octal digits. Throws an error if resulting number >
// 255.
static int oct_esc(riff_lexer *x) {
    if (!isoctal(*x->p)) {
        err(x, "expected octal digit");
    }
    int e = decval(*x->p++);
    for (int i = 0; i < 2; ++i) {
        if (!isoctal(*x->p)) {
            break;
        }
        e *= 8;
        e += decval(*x->p++);
    }
    if (e > UCHAR_MAX) {
        err(x, "invalid octal escape");
    }
    return e;
}

static uint32_t unicode_int(riff_lexer *x, int len) {
    uint32_t c = 0;
    for (int i = 0; i < len && isxdigit(*x->p); ++i) {
        c <<= 4;
        c += hexval(*x->p++);
    }
    return c;
}

static void unicode_esc(riff_lexer *x, int len) {
    uint32_t c = unicode_int(x, len);
    char buf[8];
    int n = 8 - riff_unicodetoutf8(buf, c);
    for (; n < 8; ++n) {
        m_growarray(x->buf.c, x->buf.n, x->buf.cap, x->buf.c);
        x->buf.c[x->buf.n++] = buf[n];
    }
}

static int char_literal(riff_lexer *x, riff_token *tk, int d) {
    int c;
    riff_int v = 0;
    while ((c = *x->p) != d) {

        // NOTE: check for (v & (0xFFull << 55) to assert a multicharacter
        // sequence contains only 64 bits worth of characters. Otherwise, the
        // resulting value will simply be the lowest 64 bits of the sequence.

        v <<= 8;
        switch (c) {
        case '\\':
            advance();
            switch (*x->p) {
            case 'a':  advance(); v += '\a';       break;
            case 'b':  advance(); v += '\b';       break;
            case 'e':  advance(); v += 0x1b;       break; // Escape char
            case 'f':  advance(); v += '\f';       break;
            case 'n':  advance(); v += '\n';       break;
            case 'r':  advance(); v += '\r';       break;
            case 't':  advance(); v += '\t';       break;
            case 'v':  advance(); v += '\v';       break;
            case 'x':  advance(); v += hex_esc(x); break;
            case '\\': advance(); v += '\\';       break; 
            case '\'': advance(); v += '\'';       break; 
            case 'u': {
                advance();
                uint32_t u = unicode_int(x, 4);
                v <<= u & 0xff00 ? 8 : 0;
                v += u;
                break;
            }
            case 'U': {
                advance();
                uint32_t u = unicode_int(x, 8);
                v <<= u & 0xff000000 ? 24 :
                      u & 0x00ff0000 ? 16 :
                      u & 0x0000ff00 ?  8 : 0;
                v += u;
                break;
            }
            default:
               v += oct_esc(x);
               break;
            }
            break;
        case '\0':
            err(x, "reached end of input with unterminated character literal");
        default:
            if (c & 0x80) {
                char *end;
                riff_int u = riff_utf8tounicode(x->p, &end);
                v <<= u & 0xff000000 ? 24 :
                      u & 0x00ff0000 ? 16 :
                      u & 0x0000ff00 ?  8 : 0;
                v += u;
                x->p = end;
                if (u < 0)
                    err(x, "invalid unicode character");
            } else {
                advance();
                v += c;
            }
            break;
        }
    }
    advance();
    tk->i = v;
    return TK_INT;
}

// TODO handling of multi-line string literals
static int str_literal(riff_lexer *x, riff_token *tk, int d) {
    x->buf.n = 0;
    int c;
str_start:
    while ((c = *x->p) != d) {
        switch(c) {
        case '\\':
            advance();
            switch (*x->p) {
            case 'a': advance(); c = '\a';       break;
            case 'b': advance(); c = '\b';       break;
            case 'e': advance(); c = 0x1b;       break; // Escape char
            case 'f': advance(); c = '\f';       break;
            case 'n': advance(); c = '\n';       break;
            case 'r': advance(); c = '\r';       break;
            case 't': advance(); c = '\t';       break;
            case 'v': advance(); c = '\v';       break;
            case 'x': advance(); c = hex_esc(x); break;

            case 'u': advance(); unicode_esc(x, 4); goto str_start;
            case 'U': advance(); unicode_esc(x, 8); goto str_start;

            // Ignore newlines following `\`
            case '\n':
            case '\r':
                ++x->ln;
                advance();
                goto str_start;
            case '\\':
            case '\'':
            case '"':
                c = *x->p;
                advance();
                break;
            default:
                c = oct_esc(x);
                break;
            }
            break;

        // Newlines inside quoted string literal
        case '\n':
        case '\r':
            ++x->ln;
            c = '\n';
            advance();
            break;
        case '\0':
            err(x, "reached end of input with unterminated string");
        default:
            advance();
            break;
        }
        m_growarray(x->buf.c, x->buf.n, x->buf.cap, x->buf.c);
        x->buf.c[x->buf.n++] = c;
    }
    riff_str *s = riff_str_new(x->buf.c, x->buf.n);
    advance();
    tk->s = s;
    return TK_STR;
}

static int regex_literal(riff_lexer *x, riff_token *tk, int d) {
    x->buf.n = 0;
    int c;
re_start:
    while ((c = *x->p) != d) {
        switch(c) {
        case '\\':
            advance();
            switch (*x->p) {
            case 'u': advance(); unicode_esc(x, 4); goto re_start;
            case 'U': advance(); unicode_esc(x, 8); goto re_start;
            // Ignore newlines following `\`
            case '\n':
            case '\r':
                ++x->ln;
                advance();
                goto re_start;
            case '/':
                c = *x->p;
                advance();
                break;
            default:
                // Copy over escape sequence unmodified - let PCRE2
                // handle it.
                m_growarray(x->buf.c, x->buf.n + 1, x->buf.cap, x->buf.c);
                x->buf.c[x->buf.n++] =  '\\';
                x->buf.c[x->buf.n++] =  *x->p;
                advance();
                goto re_start;
            }
            break;

        // Newlines inside regex literal
        case '\n':
        case '\r':
            ++x->ln;
            c = '\n';
            advance();
            break;
        case '\0':
            err(x, "reached end of input with unterminated regular expression");
        default:
            advance();
            break;
        }
        m_growarray(x->buf.c, x->buf.n, x->buf.cap, x->buf.c);
        x->buf.c[x->buf.n++] = c;
    }
    uint32_t flags = 0;
    advance();

    // Parse regex options ([ADJUimnsux]*)
    while (valid_regex_flag(*x->p)) {
        switch (*x->p) {
        case 'A': flags |= RE_ANCHORED;        break;
        case 'D': flags |= RE_DOLLAREND;       break;
        case 'J': flags |= RE_DUPNAMES;        break;
        case 'U': flags |= RE_UNGREEDY;        break;
        case 'i': flags |= RE_ICASE;           break;
        case 'm': flags |= RE_MULTILINE;       break;
        case 'n': flags |= RE_NO_AUTO_CAPTURE; break;
        case 's': flags |= RE_DOTALL;          break;
        case 'u': flags |= RE_UNICODE;         break;
        case 'x':
            if (x->p[1] == 'x') {
                flags |= RE_EXTENDED_MORE;
                advance();
            } else {
                flags |= RE_EXTENDED;
            }
            break;
        }
        advance();
    }
    int errcode;
    riff_regex *r = re_compile(x->buf.c, x->buf.n, flags, &errcode);

    // Regex compilation error handling
    if (errcode != 100) {
        PCRE2_UCHAR errstr[0x200];
        pcre2_get_error_message(errcode, errstr, 0x200);
        err(x, (const char *) errstr);
    }
    tk->r = r;
    return TK_REGEX;
}

static int ident_or_reserved(riff_lexer *x, riff_token *tk) {
    const char *start = x->p - 1;
    size_t count = 1;
    while (valid_alphanum(*x->p)) {
        ++count;
        advance();
    }
    tk->s = riff_str_new(start, count);
    return tk->s->extra ? tk->s->extra : TK_IDENT;
}

static int check2(riff_lexer *x, int c, int t1, int t2) {
    if (*x->p == c) {
        advance();
        return t1;
    } else {
        return t2;
    }
}

static int check3(riff_lexer *x, int c1, int t1, int c2, int t2, int t3) {
    if (*x->p == c1) {
        advance();
        return t1;
    } else if (*x->p == c2) {
        advance();
        return t2;
    } else {
        return t3;
    }
}

static int
check4(riff_lexer *x, int c1, int t1, int c2, int c3, int t2, int t3, int t4) {
    if (*x->p == c1) {
        advance();
        return t1;
    } else if (*x->p == c2) {
        advance();
        if (*x->p == c3) {
            advance();
            return t2;
        } else {
            return t3;
        }
    } else {
        return t4;
    }
}

static void line_comment(riff_lexer *x) {
    while (1) {
        if (*x->p == '\n' || *x->p == '\0') {
            break;
        } else {
            advance();
        }
    }
}

static void block_comment(riff_lexer *x) {
    while (1) {
        if (*x->p == '\n') {
            x->ln++;
        }
        if (*x->p == '\0') {
            err(x, "reached end of input with unterminated block comment");
        } else if (*x->p == '*') {
            advance();
            if (*x->p == '/') {
                advance();
                break;
            }
        } else {
            advance();
        }
    }
}

static int tokenize(riff_lexer *x, int mode, riff_token *tk) {
    int c;
    while (1) {
        switch (c = *x->p++) {
        case '\0': return 1;
        case '\n': case '\r': ++x->ln;
        case ' ': case '\t': break;
        case '!':
            if (mode) {
                return check3(x, '=', TK_NE, '~', TK_NMATCH, '!');
            } else {
                return check2(x, '=', TK_NE, '!');
            }
        case '"': return str_literal(x, tk, c);
        case '#': return check2(x, '=', TK_CATX, '#');
        case '%': return check2(x, '=', TK_MODX, '%');
        case '&': return check3(x, '=', TK_ANDX, '&', TK_AND, '&');
        case '*': return check4(x, '=', TK_MULX, '*', 
                                   '=', TK_POWX, TK_POW, '*');
        case '+':
            if (mode) {
                return check3(x, '=', TK_ADDX, '+', TK_INC, '+');
            } else {
                // Allow `+` to be consumed when directly prefixing a numeric
                // literal. This has no effect on operator precedence (unlike
                // `-`) since the result of any expression is the same
                // regardless. All this really does is save an OP_NUM byte from
                // being emitted.
                if (isdigit(*x->p) || *x->p == '.') {
                    return num_literal(x, tk);
                } else {
                    return check3(x, '=', TK_ADDX, '+', TK_INC, '+');
                }
            }
            break;
        case '-': return check3(x, '=', TK_SUBX, '-', TK_DEC, '-');
        case '.':
            if (isdigit(*x->p)) {
                return num_literal(x, tk);
            } else if ((*x->p) == '.') {
                advance();
                return TK_2DOTS;
            } else {
                return '.';
            }
        case '/':
            switch (*x->p) {
            case '/': advance(); line_comment(x);  break;
            case '*': advance(); block_comment(x); break;
            default: 
                if (mode) {
                    return check2(x, '=', TK_DIVX, '/');
                } else {
                    return regex_literal(x, tk, c);
                }
                break;
            }
            break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return num_literal(x, tk);
        case '<': return check4(x, '=', TK_LE, '<',
                                   '=', TK_SHLX, TK_SHL, '<');
        case '=': return check2(x, '=', TK_EQ, '=');
        case '>': return check4(x, '=', TK_GE, '>',
                                   '=', TK_SHRX, TK_SHR, '>');
        case '^': return check2(x, '=', TK_XORX, '^');
        case '|': return check3(x, '=', TK_ORX, '|', TK_OR, '|');
        case '\'': return char_literal(x, tk, c);
        case '$': case '(': case ')': case ',': case ':':
        case ';': case '?': case ']': case '[': case '{':
        case '}': case '~':
            return c;
        default:
            if (valid_alpha(c)) {
                return ident_or_reserved(x, tk);
            } else {
                err(x, "invalid token");
            }
        }
    }
}

int riff_lex_init(riff_lexer *x, const char *src) {
    struct {
        const char *str;
        uint8_t     tk;
    } reserved[] = {
        { "break",    TK_BREAK  },
        { "continue", TK_CONT   },
        { "do",       TK_DO     },
        { "elif",     TK_ELIF   },
        { "else",     TK_ELSE   },
        { "fn",       TK_FN     },
        { "for",      TK_FOR    },
        { "if",       TK_IF     },
        { "in",       TK_IN     },
        { "local",    TK_LOCAL  },
        { "loop",     TK_LOOP   },
        { "null",     TK_NULL   },
        { "return",   TK_RETURN },
        { "while",    TK_WHILE  }
    };

    FOREACH(reserved, i) {
        riff_str_new_extra(reserved[i].str, strlen(reserved[i].str), reserved[i].tk);
    }

    x->tk.kind = 0;
    x->la.kind = 0;
    x->ln      = 1;
    x->p       = src;
    x->buf.n   = 0;
    x->buf.cap = 0;
    x->buf.c   = NULL;
    riff_lex_advance(x, 0);
    return 0;
}

// Lexer itself should be stack-allocated, but the string buffer is
// heap-allocated
void riff_lex_free(riff_lexer *x) {
    if (x->buf.c != NULL) {
        free(x->buf.c);
    }
}

int riff_lex_advance(riff_lexer *x, int mode) {
    if (x->tk.kind == TK_EOI) {
        return 1;
    }

    // If a lookahead token already exists, assign it to the current
    // token
    if (x->la.kind != 0) {
        x->tk.kind = x->la.kind;
        x->tk.i    = x->la.i;
        x->la.kind = 0;
    } else if ((x->tk.kind = tokenize(x, mode, &x->tk)) == 1) {
        x->tk.kind = TK_EOI;
        return 1;
    }
    return 0;
}

// Populate the lookahead riff_token, leaving the current riff_token
// unchanged
int riff_lex_peek(riff_lexer *x, int mode) {
    if ((x->la.kind = tokenize(x, mode, &x->la)) == 1) {
        x->la.kind = TK_EOI;
        return 1;
    }
    return 0;
}
