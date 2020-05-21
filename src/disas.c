#include <stdio.h>
#include <stdint.h>

#include "disas.h"

static int simple_ins(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

void disas_chunk(chunk_t *chunk, const char *name) {
    printf("%s\n", name);
    for (int i = 0; i < chunk->size;)
        i = disas_ins(chunk, i);
}

int disas_ins(chunk_t *chunk, int offset) {
    printf("%d ", offset);
    uint8_t ins = chunk->code[offset];
    switch(ins) {
    case OP_RETURN: return simple_ins("return", offset);
    default: return offset + 1;
    }
}
