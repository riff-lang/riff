#ifndef LEX_H
#define LEX_H

// Lexical Analyzer

typedef union {
    riff_real_t  r;
    riff_int_t   i;
    riff_str_t  *s;
} lexeme_t;

typedef struct {
    tokentype_t type;
    lexeme_t    lex;
} token_t;

typedef enum {
    //Single char tokens
    TK_OPAREN,
    TK_CPAREN,
    TK_OBRACE,
    TK_CBRACE,
    TK_OBRACKET,
    TK_CBRACKET,
    TK_COMMA,
    TK_DOT,
    TK_ADD,
    TK_SUB,
    TK_ASSIGN,
    TK_MUL,
    TK_DIV,
    TK_AND,
    TK_OR,
    TK_XOR,
    TK_BANG,
    TK_NOT,
    TK_MOD,
    TK_GT,
    TK_LT,
    TK_QMARK,
    TK_COLON,
    TK_SEMICOLON,
    TK_CARDINALITY,

    // Two char tokens
    TK_SHL,
    TK_SHR,
    TK_LOGICAL_AND,
    TK_LOGICAL_OR,
    TK_ADD_ASSIGN,
    TK_SUB_ASSIGN,
    TK_MUL_ASSIGN,
    TK_DIV_ASSIGN,
    TK_AND_ASSIGN,
    TK_OR_ASSIGN,
    TK_XOR_ASSIGN,
    TK_MOD_ASSIGN,
    TK_EQUAL,
    TK_BANG_EQUAL,
    TK_GT_EQUAL,
    TK_LT_EQUAL,
    TK_PRE_DEC,
    TK_PRE INC,
    TK_POST_DEC,
    TK_POST_INC,
    TK_POW,
    TK_ELVIS,

    // Three char tokens
    TK_SHL_ASSIGN,
    TK_SHR_ASSIGN,
    TK_POW_ASSIGN,

    // Keywords
    TK_IF,
    TK_ELSE,
    TK_FOR,
    TK_WHILE,
    TK_PRINT,
    TK_FUNCTION,
    TK_RETURN,

    TK_IDENTIFIER,
    TK_STRING,
    TK_INT,
    TK_FLOAT
} tokentype_t;

#endif
