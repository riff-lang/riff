#include "code.h"
#include "mem.h"

void c_init(chunk_t *c, const char *name) {
    c->name  = name;
    c->n     = 0;
    c->cap   = 0;
    c->code  = NULL;
    c->k.n   = 0;
    c->k.cap = 0;
    c->k.v   = NULL;
}

void c_push(chunk_t *c, uint8_t b) {
    if (c->cap <= c->n) {
        c->cap  = increase_cap(c->cap);
        c->code = realloc(c->code, c->cap);
    }
    c->code[c->n++] = b;
}

void c_free(chunk_t *c) {
    free(c);
    c_init(c, "");
}

// Add a constant val_t to a chunk's constant table, returning the
// constant's index in the table.
uint8_t c_addk(chunk_t *c, val_t *v) {
    if (c->k.cap <= c->k.n) {
        c->k.cap = increase_cap(c->k.cap);
        c->k.v   = realloc(c->k.v, sizeof(val_t *) * c->k.cap);
    }
    c->k.v[c->k.n++] = v;
    return c->k.n - 1;
}
