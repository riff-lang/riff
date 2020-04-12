#include <stdio.h>
#include <stdint.h>

#include "disas.h"

static int simple_ins(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

void disas_block(codeblock_t *block, const char *name) {
    printf("%s\n", name);
    for (int i = 0; i < block->size;)
        i = disas_ins(block, i);
}

int disas_ins(codeblock_t *block, int offset) {
    printf("%d ", offset);
    uint8_t ins = block->code[offset];
    switch(ins) {
    case OP_RETURN: return simple_ins("return", offset);
    default: return offset + 1;
    }
}
