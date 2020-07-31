#ifndef PARSE_H
#define PARSE_H

#include "code.h"
#include "lex.h"
#include "types.h"

typedef struct {
    lexer_t *x;  // Parser controls lexical analysis
    code_t  *c;  // Current code object

    // Implicit print condition flags
    uint8_t  ax; // Assignment flag
    uint8_t  ox; // Typical (i.e. not ++/--) operation flag
    uint8_t  px; // Prefix or postfix increment/decrement flag

    uint8_t  rx; // Reference flag, i.e. OP_XXXA vs OP_XXXV instructions
    uint8_t  depth;
} parser_t;

int y_compile(const char *, code_t *);

#endif
