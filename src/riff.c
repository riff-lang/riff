#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "code.h"
#include "disas.h"
#include "lex.h"
#include "parse.h"
#include "vm.h"

static char *stringify_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "File not found: %s\n", path);
        exit(1);
    }
    fseek(file, 0L, SEEK_END);
    size_t s = ftell(file);
    rewind(file);
    char *buf = malloc(s + 1);
    size_t b = fread(buf, sizeof(char), s, file);
    buf[b] = '\0';
    return buf;
}

// TODO handle piped input (stdin)
int main(int argc, char **argv) {
    // Temporary; Program will start a REPL if invoked with no args
    if (argc == 1) {
        printf("No program given\n");
        exit(1);
    }

    code_t c;
    c_init(&c);

    int df = 0;
    int ff = 0;
    int uf = 0;
    opterr = 0;

    int o;
    while ((o = getopt(argc, argv, "df")) != -1) {
        switch (o) {
        case 'd': df = 1; break;
        case 'f': ff = 1; break;
        case '?': uf = 1; break;
        default: break;
        }
    }

    // If getopt() tries to parse an unidentified option, decrement
    // optind. getopt() will try to parse the actual program if it
    // begins with a unary minus sign.
    if (uf) optind--;

    if (ff) y_compile(stringify_file(argv[optind]), &c);
    else    y_compile(argv[optind], &c);

    if (df) d_code_chunk(&c);
    else    z_exec(&c);
    return 0;
}
