#ifndef BUF_H
#define BUF_H

#include "util.h"

#include <stddef.h>
#include <stdlib.h>

typedef struct {
    char   *buf;
    size_t  n;
    size_t  cap;
} riff_buf;

static inline void riff_buf_init_size(riff_buf *b, size_t sz) {
    b->buf = malloc(sz * sizeof (char));
    b->n   = 0;
    b->cap = sz;
}

static inline void riff_buf_init(riff_buf *b) {
    riff_buf_init_size(b, 8);
}

static inline void riff_buf_free(riff_buf *b) {
    free(b->buf);
}

static inline void riff_buf_grow(riff_buf *b) {
    if (riff_unlikely(b->cap <= b->n)) {
        b->cap *= 2;
        b->buf = realloc(b->buf, b->cap * sizeof (char));
    }
}

static inline void riff_buf_resize(riff_buf *b, size_t sz) {
    if (b->cap < b->n) {
        b->buf = realloc(b->buf, sz * sizeof (char));
        b->cap = sz;
    }
}

static inline void riff_buf_add_char(riff_buf *b, char c) {
    riff_buf_grow(b);
    b->buf[b->n++] = c;
}

#endif
