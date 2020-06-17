#include <stdio.h>

#include "disas.h"

#define OPTR_STR "<Operator, %s >"
#define KW_STR   "<Keyword, %s>"

static void tk2str(token_t *tk, char *s) {
    switch(tk->type) {
    case '+': case '-': case '*': case '/': case '%':
    case '!': case '&': case '|': case '^': case '<':
    case '>': case '=': case '?': case ':': case '#':
    case '(': case ')': case '[': case ']': case '{':
    case '}': case '~': case ';': case ',':
        sprintf(s, "<Operator, %c >", tk->type);
        break;
    case TK_AND:        sprintf(s, OPTR_STR, "&&");  break;
    case TK_CAT:        sprintf(s, OPTR_STR, "::");  break;
    case TK_DEC:        sprintf(s, OPTR_STR, "--");  break;
    case TK_EQ:         sprintf(s, OPTR_STR, "==");  break;
    case TK_GE:         sprintf(s, OPTR_STR, ">=");  break;
    case TK_INC:        sprintf(s, OPTR_STR, "++");  break;
    case TK_LE:         sprintf(s, OPTR_STR, "<=");  break;
    case TK_NE:         sprintf(s, OPTR_STR, "!=");  break;
    case TK_OR:         sprintf(s, OPTR_STR, "||");  break;
    case TK_POW:        sprintf(s, OPTR_STR, "**");  break;
    case TK_SHL:        sprintf(s, OPTR_STR, "<<");  break;
    case TK_SHR:        sprintf(s, OPTR_STR, ">>");  break;
    case TK_ADD_ASSIGN: sprintf(s, OPTR_STR, "+=");  break;
    case TK_AND_ASSIGN: sprintf(s, OPTR_STR, "&=");  break;
    case TK_DIV_ASSIGN: sprintf(s, OPTR_STR, "/=");  break;
    case TK_MOD_ASSIGN: sprintf(s, OPTR_STR, "%=");  break;
    case TK_MUL_ASSIGN: sprintf(s, OPTR_STR, "*=");  break;
    case TK_OR_ASSIGN:  sprintf(s, OPTR_STR, "|=");  break;
    case TK_SUB_ASSIGN: sprintf(s, OPTR_STR, "-=");  break;
    case TK_XOR_ASSIGN: sprintf(s, OPTR_STR, "^=");  break;
    case TK_CAT_ASSIGN: sprintf(s, OPTR_STR, "::="); break;
    case TK_POW_ASSIGN: sprintf(s, OPTR_STR, "**="); break;
    case TK_SHL_ASSIGN: sprintf(s, OPTR_STR, "<<="); break;
    case TK_SHR_ASSIGN: sprintf(s, OPTR_STR, ">>="); break;

    case TK_BREAK:  sprintf(s, KW_STR, "break");  break;
    case TK_ELSE:   sprintf(s, KW_STR, "else");   break;
    case TK_EXIT:   sprintf(s, KW_STR, "exit");   break;
    case TK_FN:     sprintf(s, KW_STR, "fn");     break;
    case TK_FOR:    sprintf(s, KW_STR, "for");    break;
    case TK_IF:     sprintf(s, KW_STR, "if");     break;
    case TK_LOCAL:  sprintf(s, KW_STR, "local");  break;
    case TK_PRINT:  sprintf(s, KW_STR, "print");  break;
    case TK_RETURN: sprintf(s, KW_STR, "return"); break;
    case TK_WHILE:  sprintf(s, KW_STR, "while");  break;

    case TK_FLT:
        sprintf(s, "<Float, %f (0x%a)>", tk->lexeme.f, tk->lexeme.f);
        break;
    case TK_INT:
        sprintf(s, "<Int, %lld (0x%llx)>", tk->lexeme.i, tk->lexeme.i);
        break;
    case TK_STR:
        sprintf(s, "<String (%zu), %s>", tk->lexeme.s->l, tk->lexeme.s->str);
        break;
    case TK_ID:
        sprintf(s, "<Identifier, %s>", tk->lexeme.s->str);
        break;
    default:
        sprintf(s, "Unidentified token");
        break;
    }
}

#undef OPTR_STR
#undef KW_STR

void print_tk_stream(const char *src) {
    lexer_t x;
    x_init(&x, src);
    while (!x_next(&x)) {
        char s[1024];
        tk2str(&x.tk, s);
        printf("%s\n", s);
    }
}
