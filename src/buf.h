#ifndef BUF_H
#define BUF_H

#include "util.h"

#include <stddef.h>
#include <stdlib.h>

typedef RIFF_VEC(char) riff_buf;

static inline void riff_buf_init_size(riff_buf *b, size_t sz) {
    riff_vec_init_size(b, sz);
}

static inline void riff_buf_init(riff_buf *b) {
    riff_vec_init(b);
}

static inline void riff_buf_free(riff_buf *b) {
    if (b->cap)
        free(b->list);
}

static inline void riff_buf_resize(riff_buf *b, size_t sz) {
    riff_vec_resize(b, sz);
}

static inline void riff_buf_add_char(riff_buf *b, char c) {
    riff_vec_add(b, c);
}

#endif
