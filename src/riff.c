#include <stdio.h>
#include <stdlib.h>

#include "code.h"
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
        if (x.tk.type <= 255)
            printf("%c\n", x.tk.type);
        else if (x.tk.type < TK_FLT)
            printf("%d\n", x.tk.type);
        else if (x.tk.type == TK_FLT)
            printf("<FLT, %g>\n", x.tk.lexeme.f);
        else if (x.tk.type == TK_ID)
            printf("<ID, %s>\n", x.tk.lexeme.s->str);
        else if (x.tk.type == TK_INT)
            printf("<INT, %lld>\n", x.tk.lexeme.i);
        else if (x.tk.type == TK_STR)
            printf("<STR (%zu), %s>\n", x.tk.lexeme.s->l, x.tk.lexeme.s->str);
    }
}

int main(int argc, char **argv) {
    if (argc == 2)
        print_tk_stream(argv[1]);
    else
        print_tk_stream(stringify_file(argv[2]));
    return 0;
}
