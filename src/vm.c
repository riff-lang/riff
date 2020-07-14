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

#define numval(x) (IS_INT(x) ? x->u.i : IS_FLT(x) ? x->u.f : 0)
#define intval(x) (IS_INT(x) ? x->u.i : IS_FLT(x) ? (int_t) x->u.f : 0)
#define fltval(x) (IS_FLT(x) ? x->u.f : IS_INT(x) ? (flt_t) x->u.i : 0)

#define num_arith(l,r,op) \
    return (IS_FLT(l) || IS_FLT(r)) ? \
           v_newflt(numval(l) op numval(r)) : \
           v_newint(intval(l) op intval(r)); 

#define int_arith(l,r,op) return v_newint(intval(l) op intval(r));
#define flt_arith(l,r,op) return v_newflt(numval(l) op numval(r));

// #define int_arith(l,r,op) \
//     l->u.i  = intval(l) op intval(r); \
//     l->type = TYPE_INT; \
//     return l;

// #define flt_arith(l,r,op) \
//     l->u.f  = numval(l) op numval(r); \
//     l->type = TYPE_FLT; \
//     return l;

// #define num_arith(l,r,op) \
//     if (IS_FLT(l) || IS_FLT(r)) { \
//         flt_arith(l,r,op); \
//     } else  { \
//         int_arith(l,r,op); \
//     }

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

// Unconditional jumps
#define j8  (ip += (int8_t) ip[1])
#define j16 (ip += (int16_t) ((ip[1] << 8) + ip[2]))

// Conditional jumps (pop stack unconditionally)
#define jc8(x)  (x ? j8  : (ip += 2)); sp--;
#define jc16(x) (x ? j16 : (ip += 3)); sp--;

// Conditional jumps (pop stack if jump not taken)
#define xjc8(x)  if (x) j8;  else {sp--; ip += 2;}
#define xjc16(x) if (x) j16; else {sp--; ip += 3;}

// Standard binary operations
#define binop(x) \
    sp[-2] = *z_##x(deref(sp-2), deref(sp-1));\
    sp--; \
    ip++;

// Unary operations
#define unop(x) \
    sp[-1] = *z_##x(deref(sp-1)); \
    ip++;

// Pre-increment/decrement
#define pre(x) { \
    str_t *k = sp[-1].u.s; \
    unop(x); \
    h_insert(&globals, k, &sp[-1]); \
}

// Post-increment/decrement
#define post(x) { \
    str_t *k = sp[-1].u.s; \
    unop(num); \
    h_insert(&globals, k, z_##x(sp-1)); \
}

// Compound assignment operations
#define cbinop(x) { \
    str_t *k = sp[-2].u.s; \
    binop(x); \
    h_insert(&globals, k, deref(sp-1)); \
}

#define push(x) \
    sp->type = x->type; \
    sp->u    = x->u; \
    sp++;

#define pushi(x) \
    sp->type = TYPE_INT; \
    sp->u.i  = x; \
    sp++;

