#ifndef LEX_H
#define LEX_H

#include "value.h"

#include <stdint.h>
#include <stdlib.h>

// Lexical Analyzer

typedef struct {
    int kind;
    union {
        riff_int    i;
        riff_float  f;
        riff_str   *s;
        riff_regex *r;
    };
} riff_token;

enum lexer_mode {
    LEX_NUD,
    LEX_LED
};

enum token_kind {
    // NOTE: Single character tokens are implicitly enumerated

    TK_AND = 128, // &&
    TK_DEC,       // --
    TK_2DOTS,     // ..
    TK_EQ,        // ==
    TK_GE,        // >=
    TK_INC,       // ++
    TK_LE,        // <=
    TK_NE,        // !=
    TK_NMATCH,    // !~
    TK_OR,        // ||
    TK_POW,       // **
    TK_SHL,       // <<
    TK_SHR,       // >>
    TK_ADDX,      // +=
    TK_ANDX,      // &=
    TK_CATX,      // #=
    TK_DIVX,      // /=
    TK_MODX,      // %=
    TK_MULX,      // *=
    TK_ORX,       // |=
    TK_POWX,      // **=
    TK_SHLX,      // <<=
    TK_SHRX,      // >>=
    TK_SUBX,      // -=
    TK_XORX,      // ^=

    // Keywords
    TK_BREAK,
    TK_CONT,
    TK_DO,
    TK_ELIF,
    TK_ELSE,
    TK_FN,
    TK_FOR,
    TK_IF,
    TK_IN,
    TK_LOCAL,
    TK_LOOP,
    TK_RETURN,
    TK_WHILE,

    // Literals
    TK_NULL,
    TK_FLOAT,
    TK_INT,
    TK_STR,
    TK_REGEX,

    // Identifiers
    TK_IDENT,

    // End of input
    TK_EOI
};

typedef struct {
    riff_token  tk;     // Current token
    riff_token  la;     // Lookahead token
    int         ln;     // Current line of the source
    const char *p;      // Pointer to the current position in the source
    struct {
        int   n;
        int   cap;
        char *c;
    } buf;              // String buffer
} riff_lexer;

int  riff_lex_init(riff_lexer *, const char *);
void riff_lex_free(riff_lexer *);
int  riff_lex_advance(riff_lexer *, int);
int  riff_lex_peek(riff_lexer *, int);

#endif
