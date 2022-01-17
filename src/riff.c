#include "code.h"
#include "disas.h"
#include "env.h"
#include "mem.h"
#include "parse.h"
#include "types.h"
#include "util.h"
#include "vm.h"

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Entry point for the Emscripten-compiled WASM/JS module
#ifdef __EMSCRIPTEN__
int wasm_main(int flag, char *str) {
    rf_env e;
    e_init(&e);
    rf_fn main;
    rf_code c;
    c_init(&c);
    main.code = &c;
    main.arity = 0;
    e.main = main;
    e.pname = "<playground>";
    e.src = str;
    main.name = TEMP_HASHED_STR((char *) e.pname, u_strhash(e.pname), strlen(e.pname));
    y_compile(&e);
    if (flag)
        z_exec(&e);
    else
        d_prog(&e);
    return 0;
}
#else
static void version(void) {
    puts("riff " GIT_DESC " Copyright 2020-2022, Darryl Abbate");
}

static void usage(void) {
    puts("usage: riff [options] program [argument ...]\n"
         "Available options:\n"
         "  -e prog  execute string 'prog'\n"
         "  -h       print this usage text and exit\n"
         "  -l       list bytecode with assembler-like mnemonics\n"
         "  -v       print version information and exit\n"
         "  --       stop processing options\n"
         "  -        stop processing options and execute stdin");
}

static void skip_shebang(FILE *file, size_t *s) {
    int c0 = fgetc(file);
    if (c0 == '#') {
        int c = fgetc(file);
        if (c == '!') {
            s -= 2;
            do {
                c = fgetc(file);
                --s;
            } while (c != EOF && c != '\n');
        } else {
            ungetc(c, file);
            ungetc(c0, file);
        }
    } else {
        ungetc(c0, file);
    }
}

static char *stdin2str(void) {
    size_t s = ftell(stdin);
    skip_shebang(stdin, &s);
    int cap = 0;
    char *buf = NULL;
    int n = 0;
    char c;
    while ((c = fgetc(stdin)) != EOF) {
        m_growarray(buf, n, cap, buf);
        buf[n++] = c;
    }
    if (!buf)
        return NULL;
    buf[n] = '\0';
    return buf;
}

static char *file2str(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "riff: file not found: %s\n", path);
        exit(1);
    }
    fseek(file, 0L, SEEK_END);
    size_t s = ftell(file);
    rewind(file);
    skip_shebang(file, &s);
    char *buf = malloc(s + 1);
    size_t end = fread(buf, sizeof(char), s, file);
    fclose(file);
    buf[end] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    rf_env e;
    e_init(&e);
    rf_fn main;
    rf_code c;
    c_init(&c);
    main.code = &c;
    main.arity = 0;
    e.main = main;
    e.argc = argc;
    e.argv = argv;
    bool disas = false;
    bool opt_e = false;
    opterr = 0;
    int o;
    while ((o = getopt(argc, argv, "e:hlv")) != -1) {
        switch (o) {
        case 'e':
            opt_e = true;
            e.src = optarg;
            y_compile(&e);
            break;
        case 'h':
            usage();
            exit(0);
        case 'l':
            disas = true;
            break;
        case 'v':
            version();
            exit(0);
        case '?':
            if (optopt == 'e')
                printf("riff: missing argument for option '-e'\n");
            else
                printf("riff: unrecognized option: '-%c'\n", optopt);
            usage();
            exit(1);
        default:
            break;
        }
    }

    // Check for piped input
    if (optind == argc && !opt_e) {
        if ((fseek(stdin, 0, SEEK_END), ftell(stdin)) == 0) {
            fprintf(stderr, "riff: No program given\n");
            return 1;
        }
        e.src = stdin2str();
        e.pname = "<stdin>";
        y_compile(&e);
    } else if (optind < argc) {
        // Option '-': Stop processing options and execute stdin
        if (argv[optind][0] == '-' && argv[optind][1] != '-') {
            e.src = stdin2str();
            e.pname = "<stdin>";
        } else {
            e.src = file2str(argv[optind]);
            e.pname = argv[optind];
        }
        e.arg0 = optind;
        y_compile(&e);
    } else {
        e.pname = "<command-line>";
    }

    // -l: List riff's arbitrary disassembly for the given program
    if (disas) {
        main.name = TEMP_HASHED_STR((char *) e.pname, u_strhash(e.pname), strlen(e.pname));
        d_prog(&e);
    } else {
        main.name = NULL;
        z_exec(&e);
    }
    return 0;
}
#endif