int z_exec(code_t *c) {
    h_init(&globals);
    register val_t *sp = malloc(256 * sizeof(val_t));
    register uint8_t *ip = c->code;
    while (1) {
        switch (*ip) {
        case OP_JMP8:    j8; break;
        case OP_JMP16:   j16; break;
        case OP_JNZ8:    jc8(test(deref((sp-1)))); break;
        case OP_JNZ16:   jc16(test(deref((sp-1)))); break;
        case OP_JZ8:     jc8(!test(deref((sp-1)))); break;
        case OP_JZ16:    jc16(!test(deref((sp-1)))); break;
        case OP_XJNZ8:   xjc8(test(deref((sp-1)))); break;
        case OP_XJNZ16:  xjc16(test(deref((sp-1)))); break;
        case OP_XJZ8:    xjc8(!test(deref((sp-1)))); break;
        case OP_XJZ16:   xjc16(!test(deref((sp-1)))); break;
        case OP_TEST:    unop(test); break;
        case OP_ADD:     binop(add); break;
        case OP_SUB:     binop(sub); break;
        case OP_MUL:     binop(mul); break;
        case OP_DIV:     binop(div); break;
        case OP_MOD:     binop(mod); break;
        case OP_POW:     binop(pow); break;
        case OP_AND:     binop(and); break;
        case OP_OR:      binop(or);  break;
        case OP_XOR:     binop(xor); break;
        case OP_SHL:     binop(shl); break;
        case OP_SHR:     binop(shr); break;
        case OP_NUM:     unop(num);  break;
        case OP_NEG:     unop(neg);  break;
        case OP_NOT:     unop(not);  break;
        case OP_EQ:      binop(eq);  break;
        case OP_NE:      binop(ne);  break;
        case OP_GT:      binop(gt);  break;
        case OP_GE:      binop(ge);  break;
        case OP_LT:      binop(lt);  break;
        case OP_LE:      binop(le);  break;
        case OP_LNOT:    unop(lnot); break;
        case OP_CALL: // TODO
            break;
        case OP_CAT:     // TODO binop(cat);
            break;
        case OP_PREINC:  pre(inc); break;
        case OP_PREDEC:  pre(dec); break;
        case OP_POSTINC: post(inc); break;
        case OP_POSTDEC: post(dec); break;
        case OP_LEN:     unop(len); break;
        case OP_ADDX:    cbinop(add); break;
        case OP_SUBX:    cbinop(sub); break;
        case OP_MULX:    cbinop(mul); break;
        case OP_DIVX:    cbinop(div); break;
        case OP_MODX:    cbinop(mod); break;
        case OP_CATX:    // TODO cbinop(cat);
            break;
        case OP_POWX:    cbinop(pow); break;
        case OP_ANDX:    cbinop(and); break;
        case OP_ORX:     cbinop(or);  break;
        case OP_SHLX:    cbinop(shl); break;
        case OP_SHRX:    cbinop(shr); break;
        case OP_XORX:    cbinop(xor); break;
        case OP_POP:
            sp--;
            ip++;
            break;
        case OP_PUSH0:
            pushi(0);
            ip++;
            break;
        case OP_PUSH1:
            pushi(1);
            ip++;
            break;
        case OP_PUSH2:
            pushi(2);
            ip++;
            break;
        case OP_PUSHI:
            pushi(ip[1]);
            ip += 2;
            break;
        case OP_PUSHK:
            push(c->k.v[ip[1]]);
            ip += 2;
            break;
        case OP_PUSHK0:
            push(c->k.v[0]);
            ip++;
            break;
        case OP_PUSHK1:
            push(c->k.v[1]);
            ip++;
            break;
        case OP_PUSHK2:
            push(c->k.v[2]);
            ip++;
            break;
        case OP_PUSHS:
            push(c->k.v[ip[1]]);
            sp[-1].type = TYPE_SYM;
            ip += 2;
            break;
        case OP_PUSHS0:
            push(c->k.v[0]);
            sp[-1].type = TYPE_SYM;
            ip++;
            break;
        case OP_PUSHS1:
            push(c->k.v[1]);
            sp[-1].type = TYPE_SYM;
            ip++;
            break;
        case OP_PUSHS2:
            push(c->k.v[2]);
            sp[-1].type = TYPE_SYM;
            ip++;
            break;
        case OP_RET:
            exit(0);
        case OP_RET1: // TODO
            break;
        case OP_GET: // TODO
            break;
        case OP_SET:
            if (!IS_SYM((sp-2)))
                err("Attempt to assign to constant value");
            h_insert(&globals, sp[-2].u.s, deref(sp-1));
            sp[-2] = *deref(sp-1);
            sp--;
            ip++;
            break;
        case OP_PRINT:
            put(deref(--sp));
            ip++;
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
