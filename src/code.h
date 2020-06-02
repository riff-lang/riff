#ifndef CODE_H
#define CODE_H

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    int      size;
    int      cap;
    uint8_t *code;
} chunk_t;

void c_init(chunk_t *);
void c_push(chunk_t *, uint8_t);
void c_free(chunk_t *);

#endif
