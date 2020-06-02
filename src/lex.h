#ifndef LEX_H
#define LEX_H

#include <stdint.h>
#include <stdlib.h>

// Lexical Analyzer

typedef double  flt_t;
typedef int64_t int_t;
typedef char *  str_t;

typedef union {
    char  c;
    flt_t f;
    int_t i;
    str_t s;
} lexeme_t;

typedef struct {
    int      token;
    lexeme_t lexeme;
} token_t;

// NOTE: Single character tokens are implicitly enumerated by their
// ASCII codes

enum tokens {
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
    TK_NOT_ASSIGN,
    TK_OR_ASSIGN,
    TK_SUB_ASSIGN,
    TK_XOR_ASSIGN,

    // Three char tokens
    TK_CAT_ASSIGN,
    TK_POW_ASSIGN,
    TK_SHL_ASSIGN,
    TK_SHR_ASSIGN,

    // Keywords
    TK_BREAK,
    TK_ELSE,
    TK_EXIT,
    TK_IF,
    TK_FN,
    TK_FOR,
    TK_LOCAL,
    TK_PRINT,
    TK_RETURN,
    TK_WHILE,

    // Types
    TK_CHAR,
    TK_FLT,
    TK_ID,
    TK_INT,
    TK_STR
};

typedef struct lexer_t {
    int         ln; // Current line of the source
    token_t     tk; // Current token
    token_t     la; // Lookahead token
    const char *p;  // Pointer to the current position in the source
} lexer_t;

int x_next(lexer_t *);

#endif
