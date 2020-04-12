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
} codeblock_t;

void block_init(codeblock_t *);
void block_append(codeblock_t *, uint8_t);
void block_free(codeblock_t *);

#endif
