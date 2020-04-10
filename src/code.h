#ifndef CODE_H
#define CODE_H

#include <stdint.h>
#include <stdlib.h>

typedef enum {
    OP_RET,
} opcode_t;

typedef struct {
    int size;
    int cap;
    unit8_t *code;
} codeblock_t;

void block_init(codeblock_t *);
void block_write(codeblock_t *, uint8_t);

#endif
