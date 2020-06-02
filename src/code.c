#include "code.h"
#include "mem.h"

void c_init(chunk_t *c) {
    c->size = 0;
    c->cap  = 0;
    c->code = NULL;
}

void c_push(chunk_t *c, uint8_t b) {
    if (c->cap <= c->size) {
        c->cap  = GROW_CAP(c->cap);
        c->code = realloc(c->code, c->cap);
    }
    c->code[c->size++] = b;
}

void c_free(chunk_t *c) {
    free(c);
    c_init(c);
}
