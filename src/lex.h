#ifndef LEX_H
#define LEX_H

#include <stdint.h>

// Lexical Analyzer

typedef double  real_t;
typedef int64_t int_t;
typedef char *  str_t;

typedef union {
    char   c;
    real_t r;
    int_t  i;
    str_t  s;
} lexeme_t;

typedef struct {
    int      token;
    lexeme_t lexeme;
} token_t;

enum tokens {
    TK_AND = 257,
    TK_DEC,
    TK_EQ,
    TK_GTE,
    TK_INC,
    TK_LTE,
    TK_NEQ,
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
    TK_POW_ASSIGN,
    TK_SHL_ASSIGN,
    TK_SHR_ASSIGN,

    // Keywords
    TK_BREAK,
    TK_ELSE,
    TK_IF,
    TK_FN,
    TK_FOR,
    TK_LOCAL,
    TK_PRINT,
    TK_RETURN,
    TK_WHILE,

    TK_CHAR,
    TK_FLT,
    TK_ID,
    TK_INT,
    TK_STR
};

typedef struct lexer_t {
    int         line;
    token_t     token;
    token_t     lookahead;
    const char *p;
} lexer_t;

int lex_next(lexer_t *);

#endif
