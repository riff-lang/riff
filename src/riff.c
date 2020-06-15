#include <stdio.h>
#include <stdlib.h>

#include "code.h"
#include "disas.h"
#include "lex.h"

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

static void print_tk_stream(const char *src) {
    lexer_t x;
    x.ln = 1;
    x.p = src;
    x.la.type = 0;
    while (!x_next(&x)) {
        char s[1024];
        tk2str(&x.tk, s);
        printf("%s\n", s);
    }
}

int main(int argc, char **argv) {
    if (argc == 2)
        print_tk_stream(argv[1]);
    else
        print_tk_stream(stringify_file(argv[2]));
    return 0;
}
