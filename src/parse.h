#ifndef PARSE_H
#define PARSE_H

#include "code.h"
#include "lex.h"
#include "types.h"

typedef struct {
    lexer_t *x;     // Parser controls lexical analysis
    code_t  *c;     // Current code object

    // Flags used when evaluating whether expression statements should
    // be printed
    uint8_t  lhs;   // Set when leftmost expr has been evaluated
    uint8_t  idx;   // Depth of index (for exprs inside [])
    uint8_t  ax;    // Assignment flag
    uint8_t  ox;    // Typical (i.e. not ++/--) operation flag
    uint8_t  px;    // Prefix or postfix increment/decrement flag

    uint8_t  rx;    // Reference flag, i.e. OP_XXXA vs OP_XXXV instructions
    uint8_t  ld;    // Lexical depth
} parser_t;

int y_compile(const char *, code_t *);

#endif
