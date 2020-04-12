#include "code.h"
#include "mem.h"

void block_init(codeblock_t *block) {
    block->size = 0;
    block->cap  = 0;
    block->code = NULL;
}

void block_append(codeblock_t *block, uint8_t byte) {
    if (block->cap <= block->size) {
        block->cap  = GROW_CAP(block->cap);
        block->code = realloc(block->code, block->cap);
    }
    block->code[block->size++] = byte;
}

void block_free(codeblock_t *block) {
    free(block);
    block_init(block);
}
