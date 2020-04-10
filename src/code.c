#include "code.h"
#include "mem.h"

void block_init(codeblock_t *block) {
    block->size = 0;
    block->cap  = 0;
    block->code = NULL;
}

void block_write(codeblock_t *block, uint8_t byte) {
    if (block->cap <= block->size) {
        // Increase capacity
    }
    block->code[block->size++] = byte;
}
