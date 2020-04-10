#include <stdio.h>

int main(int argc, char **argv) {
    switch (argc) {
    case 1: repl(); break;
    case 2: run_file(argv[1]); break;
    default: fprintf(stderr, "Err\n"); exit(1);
    }
    return 0;
}
