#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

val_t *z_add(val_t *l, val_t *r) {
    l->u.i += r->u.i;
    return l;
}

val_t *z_sub(val_t *l, val_t *r) {
    l->u.i -= r->u.i;
    return l;
}

// Return logical result of value
static int ltest(val_t *v) {
    switch (v->type) {
    case TYPE_INT: return !!(v->u.i);
    case TYPE_FLT: return !!(v->u.f);
    case TYPE_STR: return 1; // Temp
    default: return 0; // Temp
    }
    return 0;
}

#define j8(x)      (x += (int8_t) c->code[ip+1])
#define j16(x)     (x += (int16_t) ((c->code[ip+1] << 8) + c->code[ip+2]))
#define jc8(x,y)   (y ? j8(x)  : (x += 2))
#define jc16(x,y)  (y ? j16(x) : (x += 2))

int z_exec(code_t *c) {
    hash_t g;
    h_init(&g);
    val_t *stk[256];
    register int sp = 0;
    register int ip = 0;
    while (ip < c->n) {
        switch (c->code[ip]) {
        case OP_JMP8:  j8(ip);  break;
        case OP_JMP16: j16(ip); break;
        case OP_JNZ8:  jc8(ip,   ltest(stk[sp-1])); sp--; break;
        case OP_JNZ16: jc16(ip,  ltest(stk[sp-1])); sp--; break;
        case OP_JZ8:   jc8(ip,  !ltest(stk[sp-1])); sp--; break;
        case OP_JZ16:  jc16(ip, !ltest(stk[sp-1])); sp--; break;
        case OP_XJNZ8:
        case OP_XJNZ16:
        case OP_XJZ8:
        case OP_XJZ16:
        case OP_TEST:
        case OP_ADD:
            stk[sp-2] = z_add(stk[sp-2], stk[sp-1]);
            sp -= 1;
            ip += 1;
            break;
        case OP_SUB:
            stk[sp-2] = z_sub(stk[sp-2], stk[sp-1]);
            sp -= 1;
            ip += 1;
            break;
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
            stk[sp++] = v_newint(0);
            ip += 1;
            break;
        case OP_PUSH1:
            stk[sp++] = v_newint(1);
            ip += 1;
            break;
        case OP_PUSH2:
            stk[sp++] = v_newint(2);
            ip += 1;
            break;
        case OP_PUSHI:
            stk[sp++] = v_newint(c->code[ip+1]);
            ip += 2;
            break;
        case OP_PUSHK:
        case OP_PUSHK0:
        case OP_PUSHK1:
        case OP_PUSHK2:
        case OP_PUSHS:
        case OP_PUSHS0:
        case OP_PUSHS1:
        case OP_PUSHS2:
        case OP_RET:
            exit(0);
        case OP_RET1:
        case OP_GET:
        case OP_SET:
            ip++;
            break;
        case OP_PRINT:
            printf("%lld\n", stk[--sp]->u.i);
            ip += 1;
            break;
        case OP_EXIT:
            exit(0);
        default:
            break;
        }
    }
    return 0;
}
