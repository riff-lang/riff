#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "code.h"
#include "disas.h"
#include "env.h"
#include "lex.h"
#include "parse.h"
#include "types.h"
#include "vm.h"

#define VERSION "0.1a"

static void version(void) {
    printf("riff %s Copyright 2020, Darryl Abbate\n", VERSION);
    exit(0);
}

static char *stringify_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "file not found: %s\n", path);
        exit(1);
    }
    fseek(file, 0L, SEEK_END);
    size_t s = ftell(file);
    rewind(file);
    char *buf  = malloc(s + 1);
    size_t end = fread(buf, sizeof(char), s, file);
    buf[end]   = '\0';
    return buf;
}


// TODO handle piped input (stdin)
int main(int argc, char **argv) {
    if (argc == 1) {
        printf("No program given\n");
        exit(1);
    }

    rf_code c;
    c_init(&c);

    int df = 0;
    int ff = 0;
    int uf = 0;
    opterr = 0;

    int o;
    while ((o = getopt(argc, argv, "dfv")) != -1) {
        switch (o) {
        case 'd': df = 1; break;
        case 'f': ff = 1; break;
        case 'v': version();
        case '?': uf = 1; break;
        default: break;
        }
    }

    rf_env e;
    e.argc = argc;
    e.ff   = ff;
    e.argv = argv;

    // If getopt() tries to parse an unidentified option, decrement
    // optind. getopt() will try to parse the actual program if it
    // begins with a unary minus sign.
    if (uf) --optind;

    // -f: Open file and convert contents to null-terminated string
    if (ff)
        y_compile(stringify_file(argv[optind]), &c);
    else
        y_compile(argv[optind], &c);

    // -d: Dump riff's arbitrary disassembly for the given program
    if (df)
        d_code_chunk(&c);
    else
        z_exec(&e, &c);
    return 0;
}
