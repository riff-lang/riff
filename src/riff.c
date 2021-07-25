#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "code.h"
#include "disas.h"
#include "env.h"
#include "parse.h"
#include "types.h"
#include "util.h"
#include "vm.h"

// Entry point for the Emscripten-compiled WASM/JS module
#ifdef __EMSCRIPTEN__
int wasm_main(int flag, char *str) {
    rf_env e;
    rf_fn  main;
    rf_code c;
    c_init(&c);
    main.code  = &c;
    main.arity = 0;
    e.nf       = 0;
    e.fcap     = 0;
    e.fn       = NULL;
    e.main     = main;
    e.argc     = 0;
    e.ff       = 0;
    e.argv     = NULL;
    e.pname    = "<playground>";
    e.src      = str;
    main.name  = s_newstr(e.pname, strlen(e.pname), 1);
    y_compile(&e);
    if (flag)
        z_exec(&e);
    else
        d_prog(&e);
    return 0;
}
#else
static void version(void) {
    puts("riff " GIT_DESC " Copyright 2020-2021, Darryl Abbate");
    exit(0);
}

static void usage(void) {
    puts("usage: riff [options] 'program' [argument ...]\n"
         "       riff [options] -f file [argument ...]\n"
         "Available options:\n"
         "  -f file  execute program stored in 'file'\n"
         "  -h       print this usage text and exit\n"
         "  -l       list bytecode with assembler-like mnemonics\n"
         "  -v       print version information and exit\n"
         "  --       stop processing options");
    exit(0);
}

// TODO handle piped input (stdin)
int main(int argc, char **argv) {
    if (argc == 1) {
        printf("No program given\n");
        exit(1);
    }

    rf_env e;
    rf_fn  main;
    rf_code c;
    c_init(&c);
    main.code  = &c;
    main.arity = 0;
    e.nf       = 0;
    e.fcap     = 0;
    e.fn       = NULL;
    e.main     = main;

    int ff = 0;
    int lf = 0;
    int uf = 0;
    opterr = 0;

    int o;
    while ((o = getopt(argc, argv, "f:hlv")) != -1) {
        switch (o) {
        case 'f':
            ff = 1;
            e.pname = optarg;
            e.src   = u_file2str(optarg);
            break;
        case 'h':
            usage();
        case 'l':
            lf = 1;
            break;
        case 'v':
            version();
        case '?':
            uf = 1;
            break;
        default:
            break;
        }
    }

    e.argc = argc;
    e.ff   = ff;
    e.argv = argv;

    // If getopt() tries to parse an unidentified option, decrement
    // optind. getopt() will try to parse the actual program if it
    // begins with a unary minus sign.
    if (uf) --optind;

    if (!ff) {
        e.pname = "<command-line>";
        e.src   = argv[optind];
    }

    main.name = s_newstr(e.pname, strlen(e.pname), 1);
    y_compile(&e);

    // -l: List riff's arbitrary disassembly for the given program
    if (lf)
        d_prog(&e);
    else
        z_exec(&e);
    return 0;
}
#endif
