#include "code.h"
#include "mem.h"

void chunk_init(chunk_t *chunk) {
    chunk->size = 0;
    chunk->cap  = 0;
    chunk->code = NULL;
}

void chunk_append(chunk_t *chunk, uint8_t byte) {
    if (chunk->cap <= chunk->size) {
        chunk->cap  = GROW_CAP(chunk->cap);
        chunk->code = realloc(chunk->code, chunk->cap);
    }
    chunk->code[chunk->size++] = byte;
}

void chunk_free(chunk_t *chunk) {
    free(chunk);
    chunk_init(chunk);
}
