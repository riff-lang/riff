#ifndef PARSE_H
#define PARSE_H

#include "code.h"
#include "lex.h"
#include "types.h"

typedef struct {
    lexer_t *x;
    code_t  *c;  // Current code object
    uint8_t  ax; // Assignment flag
    uint8_t  ox; // Typical operation flag
    uint8_t  px; // Prefix/postfix increment/decrement flag
    uint8_t  rx; // Reference flag
} parser_t;

int y_compile(const char *, code_t *);

#endif
