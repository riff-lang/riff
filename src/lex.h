#ifndef LEX_H
#define LEX_H

#include <stdint.h>
#include <stdlib.h>

#include "types.h"

// Lexical Analyzer

typedef struct {
    int kind;
    union {
        flt_t  f;
        int_t  i;
        str_t *s;
    } lexeme;
} token_t;

enum tokens {
    // NOTE: Single character tokens are implicitly enumerated

    TK_AND = 128, // &&
    TK_CAT,       // ::
    TK_DEC,       // --
    TK_EQ,        // ==
    TK_GE,        // >=
    TK_INC,       // ++
    TK_LE,        // <=
    TK_NE,        // !=
    TK_OR,        // ||
    TK_POW,       // **
    TK_SHL,       // <<
    TK_SHR,       // >>
    TK_ADDX,      // +=
    TK_ANDX,      // &=
    TK_CATX,      // ::=
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
    TK_ELSE,
    TK_EXIT,
    TK_FN,
    TK_FOR,
    TK_IF,
    TK_LOCAL,
    TK_PRINT,
    TK_RETURN,
    TK_WHILE,

    // Literals
    TK_FLT,
    TK_INT,
    TK_STR,

    // Identifiers
    TK_ID,

    // End of input
    TK_EOI
};

typedef struct {
    int         ln; // Current line of the source
    token_t     tk; // Current token
    token_t     la; // Lookahead token
    const char *p;  // Pointer to the current position in the source
    struct {
        int   n;
        int   cap;
        char *c;
    } buffer;       // String buffer
} lexer_t;

int x_init(lexer_t *, const char *);
int x_adv(lexer_t *);
int x_peek(lexer_t *);

#endif
