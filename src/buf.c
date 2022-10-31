#include "buf.h"

#include <stdlib.h>

void riff_buf_init_size(riff_buf *b, size_t sz) {
    b->buf = malloc(sz * sizeof (char));
    b->n   = 0;
    b->cap = sz;
}

void riff_buf_init(riff_buf *b) {
    riff_buf_init_size(b, 8);
}

void riff_buf_free(riff_buf *b) {
    free(b->buf);
    b->n   = 0;
    b->cap = 0;
}

void riff_buf_grow(riff_buf *b) {
    if (b->cap <= b->n) {
        b->cap *= 2;
        b->buf = realloc(b->buf, b->cap * sizeof (char));
    }
}

void riff_buf_resize(riff_buf *b, size_t sz) {
    if (b->cap < b->n) {
        b->buf = realloc(b->buf, sz * sizeof (char));
        b->cap = sz;
    }
}
