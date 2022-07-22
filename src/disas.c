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

// Convenience macros for format specifiers
#define F_IP        "%*d:  "
#define F_XX        "%02x "
#define F_MNEMONIC  " %s"
#define F_OPERAND   "%d"
#define F_LMNEMONIC " %-7s "
#define F_LOPERAND  "%-6d "
#define F_DEREF     " // %s"
#define F_ADDR      " // %d"

// Common format strings
#define INST0       F_IP F_XX "      "   F_MNEMONIC                     "\n"
#define INST0DEREF  F_IP F_XX "      "   F_LMNEMONIC "     "    F_DEREF "\n"
#define INST1       F_IP F_XX F_XX "   " F_LMNEMONIC F_OPERAND          "\n"
#define INST1DEREF  F_IP F_XX F_XX "   " F_LMNEMONIC F_LOPERAND F_DEREF "\n"
#define INST2       F_IP F_XX F_XX F_XX  F_LMNEMONIC F_OPERAND          "\n"

static int16_t toi16(uint8_t b1, uint8_t b2) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (int16_t) (b1 | (b2 << 8));
#else
    return (int16_t) ((b1 << 8) | b2);
#endif
}

static uint16_t tou16(uint8_t b1, uint8_t b2) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (uint16_t) (b1 | (b2 << 8));
#else
    return (uint16_t) ((b1 << 8) | b2);
#endif
}

// Wrap string in quotes
// TODO deconstruct bytes that correspond to escape sequences into their literal
//      forms
static void disas_tostr(riff_val *v, char *buf) {
    if (is_str(v)) {
        sprintf(buf, "\"%s\"", v->s->str);
    } else {
        riff_tostr(v, buf);
    }
}

static void d_code_obj(riff_code *c, int ipw) {
    int ip = 0;
    char s[STR_BUF_SZ];
    while (ip < c->n) {
        uint8_t  b0  = c->code[ip];
        uint8_t  b1  = c->code[ip+1];
        uint8_t  b2  = c->code[ip+2];
        int16_t  i16 = toi16(b1, b2);
        uint16_t u16 = tou16(b1, b2);
        if (OP_ARITY) {
            switch (b0) {
            case OP_CONST:
                disas_tostr(&c->k[b1], s);
                printf(INST1DEREF, ipw, ip, b0, b1, MNEMONIC, b1, s);
                break;
            case OP_TABK:
            case OP_GBLA:
            case OP_GBLV:
            case OP_SIDXA:
            case OP_SIDXV:
                riff_tostr(&c->k[b1], s);
                printf(INST1DEREF, ipw, ip, b0, b1, MNEMONIC, b1, s);
                break;
            case OP_JMP8:
            case OP_JZ8:
            case OP_JNZ8:
            case OP_XJZ8:
            case OP_XJNZ8:
                printf(INST1, ipw, ip, b0, b1, MNEMONIC, ip + (int8_t) b1);
                break;
            case OP_JMP16:
            case OP_JZ16:
            case OP_JNZ16:
            case OP_XJZ16:
            case OP_XJNZ16:
            case OP_ITERV:
            case OP_ITERKV:
                printf(INST2, ipw, ip, b0, b1, b2, MNEMONIC, ip + i16);
                break;
            case OP_LOOP8:
                printf(INST1, ipw, ip, b0, b1, MNEMONIC, ip - b1);
                break;
            case OP_LOOP16:
                printf(INST2, ipw, ip, b0, b1, b2, MNEMONIC, ip - u16);
                break;
            case OP_IMM16:
                printf(INST2, ipw, ip, b0, b1, b2, MNEMONIC, i16);
                break;
            default:
                printf(INST1, ipw, ip, b0, b1, MNEMONIC, b1);
                break;
            }
        } else if (b0 >= OP_CONST0 && b0 <= OP_GBLV2) {
            switch (b0) {
            case OP_CONST0: disas_tostr(&c->k[0], s); break;
            case OP_CONST1: disas_tostr(&c->k[1], s); break;
            case OP_CONST2: disas_tostr(&c->k[2], s); break;
            case OP_GBLA0:
            case OP_GBLV0:  riff_tostr(&c->k[0], s);     break;
            case OP_GBLA1:
            case OP_GBLV1:  riff_tostr(&c->k[1], s);     break;
            case OP_GBLA2:
            case OP_GBLV2:  riff_tostr(&c->k[2], s);     break;
            default: break;
            }
            printf(INST0DEREF, ipw, ip, b0, MNEMONIC, s);
        } else {
            printf(INST0, ipw, ip, b0, MNEMONIC);
        }
        ip += OP_ARITY + 1;
    }
}

void d_prog(riff_state *e) {
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
        riff_fn *f = e->fn[i];
        printf("\nfn %s @ %p -> %d %s\n",
               f->name->hash ? f->name->str : "<anonymous>",
               f,
               f->code->n,
               f->code->n == 1 ? "byte" : "bytes");
        d_code_obj(e->fn[i]->code, w);
    }
}
