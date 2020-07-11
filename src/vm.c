#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

static hash_t globals;

static val_t *deref(val_t *v) {
    return IS_SYM(v) ? h_lookup(&globals, v->u.s) : v;
}

val_t *z_add(val_t *l, val_t *r) {
    return v_newint(l->u.i + r->u.i);
}

val_t *z_sub(val_t *l, val_t *r) {
    return v_newint(l->u.i - r->u.i);
}

// Return logical result of value
static int test(val_t *v) {
    switch (v->type) {
    case TYPE_INT: return !!(v->u.i);
    case TYPE_FLT: return !!(v->u.f);
    case TYPE_STR: return !!(v->u.s->l);
    case TYPE_ARR: return 0; // TODO
    case TYPE_FN:  return 1;
    default:       return 0;
    }
}

static void put(val_t *v) {
    switch (v->type) {
    case TYPE_INT: printf("%lld\n", v->u.i);    break;
    case TYPE_FLT: printf("%g\n", v->u.f);      break;
    case TYPE_STR: printf("%s\n", v->u.s->str); break;
    case TYPE_ARR: // TODO
    case TYPE_FN:  // TODO
    default: break;
    }
}

#define j8(x)      (x += (int8_t) c->code[ip+1])
#define j16(x)     (x += (int16_t) ((c->code[ip+1] << 8) + c->code[ip+2]))
#define jc8(x,y)   (y ? j8(x)  : (x += 2))
#define jc16(x,y)  (y ? j16(x) : (x += 2))
#define xjc8(x,y)  if (y) j8(x);  else {sp--; x += 2;}
#define xjc16(x,y) if (y) j16(x); else {sp--; x += 2;}

#define push(x)    (stk[sp++] = x)

int z_exec(code_t *c) {
    h_init(&globals);
    val_t *stk[256];
    register int sp = 0;
    register int ip = 0;
    while (ip < c->n) {
        switch (c->code[ip]) {
        case OP_JMP8:   j8(ip);  break;
        case OP_JMP16:  j16(ip); break;
        case OP_JNZ8:   jc8(ip,    test(deref(stk[sp-1]))); sp--; break;
        case OP_JNZ16:  jc16(ip,   test(deref(stk[sp-1]))); sp--; break;
        case OP_JZ8:    jc8(ip,   !test(deref(stk[sp-1]))); sp--; break;
        case OP_JZ16:   jc16(ip,  !test(deref(stk[sp-1]))); sp--; break;
        case OP_XJNZ8:  xjc8(ip,   test(deref(stk[sp-1]))); break;
        case OP_XJNZ16: xjc16(ip,  test(deref(stk[sp-1]))); break;
        case OP_XJZ8:   xjc8(ip,  !test(deref(stk[sp-1]))); break;
        case OP_XJZ16:  xjc16(ip, !test(deref(stk[sp-1]))); break;
        case OP_TEST:
        case OP_ADD:
            stk[sp-2] = z_add(deref(stk[sp-2]), deref(stk[sp-1]));
            sp -= 1;
            ip += 1;
            break;
        case OP_SUB:
            stk[sp-2] = z_sub(deref(stk[sp-2]), deref(stk[sp-1]));
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
            push(v_newint(0));
            ip += 1;
            break;
        case OP_PUSH1:
            push(v_newint(1));
            ip += 1;
            break;
        case OP_PUSH2:
            push(v_newint(2));
            ip += 1;
            break;
        case OP_PUSHI:
            push(v_newint(c->code[ip+1]));
            ip += 2;
            break;
        case OP_PUSHK:
            push(c->k.v[c->code[ip+1]]);
            ip += 2;
            break;
        case OP_PUSHK0:
            push(c->k.v[0]);
            ip += 1;
            break;
        case OP_PUSHK1:
            push(c->k.v[1]);
            ip += 1;
            break;
        case OP_PUSHK2:
            push(c->k.v[2]);
            ip += 1;
            break;
        case OP_PUSHS:
            push(c->k.v[c->code[ip+1]]);
            stk[sp-1]->type = TYPE_SYM;
            ip += 2;
            break;
        case OP_PUSHS0:
            push(c->k.v[0]);
            stk[sp-1]->type = TYPE_SYM;
            ip += 1;
            break;
        case OP_PUSHS1:
            push(c->k.v[1]);
            stk[sp-1]->type = TYPE_SYM;
            ip += 1;
            break;
        case OP_PUSHS2:
            push(c->k.v[2]);
            stk[sp-1]->type = TYPE_SYM;
            ip += 1;
            break;
        case OP_RET:
            exit(0);
        case OP_RET1:
        case OP_GET:
        case OP_SET:
            h_insert(&globals, stk[sp-2]->u.s, deref(stk[sp-1]));
            sp -= 1;
            ip += 1;
            break;
        case OP_PRINT:
            put(deref(stk[--sp]));
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

#undef push
