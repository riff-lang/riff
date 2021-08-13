#include "disas.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

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
    [OP_FLDA]    = { "flda",     0 },
    [OP_FLDV]    = { "fldv",     0 },
    [OP_GBLA0]   = { "gbla   0", 0 },
    [OP_GBLA1]   = { "gbla   1", 0 },
    [OP_GBLA2]   = { "gbla   2", 0 },
    [OP_GBLA]    = { "gbla",     1 },
    [OP_GBLV0]   = { "gblv   0", 0 },
    [OP_GBLV1]   = { "gblv   1", 0 },
    [OP_GBLV2]   = { "gblv   2", 0 },
    [OP_GBLV]    = { "gblv",     1 },
    [OP_GE]      = { "ge",       0 },
    [OP_GT]      = { "gt",       0 },
    [OP_IDXA1]   = { "idxa",     0 },
    [OP_IDXA]    = { "idxa",     1 },
    [OP_IDXV1]   = { "idxv",     0 },
    [OP_IDXV]    = { "idxv",     1 },
    [OP_IMM0]    = { "imm    0", 0 },
    [OP_IMM16]   = { "imm",      2 },
    [OP_IMM1]    = { "imm    1", 0 },
    [OP_IMM2]    = { "imm    2", 0 },
    [OP_IMM8]    = { "imm",      1 },
    [OP_ITERKV]  = { "iterkv",   2 },
    [OP_ITERV]   = { "iterv",    2 },
    [OP_JMP16]   = { "jmp",      2 },
    [OP_JMP8]    = { "jmp",      1 },
    [OP_JNZ16]   = { "jnz",      2 },
    [OP_JNZ8]    = { "jnz",      1 },
    [OP_JZ16]    = { "jz",       2 },
    [OP_JZ8]     = { "jz",       1 },
    [OP_LCLA0]   = { "lcla   0", 0 },
    [OP_LCLA1]   = { "lcla   1", 0 },
    [OP_LCLA2]   = { "lcla   2", 0 },
    [OP_LCLA]    = { "lcla",     1 },
    [OP_LCLV0]   = { "lclv   0", 0 },
    [OP_LCLV1]   = { "lclv   1", 0 },
    [OP_LCLV2]   = { "lclv   2", 0 },
    [OP_LCLV]    = { "lclv",     1 },
    [OP_LEN]     = { "len",      0 },
    [OP_LE]      = { "le",       0 },
    [OP_LNOT]    = { "lnot",     0 },
    [OP_LOOP16]  = { "loop",     2 },
    [OP_LOOP8]   = { "loop",     1 },
    [OP_LT]      = { "lt",       0 },
    [OP_MATCH]   = { "match",    0 },
    [OP_MODX]    = { "modx",     0 },
    [OP_MOD]     = { "mod",      0 },
    [OP_MULX]    = { "mulx",     0 },
    [OP_MUL]     = { "mul",      0 },
    [OP_NEG]     = { "neg",      0 },
    [OP_NE]      = { "ne",       0 },
    [OP_NMATCH]  = { "nmatch",   0 },
    [OP_NOT]     = { "not",      0 },
    [OP_NULL]    = { "null",     0 },
    [OP_NUM]     = { "num",      0 },
    [OP_ORX]     = { "orx",      0 },
    [OP_OR]      = { "or",       0 },
    [OP_POPI]    = { "pop",      1 },
    [OP_POPL]    = { "popl",     0 },
    [OP_POP]     = { "pop",      0 },
    [OP_POSTDEC] = { "pstdec",   0 },
    [OP_POSTINC] = { "pstinc",   0 },
    [OP_POWX]    = { "powx",     0 },
    [OP_POW]     = { "pow",      0 },
    [OP_PREDEC]  = { "predec",   0 },
    [OP_PREINC]  = { "preinc",   0 },
    [OP_PRINT1]  = { "print",    0 },
    [OP_PRINT]   = { "print",    1 },
    [OP_PUSHK0]  = { "pushk  0", 0 },
    [OP_PUSHK1]  = { "pushk  1", 0 },
    [OP_PUSHK2]  = { "pushk  2", 0 },
    [OP_PUSHK]   = { "pushk",    1 },
    [OP_RET1]    = { "ret    1", 0 },
    [OP_RET]     = { "ret",      0 },
    [OP_SEQE]    = { "seqe",     0 },
    [OP_SEQF]    = { "seqf",     0 },
    [OP_SEQT]    = { "seqt",     0 },
    [OP_SEQ]     = { "seq",      0 },
    [OP_SET]     = { "set",      0 },
    [OP_SHLX]    = { "shlx",     0 },
    [OP_SHL]     = { "shl",      0 },
    [OP_SHRX]    = { "shrx",     0 },
    [OP_SHR]     = { "shr",      0 },
    [OP_SSEQE]   = { "sseqe",    0 },
    [OP_SSEQF]   = { "sseqf",    0 },
    [OP_SSEQT]   = { "sseqt",    0 },
    [OP_SSEQ]    = { "sseq",     0 },
    [OP_SUBX]    = { "subx",     0 },
    [OP_SUB]     = { "sub",      0 },
    [OP_TBL0]    = { "tbl    0", 0 },
    [OP_TBLK]    = { "tbl",      1 },
    [OP_TBL]     = { "tbl",      1 },
    [OP_TCALL]   = { "tcall",    1 },
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

