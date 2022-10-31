#ifndef BUF_H
#define BUF_H

#include <stddef.h>

typedef struct {
    char   *buf;
    size_t  n;
    size_t  cap;
} riff_buf;

void riff_buf_init_size(riff_buf *, size_t);
void riff_buf_init(riff_buf *);
void riff_buf_grow(riff_buf *);
void riff_buf_free(riff_buf *);
void riff_buf_resize(riff_buf *, size_t);

#endif
