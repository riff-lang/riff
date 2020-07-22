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

#define numval(x) (IS_INT(x) ? x->u.i : \
                   IS_FLT(x) ? x->u.f : 0)
#define intval(x) (IS_INT(x) ? x->u.i : \
                   IS_FLT(x) ? (int_t) x->u.f : 0)
#define fltval(x) (IS_FLT(x) ? x->u.f : \
                   IS_INT(x) ? (flt_t) x->u.i : 0)

#define int_arith(l,r,op) \
    assign_int(l, (intval(l) op intval(r)));

#define flt_arith(l,r,op) \
    assign_flt(l, (numval(l) op numval(r)));

#define num_arith(l,r,op) \
    if (IS_FLT(l) || IS_FLT(r)) { \
        flt_arith(l,r,op); \
    } else { \
        int_arith(l,r,op); \
    }

void z_add(val_t *l, val_t *r) { num_arith(l,r,+); }
void z_sub(val_t *l, val_t *r) { num_arith(l,r,-); }
void z_mul(val_t *l, val_t *r) { num_arith(l,r,*); }
void z_div(val_t *l, val_t *r) { flt_arith(l,r,/); }

// TODO Make sure this works as intended
// TODO Error handling, e.g. RHS = 0
void z_mod(val_t *l, val_t *r) {
    flt_t res = fmod(numval(l), numval(r));
    assign_flt(l, res < 0 ? res + numval(r) : res);
}

void z_pow(val_t *l, val_t *r) {
    assign_flt(l, pow(fltval(l), fltval(r)));
}

void z_and(val_t *l, val_t *r) { int_arith(l,r,&);  }
void z_or(val_t *l, val_t *r)  { int_arith(l,r,|);  }
void z_xor(val_t *l, val_t *r) { int_arith(l,r,^);  }
void z_shl(val_t *l, val_t *r) { int_arith(l,r,<<); }
void z_shr(val_t *l, val_t *r) { int_arith(l,r,>>); }

// TODO
void z_num(val_t *v) {
    switch (v->type) {
    case TYPE_INT:
        assign_int(v, intval(v));
        break;
    case TYPE_FLT:
        assign_flt(v, fltval(v));
        break;
    default:
        assign_int(v, 0);
        break;
    }
}

void z_neg(val_t *v) {
    switch (v->type) {
    case TYPE_INT:
        assign_int(v, -intval(v));
        break;
    case TYPE_FLT:
        assign_flt(v, -fltval(v));
        break;
    default:
        assign_int(v, -1); // TODO type coercion
        break;
    }
}

void z_not(val_t *v) {
    assign_int(v, ~intval(v));
}

void z_eq(val_t *l, val_t *r) { num_arith(l,r,==); }
void z_ne(val_t *l, val_t *r) { num_arith(l,r,!=); }
void z_gt(val_t *l, val_t *r) { num_arith(l,r,>);  }
void z_ge(val_t *l, val_t *r) { num_arith(l,r,>=); }
void z_lt(val_t *l, val_t *r) { num_arith(l,r,<);  }
void z_le(val_t *l, val_t *r) { num_arith(l,r,<=); }

void z_lnot(val_t *v) {
    assign_int(v, !numval(v));
}

void z_len(val_t *v) {
    int_t l = 0;
    switch (v->type) {
    case TYPE_INT: l = s_int2str(v->u.i)->l; break;
    case TYPE_FLT: l = s_flt2str(v->u.f)->l; break;
    case TYPE_STR: l = v->u.s->l;            break;
    case TYPE_ARR: // TODO
    case TYPE_FN:  // TODO
    default: break;
    }
    assign_int(v, l);
}

void z_test(val_t *v) {
    assign_int(v, test(v));
}

void z_cat(val_t *l, val_t *r) {
    switch (l->type) {
    case TYPE_VOID: l->u.s = s_newstr(NULL, 0, 0); break;
    case TYPE_INT:  l->u.s = s_int2str(intval(l)); break;
    case TYPE_FLT:  l->u.s = s_flt2str(fltval(l)); break;
    default: break;
    }

    switch (r->type) {
    case TYPE_VOID: r->u.s = s_newstr(NULL, 0, 0); break;
    case TYPE_INT:  r->u.s = s_int2str(intval(r)); break;
    case TYPE_FLT:  r->u.s = s_flt2str(fltval(r)); break;
    default: break;
    }

    assign_str(l, s_concat(l->u.s, r->u.s, 0));
}

