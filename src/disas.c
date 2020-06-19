#include <stdio.h>

#include "disas.h"

static struct {
    const char *mnemonic;
    int         arity;
} opcode_info[] = {
    [OP_JMP]   = { "jmp",   1 },
    [OP_JZ]    = { "jz",    1 },
    [OP_JNZ]   = { "jnz",   1 },
    [OP_ADD]   = { "add",   0 },
    [OP_SUB]   = { "sub",   0 },
    [OP_MUL]   = { "mul",   0 },
    [OP_DIV]   = { "div",   0 },
    [OP_MOD]   = { "mod",   0 },
    [OP_POW]   = { "pow",   0 },
    [OP_AND]   = { "and",   0 },
    [OP_OR]    = { "or",    0 },
    [OP_XOR]   = { "xor",   0 },
    [OP_SHL]   = { "shl",   0 },
    [OP_SHR]   = { "shr",   0 },
    [OP_NUM]   = { "num",   0 },
    [OP_NEG]   = { "neg",   0 },
    [OP_NOT]   = { "not",   0 },
    [OP_EQ]    = { "eq",    0 },
    [OP_GT]    = { "gt",    0 },
    [OP_GE]    = { "ge",    0 },
    [OP_LT]    = { "lt",    0 },
    [OP_LE]    = { "le",    0 },
    [OP_LAND]  = { "land",  0 },
    [OP_LOR]   = { "lor",   0 },
    [OP_LNOT]  = { "lnot",  0 },
    [OP_CALL]  = { "call",  1 },
    [OP_CAT]   = { "cat",   0 },
    [OP_INC]   = { "inc",   0 },
    [OP_DEC]   = { "dec",   0 },
    [OP_LEN]   = { "len",   0 },
    [OP_POP]   = { "pop",   0 },
    [OP_PUSHA] = { "pusha", 1 },
    [OP_PUSHI] = { "pushi", 1 },
    [OP_PUSHK] = { "pushk", 1 },
    [OP_PUSHV] = { "pushv", 1 },
    [OP_RET]   = { "ret",   1 },
    [OP_RET0]  = { "ret",   0 },
    [OP_SET]   = { "set",   0 },
    [OP_PRINT] = { "print", 0 }
};

void d_code_chunk(chunk_t *c) {
    printf("%s, %d bytes\n", c->name, c->size);
    int i = 0;
    char s[80];
    while (i < c->size) {
        if (opcode_info[c->code[i]].arity) {
            switch (c->code[i]) {
            case OP_PUSHK:
                switch (c->k.k[c->code[i+1]].type) {
                case TYPE_FLT: {
                    sprintf(s, "(flt) %g", c->k.k[c->code[i+1]].u.f);
                    break;
                } case TYPE_INT: {
                    sprintf(s, "(int) %lld", c->k.k[c->code[i+1]].u.i);
                    break;
                } case TYPE_STR: {
                    sprintf(s, "(str) %s", c->k.k[c->code[i+1]].u.s->str);
                    break;
                } default: {
                    break;
                }
                }
                printf("%3d: %02X %02X %-6s %2x ; %s\n",
                        i,
                        c->code[i],
                        c->code[i+1],
                        opcode_info[c->code[i]].mnemonic,
                        c->code[i+1],
                        s);
                break;
            default:
                printf("%3d: %02X %02X %-6s %2x\n",
                        i,
                        c->code[i],
                        c->code[i+1],
                        opcode_info[c->code[i]].mnemonic,
                        c->code[i+1]);
                break;
            }
            i += 2;
        } else {
            printf("%3d: %02X    %-6s\n",
                    i,
                    c->code[i], 
                    opcode_info[c->code[i]].mnemonic);
            i += 1;
        }
    }
}

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

void d_tk_stream(const char *src) {
    lexer_t x;
    x_init(&x, src);
    do {
        char s[1024];
        tk2str(&x.tk, s);
        puts(s);
    } while (!x_next(&x));
}
