#ifndef LEX_H
#define LEX_H

#include "types.h"

#include <stdint.h>
#include <stdlib.h>

// Lexical Analyzer

typedef struct {
    union {
        rf_flt  f;
        rf_int  i;
        rf_str *s;
        rf_re  *r;
    } lexeme;
    int kind;
} rf_token;

enum tokens {
    // NOTE: Single character tokens are implicitly enumerated

    TK_AND = 128, // &&
    TK_DEC,       // --
    TK_DOTS,      // ..
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
    TK_EXIT,
    TK_FN,
    TK_FOR,
    TK_IF,
    TK_IN,
    TK_LOCAL,
    TK_LOOP,
    TK_PRINT,
    TK_RETURN,
    TK_WHILE,

    // Literals
    TK_NULL,
    TK_FLT,
    TK_INT,
    TK_STR,
    TK_RE,

    // Identifiers
    TK_ID,

    // End of input
    TK_EOI
};

typedef struct {
    rf_token    tk;     // Current token
    rf_token    la;     // Lookahead token
    int         mode;
    int         ln;     // Current line of the source
    const char *p;      // Pointer to the current position in the source
    struct {
        int   n;
        int   cap;
        char *c;
    } buf;              // String buffer
} rf_lexer;

int  x_init(rf_lexer *, const char *);
void x_free(rf_lexer *);
int  x_adv(rf_lexer *);
int  x_peek(rf_lexer *);

#endif
