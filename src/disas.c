#include <stdio.h>

#include "disas.h"

static struct {
    const char *mnemonic;
    int         arity;
} opcode_info[] = {
    [OP_ADDX]    = { "addx",     0 },
    [OP_ADD]     = { "add",      0 },
    [OP_ANDX]    = { "andx",     0 },
    [OP_AND]     = { "and",      0 },
    [OP_CALL]    = { "call",     1 },
    [OP_CATX]    = { "catx",     0 },
    [OP_CAT]     = { "cat",      0 },
    [OP_DIVX]    = { "divx",     0 },
    [OP_DIV]     = { "div",      0 },
    [OP_EQ]      = { "eq",       0 },
    [OP_EXIT]    = { "exit",     0 },
    [OP_GE]      = { "ge",       0 },
    [OP_GT]      = { "gt",       0 },
    [OP_IDXA]    = { "idxa",     0 },
    [OP_IDXV]    = { "idxv",     0 },
    [OP_JMP16]   = { "jmp",      2 },
    [OP_JMP8]    = { "jmp",      1 },
    [OP_JNZ16]   = { "jnz",      2 },
    [OP_JNZ8]    = { "jnz",      1 },
    [OP_JZ16]    = { "jz",       2 },
    [OP_JZ8]     = { "jz",       1 },
    [OP_LEN]     = { "len",      0 },
    [OP_LE]      = { "le",       0 },
    [OP_LNOT]    = { "lnot",     0 },
    [OP_LT]      = { "lt",       0 },
    [OP_MODX]    = { "modx",     0 },
    [OP_MOD]     = { "mod",      0 },
    [OP_MULX]    = { "mulx",     0 },
    [OP_MUL]     = { "mul",      0 },
    [OP_NEG]     = { "neg",      0 },
    [OP_NE]      = { "ne",       0 },
    [OP_NOT]     = { "not",      0 },
    [OP_NUM]     = { "num",      0 },
    [OP_ORX]     = { "orx",      0 },
    [OP_OR]      = { "or",       0 },
    [OP_POP]     = { "pop",      0 },
    [OP_POSTDEC] = { "pstdec",   0 },
    [OP_POSTINC] = { "pstinc",   0 },
    [OP_POWX]    = { "powx",     0 },
    [OP_POW]     = { "pow",      0 },
    [OP_PREDEC]  = { "predec",   0 },
    [OP_PREINC]  = { "preinc",   0 },
    [OP_PRINT]   = { "print",    0 },
    [OP_PUSH0]   = { "pushi  0", 0 },
    [OP_PUSH1]   = { "pushi  1", 0 },
    [OP_PUSH2]   = { "pushi  2", 0 },
    [OP_PUSHA0]  = { "pusha  0", 0 },
    [OP_PUSHA1]  = { "pusha  1", 0 },
    [OP_PUSHA2]  = { "pusha  2", 0 },
    [OP_PUSHA]   = { "pusha",    1 },
    [OP_PUSHI]   = { "pushi",    1 },
    [OP_PUSHK0]  = { "pushk  0", 0 },
    [OP_PUSHK1]  = { "pushk  1", 0 },
    [OP_PUSHK2]  = { "pushk  2", 0 },
    [OP_PUSHK]   = { "pushk",    1 },
    [OP_PUSHV0]  = { "pushv  0", 0 },
    [OP_PUSHV1]  = { "pushv  1", 0 },
    [OP_PUSHV2]  = { "pushv  2", 0 },
    [OP_PUSHV]   = { "pushv",    1 },
    [OP_RET1]    = { "ret    1", 0 },
    [OP_RET]     = { "ret",      0 },
    [OP_SET]     = { "set",      0 },
    [OP_SHLX]    = { "shlx",     0 },
    [OP_SHL]     = { "shl",      0 },
    [OP_SHRX]    = { "shrx",     0 },
    [OP_SHR]     = { "shr",      0 },
    [OP_SUBX]    = { "subx",     0 },
    [OP_SUB]     = { "sub",      0 },
    [OP_TEST]    = { "test",     0 },
    [OP_XJNZ16]  = { "xjnz",     2 },
    [OP_XJNZ8]   = { "xjnz",     1 },
    [OP_XJZ16]   = { "xjz",      2 },
    [OP_XJZ8]    = { "xjz",      1 },
    [OP_XORX]    = { "xorx",     0 },
    [OP_XOR]     = { "xor",      0 }
};

#define OP_ARITY    (opcode_info[b0].arity)
#define OP_MNEMONIC (opcode_info[b0].mnemonic)

#define INST0       "%*d| %02x       %s\n"
#define INST0DEREF  "%*d| %02x       %-6s      // %s\n"
#define INST1       "%*d| %02x %02x    %-6s %d\n"
#define INST1DEREF  "%*d| %02x %02x    %-6s %-6d // %s\n"
#define INST1ADDR   "%*d| %02x %02x    %-6s %-6d // %d\n"
#define INST2ADDR   "%*d| %02x %02x %02x %-6s %-6d // %d\n"

#define OPND(x)     (c->k.v[b1]->u.x)
#define OPND0(x)    (c->k.v[0]->u.x)
#define OPND1(x)    (c->k.v[1]->u.x)
#define OPND2(x)    (c->k.v[2]->u.x)