// Potentially very slow; allocates 2 new string objects for every int
// or float LHS
// TODO Index out of bounds handling, e.g. RHS > length of LHS
void z_idx(val_t *l, val_t *r) {
    switch (l->type) {
    case TYPE_INT:
        assign_str(l, s_newstr(&s_int2str(l->u.i)->str[intval(r)], 1, 0));
        break;
    case TYPE_FLT:
        assign_str(l, s_newstr(&s_flt2str(l->u.f)->str[intval(r)], 1, 0));
        break;
    case TYPE_STR:
        assign_str(l, s_newstr(&l->u.s->str[intval(r)], 1, 0));
        break;
    case TYPE_ARR: // TODO
    default:
        assign_int(l, 0);
        break;
    }
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
#define jc8(x)  (x ? j8  : (ip += 2)); --sp;
#define jc16(x) (x ? j16 : (ip += 3)); --sp;

// Conditional jumps (pop stack if jump not taken)
#define xjc8(x)  if (x) j8;  else {--sp; ip += 2;}
#define xjc16(x) if (x) j16; else {--sp; ip += 3;}

// Standard binary operations
#define binop(x) \
    z_##x(sp[-2], sp[-1]); \
    --sp; \
    ++ip;

// Unary operations
#define unop(x) \
    z_##x(sp[-1]); \
    ++ip;

// Pre-increment/decrement
#define pre(x) { \
    switch (sp[-1]->type) { \
    case TYPE_INT: sp[-1]->u.i += x; break; \
    case TYPE_FLT: sp[-1]->u.f += x; break; \
    default: \
        assign_int(sp[-1], x); \
        break; \
    } \
    tv.type = sp[-1]->type; \
    tv.u    = sp[-1]->u; \
    sp[-1]  = &tv; \
    ++ip; \
}

// Post-increment/decrement
#define post(x) { \
    tv.type = sp[-1]->type; \
    tv.u    = sp[-1]->u; \
    switch (sp[-1]->type) { \
    case TYPE_INT: sp[-1]->u.i += x; break; \
    case TYPE_FLT: sp[-1]->u.f += x; break; \
    default: \
        assign_int(sp[-1], x); \
        break; \
    } \
    sp[-1] = &tv; \
    unop(num); \
}

// Compound assignment operations
#define cbinop(x) { \
    tp      = sp[-2]; \
    tv.type = sp[-2]->type; \
    tv.u    = sp[-2]->u; \
    sp[-2]  = &tv; \
    binop(x); \
    tp->type = sp[-1]->type; \
    tp->u    = sp[-1]->u; \
}

#define push(x) \
    sp[0]->type = x->type; \
    sp[0]->u    = x->u; \
    ++sp;

#define pushi(x) \
    assign_int(sp[0], x); \
    ++sp;

#define pusha(x) \
    sp[0] = h_lookup(&globals, c->k.v[(x)]->u.s); \
    ++sp;

#define pushv(x) \
    tp = h_lookup(&globals, c->k.v[(x)]->u.s); \
    sp[0]->type = tp->type; \
    sp[0]->u    = tp->u; \
    ++sp;

