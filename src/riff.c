#include "code.h"
#include "disas.h"
#include "env.h"
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
         "  --       stop processing options");
}

// TODO handle piped input (stdin)
int main(int argc, char **argv) {
    rf_env e;
    e_init(&e);
    rf_fn main;
    rf_code c;
    c_init(&c);
    main.code = &c;
    main.arity = 0;
    e.main = main;

    if (argc == 1) {
        fprintf(stderr, "riff: No program given\n");
        exit(1);
    }

    bool disas = false;
    opterr = 0;

    int o;
    while ((o = getopt(argc, argv, "e:hlv")) != -1) {
        switch (o) {
        case 'e':
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
            printf("riff: unrecognized option: '-%c'\n", optopt);
            usage();
            exit(1);
        default:
            break;
        }
    }

    e.argc = argc;
    e.argv = argv;

    if (optind < argc) {
        e.arg0 = optind;
        e.pname = argv[optind];
        e.src = u_file2str(argv[optind]);
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
