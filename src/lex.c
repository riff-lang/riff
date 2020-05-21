#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "lex.h"

#define next(l) (l->src++)

static void lex_err(lexer_t *lex, const char *msg) {
    fprintf(stderr, "Lexical error at or near line %d: %s\n", lex->line, msg);
    exit(1);
}

static bool valid_alpha(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c == '_');
}

// TODO
static void read_char(lexer_t *lex) {
    char c;
    if (lex->current == '\\') {
        next(lex);
        switch (lex->current) {
        case 'n': c = '\n'; break;
        default:  lex_err(lex, "Invalid escape sequence");
        }
    } else
        c = lex->current;
    lex->token.lexeme.c = c;
    next(lex);
    if (lex->current != '\'')
        lex_err(lex, "Invalid character definition");
    next(lex);
}

static int tokenize(lexer_t *lex) {
    while (1) {
        switch (lex->current) {
        case '\n': case '\r':
            lex->line++;
            next(lex);
            break;
        case ' ': case '\t':
            next(lex);
            break;
        case '!':
            next(lex);
            return lex->current == '=' ? TK_NEQ : '!';
        case '"':
            // Read string
        case '%':
            next(lex);
            return lex->current == '=' ? TK_MOD_ASSIGN : '%';
        case '&':
            next(lex);
            return lex->current == '=' ? TK_AND_ASSIGN :
                   lex->current == '&' ? TK_AND : '&';
        case '\'':
            next(lex);
            read_char(lex);
            return TK_CHAR;
        case '*':
            // *, *=, **, **=
        case '+':
            next(lex);
            return lex->current == '=' ? TK_ADD_ASSIGN :
                   lex->current == '+' ? TK_INC : '+';
        case '-':
            next(lex);
            return lex->current == '=' ? TK_SUB_ASSIGN :
                   lex->current == '-' ? TK_DEC : '-';
        case '.':
            // ., .., ..., floating-point number
        case '/':
            // /, /=, //
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            // Read number
        case '<':
            // <, <=, <<, <<=
        case '=':
            next(lex);
            return lex->current == '=' ? TK_EQ : '=';
        case '>':
            // >, >=, >>, >>=
        case '^':
            next(lex);
            return lex->current == '=' ? TK_XOR_ASSIGN : '^';
        case '|':
            next(lex);
            return lex->current == '=' ? TK_OR_ASSIGN :
                   lex->current == '|' ? TK_OR : '|';
        case '~':
            next(lex);
            return lex->current == '=' ? TK_NOT_ASSIGN : '~';
        case '#': case '(': case ')': case ',': case ':':
        case ';': case '?': case ']': case '[': case '{':
        case '}': {
            int t = lex->current;
            next(lex);
            return t;
        }
        default:
            next(lex);
        }
    }
}

void next_token(lexer_t *lex) {
    // If a lookahead token already exists (not always the case),
    // simply assign the current token to the lookahead token.
    if (lex->lookahead.token != 0) {
        lex->token = lex->lookahead;
        lex->lookahead.token = 0;
    } else
        lex->token.token = tokenize(lex);
}
