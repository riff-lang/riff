#ifndef PARSE_H
#define PARSE_H

#include "code.h"
#include "lex.h"
#include "val.h"

typedef struct {
    lexer_t *x;
    code_t  *c;  // Current code object
    int      pf; // Print flag
} parser_t;

int y_compile(const char *, code_t *);

#endif
