#include "disas.h"

#include "conf.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

static struct {
    const char *mnemonic;
    int         arity;
} opcode_info[] = {
#define OPCODE(x,y,z) { z, y }
#include "opcodes.h"
};

#define OP_ARITY    (opcode_info[b0].arity)
#define MNEMONIC    (opcode_info[b0].mnemonic)

#define INST0       "%*d: %02x       %s\n"
#define INST0DEREF  "%*d: %02x       %-6s      // %s\n"
#define INST1       "%*d: %02x %02x    %-6s %d\n"
#define INST1DEREF  "%*d: %02x %02x    %-6s %-6d // %s\n"
#define INST1ADDR   "%*d: %02x %02x    %-6s %-6d // %d\n"
#define INST2       "%*d: %02x %02x %02x %-6s %d\n"
#define INST2ADDR   "%*d: %02x %02x %02x %-6s %-6d // %d\n"

#define OPND(x)     (c->k[b1].x)
#define OPND0(x)    (c->k[0].x)
#define OPND1(x)    (c->k[1].x)
#define OPND2(x)    (c->k[2].x)

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
    int sz = c->n;
    int ip = 0;

    char s[STR_BUF_SZ];
    uint8_t b0, b1, b2;
    while (ip < sz) {
        b0 = c->code[ip];
        if (OP_ARITY) {
            b1 = c->code[ip+1];
            switch (b0) {
            case OP_CONST:
            case OP_TABK:
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
                printf(INST1DEREF, ipw, ip, b0, b1, MNEMONIC, b1, s);
                break;
            case OP_GBLA: case OP_GBLV:
            case OP_SIDXA: case OP_SIDXV:
                sprintf(s, "%s", OPND(s->str));
                printf(INST1DEREF, ipw, ip, b0, b1, MNEMONIC, b1, s);
                break;
            default:
                if (is_jump8(b0)) {
                    printf(INST1ADDR, ipw, ip, b0, b1, MNEMONIC, (int8_t) b1, ip + (int8_t) b1);
                } else if (is_jump16(b0)) {
                    b2 = c->code[ip+2];
                    int16_t a = (b1 << 8) + b2;
                    printf(INST2ADDR, ipw, ip, b0, b1, b2, MNEMONIC, a, ip + a);
                    ip += 1;
                } else if (b0 == OP_LOOP8) {
                    printf(INST1ADDR, ipw, ip, b0, b1, MNEMONIC, -b1, ip - (uint8_t) b1);
                } else if (b0 == OP_LOOP16) {
                    b2 = c->code[ip+2];
                    int a = (b1 << 8) + b2;
                    printf(INST2ADDR, ipw, ip, b0, b1, b2, MNEMONIC, -a, ip - a);
                    ip += 1;
                } else if (b0 == OP_IMM16) {
                    b2 = c->code[ip+2];
                    int a = (b1 << 8) + b2;
                    printf(INST2, ipw, ip, b0, b1, b2, MNEMONIC, a);
                    ip += 1;
                } else {
                    printf(INST1, ipw, ip, b0, b1, MNEMONIC, b1);
                }
                break;
            }
            ip += 2;
        } else if (b0 >= OP_CONST0 && b0 <= OP_GBLV2) {
            switch (b0) {
            case OP_CONST0:
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
                printf(INST0DEREF, ipw, ip, b0, MNEMONIC, s);
                break;
            case OP_CONST1:
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
                printf(INST0DEREF, ipw, ip, b0, MNEMONIC, s);
                break;
            case OP_CONST2:
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
                printf(INST0DEREF, ipw, ip, b0, MNEMONIC, s);
                break;
            case OP_GBLA0: case OP_GBLV0:
                sprintf(s, "%s", OPND0(s->str));
                printf(INST0DEREF, ipw, ip, b0, MNEMONIC, s);
                break;
            case OP_GBLA1: case OP_GBLV1:
                sprintf(s, "%s", OPND1(s->str));
                printf(INST0DEREF, ipw, ip, b0, MNEMONIC, s);
                break;
            case OP_GBLA2: case OP_GBLV2:
                sprintf(s, "%s", OPND2(s->str));
                printf(INST0DEREF, ipw, ip, b0, MNEMONIC, s);
                break;
            default:
                break;
            }
            ip += 1;
        } else {
            printf(INST0, ipw, ip, b0, MNEMONIC);
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

    printf("source:%s @ %p -> %d %s\n",
           e->pname,
           &e->main,
           e->main.code->n,
           e->main.code->n == 1 ? "byte" : "bytes");
    d_code_obj(e->main.code, w);
    for (int i = 0; i < e->nf; ++i) {
        rf_fn *f = e->fn[i];
        printf("\nfn %s @ %p -> %d %s\n",
               f->name->hash ? f->name->str : "<anonymous>",
               f,
               f->code->n,
               f->code->n == 1 ? "byte" : "bytes");
        d_code_obj(e->fn[i]->code, w);
    }
}
