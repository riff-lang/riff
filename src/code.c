#include "code.h"
#include "mem.h"

void c_init(chunk_t *c, const char *name) {
    c->name = name;
    c->size = 0;
    c->cap  = 0;
    c->code = NULL;
}

void c_push(chunk_t *c, uint8_t b) {
    if (c->cap <= c->size) {
        c->cap  = increase_cap(c->cap);
        c->code = realloc(c->code, c->cap);
    }
    c->code[c->size++] = b;
}

void c_free(chunk_t *c) {
    free(c);
    c_init(c, "");
}

// Add a constant value_t to a chunk's constant table, returning the
// constant's index in the table.
uint8_t c_addk(chunk_t *c, value_t *v) {
    // TODO Memory issues
    if (c->k.cap <= c->k.size) {
        c->k.cap = increase_cap(c->k.cap);
        c->k.k   = realloc(c->k.k, c->k.cap);
    }
    c->k.k[c->k.size++] = *v;

    return c->k.size - 1;
}
