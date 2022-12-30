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
    // Single character tokens are implicitly enumerated by their ASCII codes

    RIFF_TK_2DOTS = 128,
    RIFF_TK_AND,
    RIFF_TK_DEC,
    RIFF_TK_EQ,
    RIFF_TK_GE,
    RIFF_TK_INC,
    RIFF_TK_LE,
    RIFF_TK_NE,
    RIFF_TK_NMATCH,
    RIFF_TK_OR,
    RIFF_TK_POW,
    RIFF_TK_SHL,
    RIFF_TK_SHR,

    // Compound assignment operators
    RIFF_TK_ADDX,
    RIFF_TK_ANDX,
    RIFF_TK_CATX,
    RIFF_TK_DIVX,
    RIFF_TK_MODX,
    RIFF_TK_MULX,
    RIFF_TK_ORX,
    RIFF_TK_POWX,
    RIFF_TK_SHLX,
    RIFF_TK_SHRX,
    RIFF_TK_SUBX,
    RIFF_TK_XORX,

    // Keywords
    RIFF_TK_BREAK,
    RIFF_TK_CONT,
    RIFF_TK_DO,
    RIFF_TK_ELIF,
    RIFF_TK_ELSE,
    RIFF_TK_FN,
    RIFF_TK_FOR,
    RIFF_TK_IF,
    RIFF_TK_IN,
    RIFF_TK_LOCAL,
    RIFF_TK_LOOP,
    RIFF_TK_RETURN,
    RIFF_TK_UNTIL,
    RIFF_TK_WHILE,

    // Literals
    RIFF_TK_NULL,
    RIFF_TK_FLOAT,
    RIFF_TK_INT,
    RIFF_TK_STR,
    RIFF_TK_STR_INTER_SQ,
    RIFF_TK_STR_INTER_DQ,
    RIFF_TK_REGEX,

    // Identifiers
    RIFF_TK_IDENT,

    // End of input
    RIFF_TK_EOI
};

typedef struct {
    riff_token  tk[2];  // LL(1)
    const char *p;
    int         ln;
    riff_buf    buf;
} riff_lexer;

int  riff_lex_init(riff_lexer *, const char *);
void riff_lex_free(riff_lexer *);
int  riff_lex_advance(riff_lexer *, int);
int  riff_lex_peek(riff_lexer *, int);

#endif
