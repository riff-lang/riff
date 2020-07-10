#include <stdio.h>
#include <stdlib.h>

#include "val.h"
#include "vm.h"

int z_exec(code_t *c) {
    hash_t g;
    h_init(&g);
    val_t stack[256];
    register int sp = 0;
    register int ip = 0;
    // printf("%x ", c->code[ip]);
    while (ip < c->n) {
        // printf("%x ", c->code[ip++]);
        switch (c->code[ip]) {
        case OP_JMP8:
            ip += (int8_t) c->code[ip+1];
            break;
        case OP_JMP16:
            ip += (int16_t) ((c->code[ip+1] << 8) + c->code[ip+2]);
            break;
        case OP_JNZ8:
        case OP_JNZ16:
        case OP_JZ8:
        case OP_JZ16:
        case OP_XJZ8:
        case OP_XJZ16:
        case OP_XNJZ8:
        case OP_XNJZ16:
        case OP_TEST:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
        case OP_POW:
        case OP_AND:
        case OP_OR:
        case OP_XOR:
        case OP_SHL:
        case OP_SHR:
        case OP_NUM:
        case OP_NEG:
        case OP_NOT:
        case OP_EQ:
        case OP_NE:
        case OP_GT:
        case OP_GE:
        case OP_LT:
        case OP_LE:
        case OP_LNOT:
        case OP_CALL:
        case OP_CAT:
        case OP_PREINC:
        case OP_PREDEC:
        case OP_POSTINC:
        case OP_POSTDEC:
        case OP_LEN:
        case OP_ADDX:
        case OP_SUBX:
        case OP_MULX:
        case OP_DIVX:
        case OP_MODX:
        case OP_CATX:
        case OP_POWX:
        case OP_ANDX:
        case OP_ORX:
        case OP_SHLX:
        case OP_SHRX:
        case OP_XORX:
        case OP_POP:
        case OP_PUSH0:
        case OP_PUSH1:
        case OP_PUSH2:
        case OP_PUSHI:
        case OP_PUSHK:
        case OP_PUSHK0:
        case OP_PUSHK1:
        case OP_PUSHK2:
        case OP_PUSHS:
        case OP_PUSHS0:
        case OP_PUSHS1:
        case OP_PUSHS2:
        case OP_RET:
        case OP_RET1:
        case OP_GET:
        case OP_SET:
        case OP_PRINT:
        case OP_EXIT:
            exit(0);
        default:
            break;
        }
    }
    return 0;
}
