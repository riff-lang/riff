#ifndef CODE_H
#define CODE_H

#include <stdint.h>
#include <stdlib.h>

typedef enum {
    OP_RETURN,
} opcode_t;

typedef struct {
    int      size;
    int      cap;
    uint8_t *code;
} chunk_t;

void chunk_init(chunk_t *);
void chunk_append(chunk_t *, uint8_t);
void chunk_free(chunk_t *);

#endif
