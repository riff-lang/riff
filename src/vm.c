#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

static void err(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static hash_t globals;

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

static val_t *deref(val_t *v) {
    return IS_SYM(v) ? h_lookup(&globals, v->u.s) : v;
}

// static val_t *cast_num(val_t *v) {
//     switch (v->type) {
//     case TYPE_INT:
//     case TYPE_FLT: return v; // Don't cast
//     case TYPE_STR:
//     }
// }

#define numval(x) (IS_INT(x) ? x->u.i : IS_FLT(x) ? x->u.f : 0)
#define intval(x) (IS_INT(x) ? x->u.i : (int_t) x->u.f)
#define fltval(x) (IS_FLT(x) ? x->u.f : (flt_t) x->u.i)

#define num_arith(l,r,op) \
    return (IS_FLT(l) || IS_FLT(r)) ? \
           v_newflt(numval(l) op numval(r)) : \
           v_newint(numval(l) op numval(r)); 

#define int_arith(l,r,op) return v_newint(intval(l) op intval(r));
#define flt_arith(l,r,op) return v_newflt(numval(l) op numval(r));

val_t *z_add(val_t *l, val_t *r) { num_arith(l,r,+); }
val_t *z_sub(val_t *l, val_t *r) { num_arith(l,r,-); }
val_t *z_mul(val_t *l, val_t *r) { num_arith(l,r,*); }
val_t *z_div(val_t *l, val_t *r) { flt_arith(l,r,/); }

// TODO Make sure this works as intended
// TODO Error handling, e.g. RHS = 0
val_t *z_mod(val_t *l, val_t *r) {
    flt_t res = fmod(numval(l), numval(r));
    return res < 0 ? v_newflt(res + fltval(r)) : v_newflt(res);
}

val_t *z_pow(val_t *l, val_t *r) {
    return v_newflt(pow(fltval(l), fltval(r)));
}

val_t *z_and(val_t *l, val_t *r) { int_arith(l,r,&);  }
val_t *z_or(val_t *l, val_t *r)  { int_arith(l,r,|);  }
val_t *z_xor(val_t *l, val_t *r) { int_arith(l,r,^);  }
val_t *z_shl(val_t *l, val_t *r) { int_arith(l,r,<<); }
val_t *z_shr(val_t *l, val_t *r) { int_arith(l,r,>>); }

// TODO
val_t *z_num(val_t *v) {
    return !IS_NUM(v) ? v_newint(0) : v;
}

val_t *z_neg(val_t *v) {
    return IS_FLT(v) ? v_newflt(-(v->u.f)) : v_newint(-(v->u.i));
}

val_t *z_not(val_t *v) {
    return v_newint(~intval(v));
}

val_t *z_eq(val_t *l, val_t *r) { num_arith(l,r,==); }
val_t *z_ne(val_t *l, val_t *r) { num_arith(l,r,!=); }
val_t *z_gt(val_t *l, val_t *r) { num_arith(l,r,>);  }
val_t *z_ge(val_t *l, val_t *r) { num_arith(l,r,>=); }
val_t *z_lt(val_t *l, val_t *r) { num_arith(l,r,<);  }
val_t *z_le(val_t *l, val_t *r) { num_arith(l,r,<=); }

val_t *z_lnot(val_t *v) {
    return v_newint(!numval(v));
}

// TODO type coercion
val_t *z_len(val_t *v) {
    return IS_STR(v) ? v_newint(v->u.s->l) : v_newint(0);
}

val_t *z_test(val_t *v) {
    return v_newint(test(v));
}

val_t *z_inc(val_t *v) {
    return IS_INT(v) ? v_newint(intval(v) + 1) :
           IS_FLT(v) ? v_newflt(fltval(v) + 1) : v_newint(1);
}

val_t *z_dec(val_t *v) {
    return IS_INT(v) ? v_newint(intval(v) - 1) :
           IS_FLT(v) ? v_newflt(fltval(v) - 1) : v_newint(-1);
}

static void put(val_t *v) {
    switch (v->type) {
    case TYPE_VOID: printf("\n");                break;
    case TYPE_INT:  printf("%lld\n", v->u.i);    break;
    case TYPE_FLT:  printf("%g\n", v->u.f);      break;
    case TYPE_STR:  printf("%s\n", v->u.s->str); break;
    case TYPE_ARR:  // TODO
    case TYPE_FN:   // TODO
    default: break;
    }
}

#define j8       (ip += (int8_t) c->code[ip+1])
#define j16      (ip += (int16_t) ((c->code[ip+1] << 8) + c->code[ip+2]))
#define jc8(x)   (x ? j8  : (ip += 2)); sp--;
#define jc16(x)  (x ? j16 : (ip += 2)); sp--;
#define xjc8(x)  if (x) j8;  else {sp--; ip += 2;}
#define xjc16(x) if (x) j16; else {sp--; ip += 2;}

#define binop(x) stk[sp-2] = z_##x(deref(stk[sp-2]), deref(stk[sp-1]));\
                 sp--; ip++;
#define unop(x)  stk[sp-1] = z_##x(deref(stk[sp-1])); ip++;

#define push(x)  (stk[sp++] = x)

int z_exec(code_t *c) {
    h_init(&globals);
    val_t *stk[256];
    register int sp = 0;
    register int ip = 0;
    while (ip < c->n) {
        switch (c->code[ip]) {
        case OP_JMP8:   j8; break;
        case OP_JMP16:  j16; break;
        case OP_JNZ8:   jc8(test(deref(stk[sp-1]))); break;
        case OP_JNZ16:  jc16(test(deref(stk[sp-1]))); break;
        case OP_JZ8:    jc8(!test(deref(stk[sp-1]))); break;
        case OP_JZ16:   jc16(!test(deref(stk[sp-1]))); break;
        case OP_XJNZ8:  xjc8(test(deref(stk[sp-1]))); break;
        case OP_XJNZ16: xjc16(test(deref(stk[sp-1]))); break;
        case OP_XJZ8:   xjc8(!test(deref(stk[sp-1]))); break;
        case OP_XJZ16:  xjc16(!test(deref(stk[sp-1]))); break;
        case OP_TEST:   unop(test); break;
        case OP_ADD:    binop(add); break;
        case OP_SUB:    binop(sub); break;
        case OP_MUL:    binop(mul); break;
        case OP_DIV:    binop(div); break;
        case OP_MOD:    binop(mod); break;
        case OP_POW:    binop(pow); break;
        case OP_AND:    binop(and); break;
        case OP_OR:     binop(or);  break;
        case OP_XOR:    binop(xor); break;
        case OP_SHL:    binop(shl); break;
        case OP_SHR:    binop(shr); break;
        case OP_NUM:    unop(num);  break;
        case OP_NEG:    unop(neg);  break;
        case OP_NOT:    unop(not);  break;
        case OP_EQ:     binop(eq);  break;
        case OP_NE:     binop(ne);  break;
        case OP_GT:     binop(gt);  break;
        case OP_GE:     binop(ge);  break;
        case OP_LT:     binop(lt);  break;
        case OP_LE:     binop(le);  break;
        case OP_LNOT:   unop(lnot); break;
        case OP_CALL:   break;
        case OP_CAT:    break;
        // TODO Make sure pre inc/dec works properly with unintialized
        // variables. They currently work but they shouldn't since
        // deref should ultimately be returning v_newvoid(), which
        // only sets the str_t union member to NULL
        case OP_PREINC: {
            str_t *k = stk[sp-1]->u.s;
            unop(inc);
            h_insert(&globals, k, stk[sp-1]);
            break;
        }
        case OP_PREDEC: {
            str_t *k = stk[sp-1]->u.s;
            unop(dec);
            h_insert(&globals, k, stk[sp-1]);
            break;
        }
        case OP_POSTINC: {
            str_t *k = stk[sp-1]->u.s;
            unop(num);
            h_insert(&globals, k, z_inc(stk[sp-1]));
            break;
        }
        case OP_POSTDEC: {
            str_t *k = stk[sp-1]->u.s;
            unop(num);
            h_insert(&globals, k, z_dec(stk[sp-1]));
            break;
        }
        case OP_LEN:    unop(len); break;
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
            sp--;
            break;
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
            if (stk[sp-2]->type != TYPE_SYM)
                err("Attempt to assign to constant value");
            h_insert(&globals, stk[sp-2]->u.s, deref(stk[sp-1]));
            stk[sp-2] = deref(stk[sp-1]);
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