int z_exec(code_t *c) {
    h_init(&globals);
    val_t **sp = malloc(256 * sizeof(val_t *));
    for (int i = 0; i < 256; i++)
        sp[i] = malloc(sizeof(val_t));
    val_t  tv;                          // Temp value
    val_t *tp = malloc(sizeof(val_t));  // Temp pointer
    uint8_t *ip = c->code;
    while (1) {
        switch (*ip) {
        case OP_JMP8:    j8;                   break;
        case OP_JMP16:   j16;                  break;
        case OP_JNZ8:    jc8(test(sp[-1]));    break;
        case OP_JNZ16:   jc16(test(sp[-1]));   break;
        case OP_JZ8:     jc8(!test(sp[-1]));   break;
        case OP_JZ16:    jc16(!test(sp[-1]));  break;
        case OP_XJNZ8:   xjc8(test(sp[-1]));   break;
        case OP_XJNZ16:  xjc16(test(sp[-1]));  break;
        case OP_XJZ8:    xjc8(!test(sp[-1]));  break;
        case OP_XJZ16:   xjc16(!test(sp[-1])); break;
        case OP_TEST:    unop(test);           break;
        case OP_ADD:     binop(add);           break;
        case OP_SUB:     binop(sub);           break;
        case OP_MUL:     binop(mul);           break;
        case OP_DIV:     binop(div);           break;
        case OP_MOD:     binop(mod);           break;
        case OP_POW:     binop(pow);           break;
        case OP_AND:     binop(and);           break;
        case OP_OR:      binop(or);            break;
        case OP_XOR:     binop(xor);           break;
        case OP_SHL:     binop(shl);           break;
        case OP_SHR:     binop(shr);           break;
        case OP_NUM:     unop(num);            break;
        case OP_NEG:     unop(neg);            break;
        case OP_NOT:     unop(not);            break;
        case OP_EQ:      binop(eq);            break;
        case OP_NE:      binop(ne);            break;
        case OP_GT:      binop(gt);            break;
        case OP_GE:      binop(ge);            break;
        case OP_LT:      binop(lt);            break;
        case OP_LE:      binop(le);            break;
        case OP_LNOT:    unop(lnot);           break;
        case OP_CALL: // TODO
            break;
        case OP_CAT:     binop(cat);           break;
        case OP_PREINC:  pre(1);               break;
        case OP_PREDEC:  pre(-1);              break;
        case OP_POSTINC: post(1);              break;
        case OP_POSTDEC: post(-1);             break;
        case OP_LEN:     unop(len);            break;
        case OP_ADDX:    cbinop(add);          break;
        case OP_SUBX:    cbinop(sub);          break;
        case OP_MULX:    cbinop(mul);          break;
        case OP_DIVX:    cbinop(div);          break;
        case OP_MODX:    cbinop(mod);          break;
        case OP_CATX:    cbinop(cat);          break;
        case OP_POWX:    cbinop(pow);          break;
        case OP_ANDX:    cbinop(and);          break;
        case OP_ORX:     cbinop(or);           break;
        case OP_SHLX:    cbinop(shl);          break;
        case OP_SHRX:    cbinop(shr);          break;
        case OP_XORX:    cbinop(xor);          break;
        case OP_POP:
            --sp;
            ++ip;
            break;
        case OP_PUSH0:
            pushi(0);
            ++ip;
            break;
        case OP_PUSH1:
            pushi(1);
            ++ip;
            break;
        case OP_PUSH2:
            pushi(2);
            ++ip;
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
            ++ip;
            break;
        case OP_PUSHK1:
            push(c->k.v[1]);
            ++ip;
            break;
        case OP_PUSHK2:
            push(c->k.v[2]);
            ++ip;
            break;
        case OP_PUSHA:
            pusha(ip[1]);
            ip += 2;
            break;
        case OP_PUSHA0:
            pusha(0);
            ++ip;
            break;
        case OP_PUSHA1:
            pusha(1);
            ++ip;
            break;
        case OP_PUSHA2:
            pusha(2);
            ++ip;
            break;
        case OP_PUSHV:
            pushv(ip[1]);
            ip += 2;
            break;
        case OP_PUSHV0:
            pushv(0);
            ++ip;
            break;
        case OP_PUSHV1:
            pushv(1);
            ++ip;
            break;
        case OP_PUSHV2:
            pushv(2);
            ++ip;
            break;
        case OP_RET:
            exit(0);
        case OP_RET1: // TODO
            break;
        case OP_IDX: // TODO
            binop(idx);
            break;
        case OP_SET:
            tp       = sp[-2];
            tv.type  = sp[-1]->type;
            tv.u     = sp[-1]->u;
            sp[-2]   = &tv;
            tp->type = tv.type;
            tp->u    = tv.u;
            --sp;
            ++ip;
            break;
        case OP_PRINT:
            put(sp[-1]);
            --sp;
            ++ip;
            break;
        case OP_EXIT:
            exit(0);
        default:
            break;
        }
    }
    return 0;
}

#undef j8
#undef j16
#undef jc8
#undef jc16
#undef xjc8
#undef xjc16
#undef binop
#undef unop
#undef pre
#undef post
#undef cbinop
#undef push
#undef pusha
#undef pushi
#undef pushv
