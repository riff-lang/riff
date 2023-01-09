#include "disas.h"

#include "conf.h"
#include "string.h"
#include "util.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

static const struct {
    const char *mnemonic;
    int         arity;
} opcode_info[] = {
#define OPCODE(x,y,z) { z, y }
#include "opcodes.h"
};

#define ARITY(op)       (opcode_info[(op)].arity)
#define MNEMONIC(op)    (opcode_info[(op)].mnemonic)

// Convenience macros for format specifiers
#define F_XX        "%02x "
#define F_MNEMONIC  " %s"
#define F_OPERAND   "%d"
#define F_LMNEMONIC " %-7s "
#define F_LOPERAND  "%-6d "
#define F_DEREF     " // %s"

// Common format strings
#define INST0       "      "   F_MNEMONIC                     "\n"
#define INST0DEREF  "      "   F_LMNEMONIC "     "    F_DEREF "\n"
#define INST1       F_XX "   " F_LMNEMONIC F_OPERAND          "\n"
#define INST1DEREF  F_XX "   " F_LMNEMONIC F_LOPERAND F_DEREF "\n"
#define INST2       F_XX F_XX  F_LMNEMONIC F_OPERAND          "\n"

// Wrap string in quotes
// TODO deconstruct bytes that correspond to escape sequences into their literal
//      forms
static size_t disas_tostr(riff_val *v, char **out) {
    if (is_str(v))
        return (size_t) sprintf(*out, "'%s'", v->s->str);
    return riff_tostr(v, out);
}

static void disas_code(riff_code *c, const int ip_width) {
    uint8_t *b;
    char buf[STR_BUF_SZ], *sptr;
    int ip = 0;

    while (ip < c->n) {
        sptr = buf;
        b = &c->code[ip];
        fprintf(stdout, "%*d:  " F_XX, ip_width, ip, b[0]);
        if (ARITY(b[0]) > 0) {
            switch (b[0]) {
            case OP_CONST:
                disas_tostr(&c->k[b[1]], &sptr);
                printf(INST1DEREF, b[1], MNEMONIC(b[0]), b[1], sptr);
                break;
            case OP_TABK:
            case OP_GBLA:
            case OP_GBLV:
            case OP_SIDXA:
            case OP_SIDXV:
                riff_tostr(&c->k[b[1]], &sptr);
                printf(INST1DEREF, b[1], MNEMONIC(b[0]), b[1], sptr);
                break;
            case OP_JMP8:
            case OP_JZ8:
            case OP_JNZ8:
            case OP_XJZ8:
            case OP_XJNZ8:
                printf(INST1, b[1], MNEMONIC(b[0]), ip + (int8_t) b[1]);
                break;
            case OP_JMP16:
            case OP_JZ16:
            case OP_JNZ16:
            case OP_XJZ16:
            case OP_XJNZ16:
            case OP_ITERV:
            case OP_ITERKV:
                printf(INST2, b[1], b[2], MNEMONIC(b[0]), ip + *(int16_t *) &b[1]);
                break;
            case OP_LOOP8:
                printf(INST1, b[1], MNEMONIC(b[0]), ip - b[1]);
                break;
            case OP_LOOP16:
                printf(INST2, b[1], b[2], MNEMONIC(b[0]), ip - *(uint16_t *) &b[1]);
                break;
            case OP_IMM16:
                printf(INST2, b[1], b[2], MNEMONIC(b[0]), *(uint16_t *) &b[1]);
                break;
            default:
                printf(INST1, b[1], MNEMONIC(b[0]), b[1]);
                break;
            }
        } else if (b[0] >= OP_CONST0 && b[0] <= OP_GBLV2) {
            switch (b[0]) {
            case OP_CONST0:
            case OP_CONST1:
            case OP_CONST2:
                disas_tostr(&c->k[b[0] - OP_CONST0], &sptr);
                break;
            case OP_GBLA0:
            case OP_GBLA1:
            case OP_GBLA2:
                riff_tostr(&c->k[b[0] - OP_GBLA0], &sptr);
                break;
            case OP_GBLV0:
            case OP_GBLV1:
            case OP_GBLV2:
                riff_tostr(&c->k[b[0] - OP_GBLV0], &sptr);
                break;
            }
            printf(INST0DEREF, MNEMONIC(b[0]), sptr);
        } else {
            printf(INST0, MNEMONIC(b[0]));
        }
        ip += ARITY(b[0]) + 1;
    }
}

static int max_ip_width(riff_state *state) {
    // Calculate width for the IP in the disassembly
    int w = (int) log10(state->main.code.n - 1) + 1;
    RIFF_VEC_FOREACH(&state->global_fn, i) {
        int fw = (int) log10(RIFF_VEC_GET(&state->global_fn, i)->code.n - 1) + 1;
        w = w < fw ? fw : w;
    }
    RIFF_VEC_FOREACH(&state->anon_fn, i) {
        int fw = (int) log10(RIFF_VEC_GET(&state->anon_fn, i)->code.n - 1) + 1;
        w = w < fw ? fw : w;
    }
    return w;
}

static void print_fn_header(const char *prefix, riff_fn *fn) {
    fprintf(stdout, "%sfn %s @ %p -> %d %s\n",
            prefix,
            fn->name && riff_strlen(fn->name) ? fn->name->str : "<anonymous>",
            fn,
            fn->code.n,
            fn->code.n == 1 ? "byte" : "bytes");
}

int riff_disas(riff_state *state) {
    int ip_width = max_ip_width(state);
    fprintf(stdout, "source:%s @ %p -> %d %s\n",
            state->name,
            &state->main,
            state->main.code.n,
            state->main.code.n == 1 ? "byte" : "bytes");
    disas_code(&state->main.code, ip_width);
    RIFF_VEC_FOREACH(&state->global_fn, i) {
        riff_fn *fn = RIFF_VEC_GET(&state->global_fn, i);
        puts("");
        print_fn_header("", fn);
        disas_code(&fn->code, ip_width);
    }
    RIFF_VEC_FOREACH(&state->anon_fn, i) {
        riff_fn *fn = RIFF_VEC_GET(&state->anon_fn, i);
        puts("");
        print_fn_header("local ", fn);
        disas_code(&fn->code, ip_width);
    }
    return 0;
}
