#ifndef LEX_H
#define LEX_H

#include "buf.h"
#include "value.h"

#include <stdint.h>
#include <stdlib.h>

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
    LEX_LED,
    LEX_STR_SQ,     // Interpolated string (single quotes)
    LEX_STR_DQ      // Interpolated string (double quotes)
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
    TK_STR_INTER_SQ,
    TK_STR_INTER_DQ,
    TK_REGEX,

    // Identifiers
    TK_IDENT,

    // End of input
    TK_EOI
};

typedef struct {
    riff_token  tk[2];  // LL(1)
    const char *p;
    int         ln;
    riff_buf   *buf;
} riff_lexer;

int  riff_lex_init(riff_lexer *, const char *);
int  riff_lex_advance(riff_lexer *, int);
int  riff_lex_peek(riff_lexer *, int);

#endif
