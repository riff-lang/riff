#ifndef LEX_H
#define LEX_H

#include <stdint.h>
#include <stdlib.h>

#include "str.h"
#include "val.h"

// Lexical Analyzer

typedef struct {
    int type;
    union {
        flt_t  f;
        int_t  i;
        str_t *s;
    } lexeme;
} token_t;

enum tokens {
    // Single character tokens are implicitly enumerated

    // Multi-char operators
    TK_AND = 256,
    TK_CAT,
    TK_DEC,
    TK_EQ,
    TK_GE,
    TK_INC,
    TK_LE,
    TK_NE,
    TK_OR,
    TK_POW,
    TK_SHL,
    TK_SHR,
    TK_ADD_ASSIGN,
    TK_AND_ASSIGN,
    TK_DIV_ASSIGN,
    TK_MOD_ASSIGN,
    TK_MUL_ASSIGN,
    TK_OR_ASSIGN,
    TK_SUB_ASSIGN,
    TK_XOR_ASSIGN,
    TK_CAT_ASSIGN,
    TK_POW_ASSIGN,
    TK_SHL_ASSIGN,
    TK_SHR_ASSIGN,

    // Keywords
    TK_BREAK,
    TK_ELSE,
    TK_EXIT,
    TK_FN,
    TK_FOR,
    TK_IF,
    TK_LOCAL,
    TK_PRINT,
    TK_RETURN,
    TK_WHILE,

    // Types
    TK_FLT,
    TK_ID,
    TK_INT,
    TK_STR,

    TK_EOF
};

typedef struct lexer_t {
    int         ln; // Current line of the source
    token_t     tk; // Current token
    token_t     la; // Lookahead token
    const char *p;  // Pointer to the current position in the source
} lexer_t;

int x_init(lexer_t *, const char *);
int x_adv(lexer_t *);

#endif
