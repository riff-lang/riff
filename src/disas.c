#include <stdio.h>

#include "disas.h"

static const char *tokenstr[] = {
    [TK_AND] =
    "&&", "::", "--", "==", ">=", "++", "<=", "!=", "||", "**",
    "<<", ">>", "+=", "&=", "/=", "%=", "*=", "|=", "-=", "^=",
    "::=", "**=", "<<=", ">>=",
    "break", "else", "exit", "fn", "for", "if", "local", "print",
    "return", "while"
};

#define OPTR_STR "<Operator, %s >"
#define KW_STR   "<Keyword, %s>"

static void tk2str(token_t *tk, char *s) {
    if (tk->type < TK_AND)
        sprintf(s, "<Operator, %c >", tk->type);
    else if (tk->type < TK_BREAK)
        sprintf(s, OPTR_STR, tokenstr[tk->type]);
    else if (tk->type < TK_FLT)
        sprintf(s, KW_STR, tokenstr[tk->type]);
    else {
        switch(tk->type) {
        case TK_FLT:
            sprintf(s, "<Float, %f (0x%a)>", tk->lexeme.f, tk->lexeme.f);
            break;
        case TK_INT:
            sprintf(s, "<Int, %lld (0x%llx)>", tk->lexeme.i, tk->lexeme.i);
            break;
        case TK_STR:
            sprintf(s, "<String (%zu, 0x%llx), %s>", tk->lexeme.s->l,
                                                     tk->lexeme.s->hash,
                                                     tk->lexeme.s->str);
            break;
        case TK_ID:
            sprintf(s, "<Identifier, %s>", tk->lexeme.s->str);
            break;
        default:
            sprintf(s, "Unidentified token");
            break;
        }
    }
}

#undef OPTR_STR
#undef KW_STR

void print_tk_stream(const char *src) {
    lexer_t x;
    x_init(&x, src);
    do {
        char s[1024];
        tk2str(&x.tk, s);
        puts(s);
    } while (!x_next(&x));
}