static int is_jump8(int op) {
    return op == OP_JMP8 || op == OP_JZ8 || op == OP_JNZ8 ||
           op == OP_XJZ8 || op == OP_XJNZ8;
}

static int is_jump16(int op) {
    return op == OP_JMP16 || op == OP_JZ16 || op == OP_JNZ16 ||
           op == OP_XJZ16 || op == OP_XJNZ16;
}

// TODO This function is way too big for its own good
void d_code_chunk(code_t *c) {
    int sz  = c->n;
    int ipw = sz <= 10   ? 1
            : sz <= 100  ? 2
            : sz <= 1000 ? 3
            : 4;
    int ip  = 0;

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
                    sprintf(s, "%g", OPND(f));
                    break;
                case TYPE_INT:
                    sprintf(s, "%lld", OPND(i));
                    break;
                case TYPE_STR:
                    sprintf(s, "\"%s\"", OPND(s->str));
                    break;
                default:
                    break;
                }
                printf(INST1DEREF, ipw, ip, b0, b1, OP_MNEMONIC, b1, s);
                break;
            case OP_PUSHA: case OP_PUSHV:
                sprintf(s, "%s", OPND(s->str));
                printf(INST1DEREF, ipw, ip, b0, b1, OP_MNEMONIC, b1, s);
                break;
            default:
                if (is_jump8(b0)) {
                    printf(INST1ADDR, ipw, ip, b0, b1, OP_MNEMONIC, (int8_t) b1, ip + (int8_t) b1);
                } else if (is_jump16(b0)) {
                    int b2 = c->code[ip+2];
                    int16_t a = (b1 << 8) + b2;
                    printf(INST2ADDR, ipw, ip, b0, b1, b2, OP_MNEMONIC,
                            a, ip + a);
                    ip += 1;
                } else {
                    printf(INST1, ipw, ip, b0, b1, OP_MNEMONIC, b1);
                }
                break;
            }
            ip += 2;
        } else if (b0 >= OP_PUSHK0 && b0 <= OP_PUSHV2) {
            switch (b0) {
            case OP_PUSHK0:
                switch (c->k.v[0]->type) {
                case TYPE_FLT:
                    sprintf(s, "%g", OPND0(f));
                    break;
                case TYPE_INT:
                    sprintf(s, "%lld", OPND0(i));
                    break;
                case TYPE_STR:
                    sprintf(s, "\"%s\"", OPND0(s->str));
                    break;
                default:
                    break;
                }
                printf(INST0DEREF, ipw, ip, b0, OP_MNEMONIC, s);
                break;
            case OP_PUSHK1:
                switch (c->k.v[1]->type) {
                case TYPE_FLT:
                    sprintf(s, "%g", OPND1(f));
                    break;
                case TYPE_INT:
                    sprintf(s, "%lld", OPND1(i));
                    break;
                case TYPE_STR:
                    sprintf(s, "\"%s\"", OPND1(s->str));
                    break;
                default:
                    break;
                }
                printf(INST0DEREF, ipw, ip, b0, OP_MNEMONIC, s);
                break;
            case OP_PUSHK2:
                switch (c->k.v[2]->type) {
                case TYPE_FLT:
                    sprintf(s, "%g", OPND2(f));
                    break;
                case TYPE_INT:
                    sprintf(s, "%lld", OPND2(i));
                    break;
                case TYPE_STR:
                    sprintf(s, "\"%s\"", OPND2(s->str));
                    break;
                default:
                    break;
                }
                printf(INST0DEREF, ipw, ip, b0, OP_MNEMONIC, s);
                break;
            case OP_PUSHA0: case OP_PUSHV0:
                sprintf(s, "%s", OPND0(s->str));
                printf(INST0DEREF, ipw, ip, b0, OP_MNEMONIC, s);
                break;
            case OP_PUSHA1: case OP_PUSHV1:
                sprintf(s, "%s", OPND1(s->str));
                printf(INST0DEREF, ipw, ip, b0, OP_MNEMONIC, s);
                break;
            case OP_PUSHA2: case OP_PUSHV2:
                sprintf(s, "%s", OPND2(s->str));
                printf(INST0DEREF, ipw, ip, b0, OP_MNEMONIC, s);
                break;
            default:
                break;
            }
            ip += 1;
        } else {
            printf(INST0, ipw, ip, b0, OP_MNEMONIC);
            ip += 1;
        }
    }
}

#undef OP_ARITY
#undef OP_MNEMONIC
#undef INST0
#undef INST0DEREF
#undef INST1
#undef INST1DEREF
#undef INST1ADDR
#undef INST2ADDR
#undef OPND
#undef OPND0
#undef OPND1
#undef OPND2

static const char *tokenstr[] = {
    [TK_AND] =
    "&&", "::", "--", "==", ">=", "++", "<=", "!=", "||", "**",
    "<<", ">>", "+=", "&=", "::=", "/=", "%=", "*=", "|=", "**=",
    "<<=", ">>=", "-=", "^=",
    "break", "do", "else", "exit", "fn", "for", "if", "local",
    "print", "return", "while"
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
