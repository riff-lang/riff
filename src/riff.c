#include "buf.h"
#include "code.h"
#include "disas.h"
#include "mem.h"
#include "parse.h"
#include "state.h"
#include "string.h"
#include "value.h"
#include "vm.h"

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__

char *riff_version(void) {
    return RIFF_VERSION;
}

// Entry point for the Emscripten-compiled WASM/JS module
int riff_main(int flag, char *str) {
    riff_state global_state;

    riff_stab_init();
    riff_state_init(&global_state);
    global_state.name = "<playground>",
    global_state.src = str,

    riff_compile(&global_state);
    if (flag)
        vm_exec(&global_state);
    else
        riff_disas(&global_state);
    return 0;
}

#else

static void version(void) {
    puts("riff " RIFF_VERSION " Copyright 2020-2023 Darryl Abbate");
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
    riff_buf buf;
    riff_buf_init(&buf);
    char c;
    while ((c = fgetc(stdin)) != EOF) {
        riff_buf_add_char(&buf, c);
    }
    if (!buf.n)
        return NULL;
    riff_buf_add_char(&buf, '\0');
    return buf.list;
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
    riff_state global_state;

    bool opt_e = false;

    riff_stab_init();
    riff_state_init(&global_state);
    global_state.argc = argc,
    global_state.argv = argv,

    opterr = 0;
    int o;
    while ((o = getopt(argc, argv, "e:hlv")) != -1) {
        switch (o) {
        case 'e':
            opt_e = true;
            global_state.src = optarg;
            riff_compile(&global_state);
            break;
        case 'h':
            usage();
            exit(0);
        case 'l':
            global_state.disas = true;
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
        global_state.src = stdin2str();
        global_state.name = "<stdin>";
        riff_compile(&global_state);
    } else if (optind < argc) {
        // Option '-': Stop processing options and execute stdin
        if (argv[optind][0] == '-' && argv[optind][1] != '-') {
            global_state.src = stdin2str();
            global_state.name = "<stdin>";
        } else {
            global_state.src = file2str(argv[optind]);
            global_state.name = argv[optind];
        }
        global_state.arg0 = optind;
        riff_compile(&global_state);
    } else {
        global_state.name = "<command-line>";
    }

    // -l: List riff's arbitrary disassembly for the given program
    if (riff_unlikely(global_state.disas))
        riff_disas(&global_state);
    else
        vm_exec(&global_state);
    return 0;
}

#endif
