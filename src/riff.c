#include <stdio.h>
#include <stdlib.h>

#include "code.h"
#include "disas.h"
#include "lex.h"
#include "parse.h"
#include "vm.h"

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

// TODO handle piped input (stdin)
int main(int argc, char **argv) {
    code_t c;
    c_init(&c);
    if (argc == 2)
        y_compile(argv[1], &c);
    else if (argc == 3)
        y_compile(stringify_file(argv[2]), &c);
    z_exec(&c);
    // d_code_chunk(&c);
    return 0;
}