#define INST0       "%*d: %02x       %s\n"
#define INST0DEREF  "%*d: %02x       %-6s      // %s\n"
#define INST1       "%*d: %02x %02x    %-6s %d\n"
#define INST1DEREF  "%*d: %02x %02x    %-6s %-6d // %s\n"
#define INST1ADDR   "%*d: %02x %02x    %-6s %-6d // %d\n"
#define INST2       "%*d: %02x %02x %02x %-6s %d\n"
#define INST2ADDR   "%*d: %02x %02x %02x %-6s %-6d // %d\n"

#define OPND(x)     (c->k[b1].u.x)
#define OPND0(x)    (c->k[0].u.x)
#define OPND1(x)    (c->k[1].u.x)
#define OPND2(x)    (c->k[2].u.x)

static int is_jump8(int op) {
    return op == OP_JMP8 || op == OP_JZ8 || op == OP_JNZ8 ||
           op == OP_XJZ8 || op == OP_XJNZ8;
}

static int is_jump16(int op) {
    return op == OP_JMP16 || op == OP_JZ16 || op == OP_JNZ16 ||
           op == OP_XJZ16 || op == OP_XJNZ16 ||
           op == OP_ITERV || op == OP_ITERKV;
}

// TODO This function is way too big for its own good
static void d_code_obj(rf_code *c, int ipw) {
    int sz  = c->n;
    int ip  = 0;

    char s[80];
    int b0, b1;
    while (ip < sz) {
        b0 = c->code[ip];
        if (OP_ARITY) {
            b1 = c->code[ip+1];
            switch (b0) {
            case OP_PUSHK:
            case OP_TBLK:
                switch (c->k[b1].type) {
                case TYPE_FLT:
                    sprintf(s, "%g", OPND(f));
                    break;
                case TYPE_INT:
                    sprintf(s, "%"PRId64, OPND(i));
                    break;
                case TYPE_STR:
                    sprintf(s, "\"%s\"", OPND(s->str));
                    break;
                case TYPE_RE:
                    sprintf(s, "regex: %p", OPND(r));
                    break;
                case TYPE_RFN:
                    sprintf(s, "fn: %p", OPND(fn));
                    break;
                default:
                    break;
                }
                printf(INST1DEREF, ipw, ip, b0, b1, OP_MNEMONIC, b1, s);
                break;
            case OP_GBLA: case OP_GBLV:
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
                } else if (b0 == OP_LOOP8) {
                    printf(INST1ADDR, ipw, ip, b0, b1, OP_MNEMONIC, -b1, ip - (uint8_t) b1);
                } else if (b0 == OP_LOOP16) {
                    int b2 = c->code[ip+2];
                    int a = (b1 << 8) + b2;
                    printf(INST2ADDR, ipw, ip, b0, b1, b2, OP_MNEMONIC,
                            -a, ip - a);
                    ip += 1;
                } else if (b0 == OP_IMM16) {
                    int b2 = c->code[ip+2];
                    int a = (b1 << 8) + b2;
                    printf(INST2, ipw, ip, b0, b1, b2, OP_MNEMONIC, a);
                    ip += 1;

                } else {
                    printf(INST1, ipw, ip, b0, b1, OP_MNEMONIC, b1);
                }
                break;
            }
            ip += 2;
        } else if (b0 >= OP_PUSHK0 && b0 <= OP_GBLV2) {
            switch (b0) {
            case OP_PUSHK0:
                switch (c->k[0].type) {
                case TYPE_FLT:
                    sprintf(s, "%g", OPND0(f));
                    break;
                case TYPE_INT:
                    sprintf(s, "%"PRId64, OPND0(i));
                    break;
                case TYPE_STR:
                    sprintf(s, "\"%s\"", OPND0(s->str));
                    break;
                case TYPE_RE:
                    sprintf(s, "regex: %p", OPND0(r));
                    break;
                case TYPE_RFN:
                    sprintf(s, "fn: %p", OPND0(fn));
                    break;
                default:
                    break;
                }
                printf(INST0DEREF, ipw, ip, b0, OP_MNEMONIC, s);
                break;
            case OP_PUSHK1:
                switch (c->k[1].type) {
                case TYPE_FLT:
                    sprintf(s, "%g", OPND1(f));
                    break;
                case TYPE_INT:
                    sprintf(s, "%"PRId64, OPND1(i));
                    break;
                case TYPE_STR:
                    sprintf(s, "\"%s\"", OPND1(s->str));
                    break;
                case TYPE_RE:
                    sprintf(s, "regex: %p", OPND1(r));
                    break;
                case TYPE_RFN:
                    sprintf(s, "fn: %p", OPND1(fn));
                    break;
                default:
                    break;
                }
                printf(INST0DEREF, ipw, ip, b0, OP_MNEMONIC, s);
                break;
            case OP_PUSHK2:
                switch (c->k[2].type) {
                case TYPE_FLT:
                    sprintf(s, "%g", OPND2(f));
                    break;
                case TYPE_INT:
                    sprintf(s, "%"PRId64, OPND2(i));
                    break;
                case TYPE_STR:
                    sprintf(s, "\"%s\"", OPND2(s->str));
                    break;
                case TYPE_RE:
                    sprintf(s, "regex: %p", OPND2(r));
                    break;
                case TYPE_RFN:
                    sprintf(s, "fn: %p", OPND2(fn));
                    break;
                default:
                    break;
                }
                printf(INST0DEREF, ipw, ip, b0, OP_MNEMONIC, s);
                break;
            case OP_GBLA0: case OP_GBLV0:
                sprintf(s, "%s", OPND0(s->str));
                printf(INST0DEREF, ipw, ip, b0, OP_MNEMONIC, s);
                break;
            case OP_GBLA1: case OP_GBLV1:
                sprintf(s, "%s", OPND1(s->str));
                printf(INST0DEREF, ipw, ip, b0, OP_MNEMONIC, s);
                break;
            case OP_GBLA2: case OP_GBLV2:
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

void d_prog(rf_env *e) {
    // Calculate width for the IP in the disassembly
    int w = (int) log10(e->main.code->n) + 1;
    for (int i = 0; i < e->nf; ++i) {
        int fw = (int) log10(e->fn[i]->code->n) + 1;
        w = w < fw ? fw : w;
    }

    printf("source:%s @ %p -> %d bytes\n",
           e->pname,
           e->main.code,
           e->main.code->n);
    d_code_obj(e->main.code, w);
    for (int i = 0; i < e->nf; ++i) {
        printf("\n");
        if (e->fn[i]->name->hash) {
            printf("fn %s @ %p -> %d bytes\n",
                   e->fn[i]->name->str,
                   e->fn[i]->code,
                   e->fn[i]->code->n);
        } else {
            printf("%s @ %p -> %d bytes\n",
                   e->fn[i]->name->str,
                   e->fn[i]->code,
                   e->fn[i]->code->n);
        }
        d_code_obj(e->fn[i]->code, w);
    }
}

static const char *tokenstr[] = {
    [TK_AND] =
    "&&", "--", "..", "==", ">=", "++", "<=", "!=", "!~", "||", "**",
    "<<", ">>", "+=", "&=", "#=", "/=", "%=", "*=", "|=", "**=",
    "<<=", ">>=", "-=", "^=",
    "break", "continue", "do", "elif", "else", "fn", "for",
    "if", "in", "local", "loop", "print", "return", "while"
};

#define OPTR_STR "<Operator, %s >"
#define KW_STR   "<Keyword, %s>"

static void tk2str(rf_token *tk, char *s) {
    if (tk->kind < TK_AND)
        sprintf(s, "<Operator, %c >", tk->kind);
    else if (tk->kind < TK_BREAK)
        sprintf(s, OPTR_STR, tokenstr[tk->kind]);
    else if (tk->kind < TK_FLT)
        sprintf(s, KW_STR, tokenstr[tk->kind]);
    else {
        switch(tk->kind) {
        case TK_NULL:
            sprintf(s, "<null>");
            break;
        case TK_FLT:
            sprintf(s, "<Float, %f (0x%a)>", tk->lexeme.f, tk->lexeme.f);
            break;
        case TK_INT:
            sprintf(s, "<Int, %"PRId64" (0x%"PRIx64")>", tk->lexeme.i, tk->lexeme.i);
            break;
        case TK_STR:
            sprintf(s, "<String (%zu, 0x%x), %s>", tk->lexeme.s->l,
                                                   tk->lexeme.s->hash,
                                                   tk->lexeme.s->str);
            break;
        case TK_RE:
            sprintf(s, "<Regex>");
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

void d_tk_stream(const char *src) {
    rf_lexer x;
    x_init(&x, src);
    do {
        char s[1024];
        tk2str(&x.tk, s);
        puts(s);
    } while (!x_adv(&x));
}
