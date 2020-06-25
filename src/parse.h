#ifndef PARSE_H
#define PARSE_H

#include "code.h"
#include "lex.h"
#include "val.h"

typedef struct {
    lexer_t *x;
    chunk_t *c; // Current code chunk
    int      a; // Assignment flag
    // Hold tokens?
} parser_t;

int y_compile(const char *, chunk_t *);

#endif
