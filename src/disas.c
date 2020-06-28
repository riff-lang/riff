#include <stdio.h>

#include "disas.h"

static struct {
    const char *mnemonic;
    int         arity;
} opcode_info[] = {
    [OP_ADD]   = { "add",     0 },
    [OP_AND]   = { "and",     0 },
    [OP_CALL]  = { "call",    1 },
    [OP_CAT]   = { "cat",     0 },
    [OP_DEC]   = { "dec",     0 },
    [OP_DIV]   = { "div",     0 },
    [OP_EQ]    = { "eq",      0 },
    [OP_GE]    = { "ge",      0 },
    [OP_GT]    = { "gt",      0 },
    [OP_INC]   = { "inc",     0 },
    [OP_JMP]   = { "jmp",     1 },
    [OP_JNZ]   = { "jnz",     1 },
    [OP_JZ]    = { "jz",      1 },
    [OP_LAND]  = { "land",    0 },
    [OP_LEN]   = { "len",     0 },
    [OP_LE]    = { "le",      0 },
    [OP_LNOT]  = { "lnot",    0 },
    [OP_LOR]   = { "lor",     0 },
    [OP_LT]    = { "lt",      0 },
    [OP_MOD]   = { "mod",     0 },
    [OP_MUL]   = { "mul",     0 },
    [OP_NE]    = { "ne",      0 },
    [OP_NEG]   = { "neg",     0 },
    [OP_NOT]   = { "not",     0 },
    [OP_NUM]   = { "num",     0 },
    [OP_OR]    = { "or",      0 },
    [OP_POP]   = { "pop",     0 },
    [OP_POW]   = { "pow",     0 },
    [OP_PRINT] = { "print",   0 },
    [OP_PUSH0] = { "pushi 0", 0 },
    [OP_PUSH1] = { "pushi 1", 0 },
    [OP_PUSH2] = { "pushi 2", 0 },
    [OP_PUSHI] = { "pushi",   1 },
    [OP_PUSHK] = { "pushk",   1 },
    [OP_PUSHS] = { "pushs",   1 },
    [OP_RET0]  = { "ret",     0 },
    [OP_RET]   = { "ret",     0 },
    [OP_SET]   = { "set",     0 },
    [OP_SHL]   = { "shl",     0 },
    [OP_SHR]   = { "shr",     0 },
    [OP_SUB]   = { "sub",     0 },
    [OP_XOR]   = { "xor",     0 }
};

#define OP_ARITY    (opcode_info[b0].arity)
#define OP_MNEMONIC (opcode_info[b0].mnemonic)

#define INST0       "%*d| %02x       %-5s\n"
#define INST1       "%*d| %02x %02x    %-5s %d\n"
#define INST1DEREF  "%*d| %02x %02x    %-5s %d    // %s\n"

#define OPND(x)     (c->k.v[b1]->u.x)

void d_code_chunk(code_t *c) {
    int sz = c->n;
    int ipw;
    if      (sz < 10)   ipw = 1;
    else if (sz < 100)  ipw = 2;
    else if (sz < 1000) ipw = 3;
    else                ipw = 4;
    int ip = 0;
    char s[80];
    int b0, b1;
    printf("code obj @ %p -> %d bytes\n", c, sz);
    while (ip < sz) {
        b0 = c->code[ip];
        if (OP_ARITY) {
            b1 = c->code[ip+1];
            switch (b0) {
            case OP_PUSHK:
                switch (c->k.v[b1]->type) {
                case TYPE_FLT:
                    sprintf(s, "(flt) %g", OPND(f));
                    break;
                case TYPE_INT:
                    sprintf(s, "(int) %lld", OPND(i));
                    break;
                case TYPE_STR:
                    sprintf(s, "(str) \"%s\"", OPND(s->str));
                    break;
                default:
                    break;
                }
                printf(INST1DEREF, ipw, ip, b0, b1, OP_MNEMONIC, b1, s);
                break;
            default:
                printf(INST1, ipw, ip, b0, b1, OP_MNEMONIC, b1);
                break;
            }
            ip += 2;
        } else {
            printf(INST0, ipw, ip, b0, OP_MNEMONIC);
            ip += 1;
        }
    }
}

#undef OP_ARITY
#undef OP_MNEMONIC
#undef INST0
#undef INST1
#undef INST1DEREF
#undef OPND

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
    if (tk->kind < TK_AND)
        sprintf(s, "<Operator, %c >", tk->kind);
    else if (tk->kind < TK_BREAK)
        sprintf(s, OPTR_STR, tokenstr[tk->kind]);
    else if (tk->kind < TK_FLT)
        sprintf(s, KW_STR, tokenstr[tk->kind]);
    else {
        switch(tk->kind) {
        case TK_FLT:
            sprintf(s, "<Float, %f (0x%a)>", tk->lexeme.f, tk->lexeme.f);
            break;
        case TK_INT:
            sprintf(s, "<Int, %lld (0x%llx)>", tk->lexeme.i, tk->lexeme.i);
            break;
        case TK_STR:
            sprintf(s, "<String (%zu, 0x%x), %s>", tk->lexeme.s->l,
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

void d_tk_stream(const char *src) {
    lexer_t x;
    x_init(&x, src);
    do {
        char s[1024];
        tk2str(&x.tk, s);
        puts(s);
    } while (!x_adv(&x));
}
