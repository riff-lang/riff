#include <stdio.h>
#include <stdlib.h>

#include "code.h"
#include "disas.h"
#include "lex.h"
#include "parse.h"

static char *stringify_file(const char *path) {
    FILE *file = fopen(path, "rb");
    fseek(file, 0L, SEEK_END);
    size_t s = ftell(file);
    rewind(file);
    char *buffer = (char *) malloc(s + 1);
    size_t b = fread(buffer, sizeof(char), s, file);
    buffer[b] = '\0';
    return buffer;
}

static void print_chunk(chunk_t *c) {
    printf("Size: %d\n", c->size);
    for(int i = 0; i < c->size; i++) {
        printf("%02x ", c->code[i]);
    }
    printf("\n");
}

int main(int argc, char **argv) {
    chunk_t c;
    c_init(&c);
    if (argc == 2)
        y_compile(argv[1], &c);
        // print_tk_stream(argv[1]);
    else
        y_compile(stringify_file(argv[2]), &c);
        // print_tk_stream(stringify_file(argv[2]));
    print_chunk(&c);
    return 0;
}
