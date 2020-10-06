#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "lib.h"
#include "mem.h"
#include "vm.h"

static void err(const char *msg) {
    fprintf(stderr, "riff: [vm] %s\n", msg);
    exit(1);
}

static hash_t    globals;
static rf_arr    argv;
static rf_int    aos;
static rf_iter  *iter;
static rf_stack  stack[STACK_SIZE];

inline rf_int str2int(rf_str *s) {
    char *end;
    return (rf_int) strtoull(s->str, &end, 0);
}

inline rf_flt str2flt(rf_str *s) {
    char *end;
    return strtod(s->str, &end);
}

// Return logical result of value
static inline int test(rf_val *v) {
    switch (v->type) {
    case TYPE_INT: return !!(v->u.i);
    case TYPE_FLT: return !!(v->u.f);

    // If entire string is a numeric value, return logical result of
    // the number. Otherwise, return whether the string is longer than
    // 0.
    case TYPE_STR: {
        char *end;
        rf_flt f = strtod(v->u.s->str, &end);
        if (*end == '\0')
            return !!f;
        return !!v->u.s->l;
    }
    case TYPE_ARR: return !!a_length(v->u.a);
    case TYPE_SEQ:
    case TYPE_RFN: case TYPE_CFN:
        return 1;
    default: return 0;
    }
}


static inline void z_add(rf_val *l, rf_val *r) { num_arith(l,r,+); }
static inline void z_sub(rf_val *l, rf_val *r) { num_arith(l,r,-); }
static inline void z_mul(rf_val *l, rf_val *r) { num_arith(l,r,*); }

// Language comparison for division by zero:
// `inf`: lua, mawk
// error: pretty much all others
static inline void z_div(rf_val *l, rf_val *r) {
    flt_arith(l,r,/);
}

// Language comparison for modulus by zero:
// `nan`: mawk
// error: pretty much all others
static inline void z_mod(rf_val *l, rf_val *r) {
    rf_flt res = fmod(numval(l), numval(r));
    assign_flt(l, res < 0 ? res + numval(r) : res);
}

static inline void z_pow(rf_val *l, rf_val *r) {
    assign_flt(l, pow(fltval(l), fltval(r)));
}

static inline void z_and(rf_val *l, rf_val *r) { int_arith(l,r,&);  }
static inline void z_or(rf_val *l, rf_val *r)  { int_arith(l,r,|);  }
static inline void z_xor(rf_val *l, rf_val *r) { int_arith(l,r,^);  }
static inline void z_shl(rf_val *l, rf_val *r) { int_arith(l,r,<<); }
static inline void z_shr(rf_val *l, rf_val *r) { int_arith(l,r,>>); }

static inline void z_num(rf_val *v) {
    switch (v->type) {
    case TYPE_STR:
        assign_flt(v, str2flt(v->u.s));
        break;
    case TYPE_NULL: case TYPE_ARR:
    case TYPE_RFN: case TYPE_CFN:
        assign_int(v, 0);
        break;
    default:
        break;
    }
}

static inline void z_neg(rf_val *v) {
    switch (v->type) {
    case TYPE_INT:
        assign_int(v, -intval(v));
        break;
    case TYPE_FLT:
        assign_flt(v, -fltval(v));
        break;
    case TYPE_STR:
        assign_flt(v, -str2flt(v->u.s));
        break;
    default:
        assign_int(v, 0);
        break;
    }
}

static inline void z_not(rf_val *v) {
    assign_int(v, ~intval(v));
}

static inline void z_eq(rf_val *l, rf_val *r) { cmp_eq(l,r,==); }
static inline void z_ne(rf_val *l, rf_val *r) { cmp_eq(l,r,!=); }
static inline void z_gt(rf_val *l, rf_val *r) { cmp_rel(l,r,>);  }
static inline void z_ge(rf_val *l, rf_val *r) { cmp_rel(l,r,>=); }
static inline void z_lt(rf_val *l, rf_val *r) { cmp_rel(l,r,<);  }
static inline void z_le(rf_val *l, rf_val *r) { cmp_rel(l,r,<=); }

static inline void z_lnot(rf_val *v) {
    assign_int(v, !test(v));
}

static inline void z_len(rf_val *v) {
    rf_int l = 0;
    switch (v->type) {

    // For integers:
    //   #x = ⌊log10(x)⌋  + 1 for x > 0
    //        ⌊log10(-x)⌋ + 2 for x < 0
    case TYPE_INT:
        l = v->u.i > 0 ? (rf_int) log10(v->u.i)  + 1 :
            v->u.i < 0 ? (rf_int) log10(-v->u.i) + 2 : 1;
        break;

    // TODO? defer to TYPE_INT behavior if v->u.f == (int) v->u.f
    case TYPE_FLT: {
        rf_str *fs = s_flt2str(v->u.f);
        l = fs->l;
        m_freestr(fs);
        break;
    }
    case TYPE_STR: l = v->u.s->l;        break;
    case TYPE_ARR: l = a_length(v->u.a); break;
    case TYPE_RFN: l = v->u.fn->code->n; break; // # of bytes
    case TYPE_SEQ: case TYPE_CFN: l = 1; break; // TODO - sequences
    default: break;
    }
    assign_int(v, l);
}

static inline void z_test(rf_val *v) {
    assign_int(v, test(v));
}

static inline void z_cat(rf_val *l, rf_val *r) {
    rf_str *lhs, *rhs;
    switch (l->type) {
    case TYPE_NULL: lhs = s_newstr(NULL, 0, 0); break;
    case TYPE_INT:  lhs = s_int2str(l->u.i);    break;
    case TYPE_FLT:  lhs = s_flt2str(l->u.f);    break;
    case TYPE_STR:  lhs = l->u.s;               break;
    case TYPE_SEQ: case TYPE_ARR:
    case TYPE_RFN: case TYPE_CFN:
        err("concatenation with incompatible type(s)");
    default: break;
    }

    switch (r->type) {
    case TYPE_NULL: rhs = s_newstr(NULL, 0, 0); break;
    case TYPE_INT:  rhs = s_int2str(r->u.i);    break;
    case TYPE_FLT:  rhs = s_flt2str(r->u.f);    break;
    case TYPE_STR:  rhs = r->u.s;               break;
    case TYPE_SEQ: case TYPE_ARR:
    case TYPE_RFN: case TYPE_CFN:
        err("concatenation with incompatible type(s)");
    default: break;
    }

    assign_str(l, s_concat(lhs, rhs, 0));
    if (!is_str(l)) { m_freestr(lhs); }
    if (!is_str(r)) { m_freestr(rhs); }
}

// Potentially very slow for strings; allocates 2 new string objects
// for every int or float LHS
static inline void z_idx(rf_val *l, rf_val *r) {
    switch (l->type) {
    case TYPE_INT: {
        rf_str *is = s_int2str(l->u.i);
        rf_int r1 = intval(r);

        // Index out-of-bounds: assign null
        if (r1 > is->l - 1 || r1 < 0)
            assign_null(l);
        else
            assign_str(l, s_newstr(&is->str[r1], 1, 0));
        m_freestr(is);
        break;
    }
    case TYPE_FLT: {
        rf_str *fs = s_flt2str(l->u.f);
        rf_int r1 = intval(r);
        if (r1 > fs->l - 1 || r1 < 0)
            assign_null(l);
        else
            assign_str(l, s_newstr(&fs->str[r1], 1, 0));
        m_freestr(fs);
        break;
    }
    case TYPE_STR: {
        if (is_seq(r)) {
            assign_str(l, s_substr(l->u.s, r->u.q->from, r->u.q->to, r->u.q->itvl));
        } else {
            rf_int r1 = intval(r);
            if (r1 > l->u.s->l - 1 || r1 < 0)
                assign_null(l);
            else
                assign_str(l, s_newstr(&l->u.s->str[r1], 1, 0));
        }
        break;
    }
    case TYPE_ARR:
        *l = *a_lookup(l->u.a, r, 0, 0);
        break;
    case TYPE_RFN: {
        rf_int r1 = intval(r);
        if (r1 > l->u.fn->code->n - 1 || r1 < 0)
            assign_null(l);
        else
            assign_int(l, l->u.fn->code->code[r1]);
        break;
    }
    default:
        assign_int(l, 0);
        break;
    }
}

// OP_PRINT functionality
static inline void z_print(rf_val *v) {
    switch (v->type) {
    case TYPE_NULL: printf("null");              break;
    case TYPE_INT:  printf("%"PRId64, v->u.i);   break;
    case TYPE_FLT:  printf("%g", v->u.f);        break;
    case TYPE_STR:  printf("%s", v->u.s->str);   break;
    case TYPE_SEQ:
        printf("seq: %"PRId64"..%"PRId64":%"PRId64,
                v->u.q->from, v->u.q->to, v->u.q->itvl);
        break;
    case TYPE_ARR:  printf("array: %p", v->u.a); break;
    case TYPE_RFN:  printf("fn: %p", v->u.fn);   break;
    case TYPE_CFN:  printf("fn: %p", v->u.cfn);  break;
    default: break;
    }
}

// TODO
#include <string.h>
static inline void init_argv(rf_arr *a, int argc, char **argv) {
    a_init(a);
    for (int i = 0; i < argc; ++i) {
        rf_str *s = s_newstr(argv[i], strlen(argv[i]), 1);
        a_insert_int(a, i, v_newstr(s), 1, 1);
        m_freestr(s);
    }
}

static inline void new_iter(rf_val *set) {
    rf_iter *new = malloc(sizeof(rf_iter));
    new->p = iter;
    iter = new;
    switch (set->type) {
    case TYPE_FLT:
        set->u.i = (rf_int) set->u.f;
        // Fall-through
    case TYPE_INT:
        iter->keys = NULL;
        iter->t = LOOP_SEQ;
        iter->st = 0;
        if (set->u.i >= 0) {
            iter->n = set->u.i + 1; // Inclusive
            iter->set.itvl = 1;
        } else {
            iter->n = -(set->u.i) + 1; // Inclusive
            iter->set.itvl = -1;
        }
        break;
    case TYPE_STR:
        iter->t = LOOP_STR;
        iter->n = set->u.s->l;
        iter->keys = NULL;
        iter->set.str = set->u.s->str;
        break;
    case TYPE_SEQ:
        iter->keys = NULL;
        iter->t = LOOP_SEQ;
        iter->set.itvl = set->u.q->itvl;
        if (iter->set.itvl > 0)
            iter->on = (set->u.q->to - set->u.q->from) + 1;
        else
            iter->on = (set->u.q->from - set->u.q->to) + 1;
        if (iter->on <= 0)
            iter->n = UINT64_MAX; // TODO "Infinite" loop
        else
            iter->n = (uint64_t) ceil(fabs(iter->on / (double) iter->set.itvl));
        iter->st = set->u.q->from;
        break;
    case TYPE_ARR:
        iter->t = LOOP_ARR;
        iter->on = iter->n = a_length(set->u.a);
        iter->keys = a_collect_keys(set->u.a);
        iter->set.arr = set->u.a;
        break;
    case TYPE_RFN:
        iter->t = LOOP_FN;
        iter->n = set->u.fn->code->n;
        iter->keys = NULL;
        iter->set.code = set->u.fn->code->code;
        break;
    case TYPE_CFN:
        err("cannot iterate over C function");
    default: break;
    }
}

static inline void destroy_iter(void) {
    rf_iter *old = iter;
    iter = iter->p;
    if (old->t == LOOP_ARR) {
        if (!(old->n + 1)) // Loop completed?
            free(old->keys - old->on);
        else
            free(old->keys - (old->on - old->n));
    }
    free(old);
}

static int exec(rf_code *c, rf_stack *sp, rf_stack *fp);

// VM entry point/initialization
int z_exec(rf_env *e) {
    h_init(&globals);
    iter = NULL;
    init_argv(&argv, e->argc, e->argv);

    // Offset for the argv; $0 should be the first user-provided arg
    aos = e->ff ? 3 : 2;

    l_register(&globals);

    // Add user-defined functions to the global hash table
    for (int i = 0; i < e->nf; ++i) {
        // Don't add anonymous functions to globals (rf_str should not
        // have a computed hash)
        if (!e->fn[i]->name->hash)
            continue;
        rf_val *fn = malloc(sizeof(rf_val));
        *fn = (rf_val) {TYPE_RFN, .u.fn = e->fn[i]};
        h_insert(&globals, e->fn[i]->name, fn, 1);
    }

    return exec(e->main.code, stack, stack);
}

// VM interpreter loop
static int exec(rf_code *c, rf_stack *sp, rf_stack *fp) {
    rf_stack *retp = sp; // Save original SP
    rf_val *tp; // Temp pointer
    register uint8_t *ip = c->code;
    while (1) {
        switch (*ip) {

// Unconditional jumps
#define j8  (ip += (int8_t) ip[1])
#define j16 (ip += (int16_t) ((ip[1] << 8) + ip[2]))

        case OP_JMP8:  j8;  break;
        case OP_JMP16: j16; break;

// Conditional jumps (pop stack unconditionally)
#define jc8(x)  (x ? j8  : (ip += 2)); --sp;
#define jc16(x) (x ? j16 : (ip += 3)); --sp;

        case OP_JNZ8:  jc8(test(&sp[-1].v));   break;
        case OP_JNZ16: jc16(test(&sp[-1].v));  break;
        case OP_JZ8:   jc8(!test(&sp[-1].v));  break;
        case OP_JZ16:  jc16(!test(&sp[-1].v)); break;

// Conditional jumps (pop stack if jump not taken)
#define xjc8(x)  if (x) j8;  else {--sp; ip += 2;}
#define xjc16(x) if (x) j16; else {--sp; ip += 3;}

        case OP_XJNZ8:  xjc8(test(&sp[-1].v));   break;
        case OP_XJNZ16: xjc16(test(&sp[-1].v));  break;
        case OP_XJZ8:   xjc8(!test(&sp[-1].v));  break;
        case OP_XJZ16:  xjc16(!test(&sp[-1].v)); break;

        // Initialize/cycle current iterator
        case OP_LOOP8: case OP_LOOP16: {
            int jmp16 = *ip == OP_LOOP16;
            if (!iter->n--) {
                if (jmp16)
                    ip += 3;
                else
                    ip += 2;
                break;
            }
            switch (iter->t) {
            case LOOP_SEQ:
                if (is_null(iter->v))
                    *iter->v = (rf_val) {TYPE_INT, .u.i = iter->st};
                else
                    iter->v->u.i += iter->set.itvl;
                break;
            case LOOP_STR:
                if (iter->k != NULL) {
                    if (is_null(iter->k)) {
                        assign_int(iter->k, 0);
                    } else {
                        iter->k->u.i += 1;
                    }
                }
                if (!is_null(iter->v)) {
                    m_freestr(iter->v->u.s);
                    iter->v->u.s = s_newstr(iter->set.str++, 1, 0);
                } else {
                    *iter->v = (rf_val) {TYPE_STR, .u.s = s_newstr(iter->set.str++, 1, 0)};
                }
                break;
            case LOOP_ARR:
                if (iter->k != NULL) {
                    *iter->k = *iter->keys;
                }
                *iter->v = *a_lookup(iter->set.arr, iter->keys, 0, 0);
                iter->keys++;
                break;
            case LOOP_FN:
                if (iter->k != NULL) {
                    if (is_null(iter->k)) {
                        assign_int(iter->k, 0);
                    } else {
                        iter->k->u.i += 1;
                    }
                }
                if (is_null(iter->v)) {
                    *iter->v = (rf_val) {TYPE_INT, .u.i = *iter->set.code++};
                } else {
                    iter->v->u.i = *iter->set.code++;
                }
                break;
            default: break;
            }

            // Treat byte(s) following OP_LOOP as unsigned since jumps
            // are always backward
            if (jmp16)
                ip -= (ip[1] << 8) + ip[2];
            else
                ip -= ip[1];
            break;
        }

        // Destroy the current iterator struct
        case OP_POPL: {
            destroy_iter();
            ++ip;
            break;
        }

        // Create iterator and jump to the corresponding OP_LOOP
        // instruction for initialization
        case OP_ITERV: case OP_ITERKV: {
            int k = *ip == OP_ITERKV;
            new_iter(&sp[-1].v); 
            --sp;
            assign_null(&sp++->v);

            // Reserve extra stack slot for k,v iterators
            if (k) {
                assign_null(&sp++->v);
                iter->k = &sp[-2].v;
            } else {
                iter->k = NULL;
            }
            iter->v = &sp[-1].v;
            j16;
            break;
        }

// Unary operations
// sp[-1].v is assumed to be safe to overwrite
#define unop(x) \
    z_##x(&sp[-1].v); \
    ++ip;

        case OP_LEN:  unop(len);  break;
        case OP_LNOT: unop(lnot); break;
        case OP_NEG:  unop(neg);  break;
        case OP_NOT:  unop(not);  break;
        case OP_NUM:  unop(num);  break;
        case OP_TEST: unop(test); break;

// Standard binary operations
// sp[-2].v and sp[-1].v are assumed to be safe to overwrite
#define binop(x) \
    z_##x(&sp[-2].v, &sp[-1].v); \
    --sp; \
    ++ip;

        case OP_ADD: binop(add); break;
        case OP_SUB: binop(sub); break;
        case OP_MUL: binop(mul); break;
        case OP_DIV: binop(div); break;
        case OP_MOD: binop(mod); break;
        case OP_POW: binop(pow); break;
        case OP_AND: binop(and); break;
        case OP_OR:  binop(or);  break;
        case OP_XOR: binop(xor); break;
        case OP_SHL: binop(shl); break;
        case OP_SHR: binop(shr); break;
        case OP_EQ:  binop(eq);  break;
        case OP_NE:  binop(ne);  break;
        case OP_GT:  binop(gt);  break;
        case OP_GE:  binop(ge);  break;
        case OP_LT:  binop(lt);  break;
        case OP_LE:  binop(le);  break;
        case OP_CAT: binop(cat); break;

// Pre-increment/decrement
// sp[-1].a is address of some variable's rf_val.
// Increment/decrement this value directly and replace the stack
// element with a copy of the value.
#define pre(x) \
    switch (sp[-1].a->type) { \
    case TYPE_INT: sp[-1].a->u.i += x; break; \
    case TYPE_FLT: sp[-1].a->u.f += x; break; \
    case TYPE_STR: \
        assign_flt(sp[-1].a, str2flt(sp[-1].a->u.s) + x); \
        break; \
    default: \
        assign_int(sp[-1].a, x); \
        break; \
    } \
    sp[-1].v = *sp[-1].a; \
    ++ip;

        case OP_PREINC: pre(1);  break;
        case OP_PREDEC: pre(-1); break;

// Post-increment/decrement
// sp[-1].a is address of some variable's rf_val. Create a copy of
// the raw value, then increment/decrement the rf_val at the given
// address.  Replace the stack element with the previously made copy
// and coerce to a numeric value if needed.
#define post(x) \
    tp = sp[-1].a; \
    sp[-1].v = *tp; \
    switch (tp->type) { \
    case TYPE_INT: tp->u.i += x; break; \
    case TYPE_FLT: tp->u.f += x; break; \
    case TYPE_STR: \
        assign_flt(tp, str2flt(tp->u.s) + x); \
        break; \
    default: \
        assign_int(tp, x); \
        break; \
    } \
    unop(num);

        case OP_POSTINC: post(1);  break;
        case OP_POSTDEC: post(-1); break;

// Compound assignment operations
// sp[-2].a is address of some variable's rf_val. Save the address
// and place a copy of the value in sp[-2].v. Perform the binary
// operation x and assign the result to the saved address.
#define cbinop(x) \
    tp = sp[-2].a; \
    sp[-2].v = *tp; \
    binop(x); \
    *tp = sp[-1].v;

        case OP_ADDX: cbinop(add); break;
        case OP_SUBX: cbinop(sub); break;
        case OP_MULX: cbinop(mul); break;
        case OP_DIVX: cbinop(div); break;
        case OP_MODX: cbinop(mod); break;
        case OP_CATX: cbinop(cat); break;
        case OP_POWX: cbinop(pow); break;
        case OP_ANDX: cbinop(and); break;
        case OP_ORX:  cbinop(or);  break;
        case OP_SHLX: cbinop(shl); break;
        case OP_SHRX: cbinop(shr); break;
        case OP_XORX: cbinop(xor); break;

        // Simple pop operation
        case OP_POP: --sp; ++ip; break;

        // Pop IP+1 values from stack
        case OP_POPI: sp -= ip[1]; ip += 2; break;

        // Push null literal on stack
        case OP_NULL: assign_null(&sp++->v); ++ip; break;

// Push immediate
// Assign integer value x to the top of the stack.
#define imm8(x) assign_int(&sp++->v, x);

        case OP_IMM8: imm8(ip[1]); ip += 2; break;
        case OP_IMM0: imm8(0);     ++ip;    break;
        case OP_IMM1: imm8(1);     ++ip;    break;
        case OP_IMM2: imm8(2);     ++ip;    break;

// Push constant
// Copy constant x from code object's constant table to the top of the
// stack.
#define pushk(x) sp++->v = c->k[(x)];

        case OP_PUSHK:  pushk(ip[1]); ip += 2; break;
        case OP_PUSHK0: pushk(0);     ++ip;    break;
        case OP_PUSHK1: pushk(1);     ++ip;    break;
        case OP_PUSHK2: pushk(2);     ++ip;    break;

// Push global address
// Assign the address of global variable x's rf_val in the globals
// table.
// h_lookup() will create an entry if needed, accommodating
// undeclared/uninitialized variable usage.
// Parser signals for this opcode for assignment or pre/post ++/--.
#define gbla(x) sp++->a = h_lookup(&globals, c->k[(x)].u.s, 1);

        case OP_GBLA:  gbla(ip[1]); ip += 2; break;
        case OP_GBLA0: gbla(0);     ++ip;    break;
        case OP_GBLA1: gbla(1);     ++ip;    break;
        case OP_GBLA2: gbla(2);     ++ip;    break;

// Push global value
// Copy the value of global variable x to the top of the stack.
// h_lookup() will create an entry if needed, accommodating
// undeclared/uninitialized variable usage.
// Parser signals for this opcode to be used when only needing the
// value, e.g. arithmetic.
#define gblv(x) sp++->v = *h_lookup(&globals, c->k[(x)].u.s, 0);

        case OP_GBLV:  gblv(ip[1]); ip += 2; break;
        case OP_GBLV0: gblv(0);     ++ip;    break;
        case OP_GBLV1: gblv(1);     ++ip;    break;
        case OP_GBLV2: gblv(2);     ++ip;    break;

// Push local address
// Push the address of FP[x] to the top of the stack.
#define lcla(x) sp++->a = &fp[(x)].v;

        case OP_LCLA:  lcla(ip[1]) ip += 2; break;
        case OP_LCLA0: lcla(0);    ++ip;    break;
        case OP_LCLA1: lcla(1);    ++ip;    break;
        case OP_LCLA2: lcla(2);    ++ip;    break;

// Push local value
// Copy the value of FP[x] to the top of the stack.
#define lclv(x) sp++->v = fp[(x)].v;

        case OP_LCLV:  lclv(ip[1]) ip += 2; break;
        case OP_LCLV0: lclv(0);    ++ip;    break;
        case OP_LCLV1: lclv(1);    ++ip;    break;
        case OP_LCLV2: lclv(2);    ++ip;    break;

// Calling convention
// Arguments are pushed in-order following the rf_val containing a
// pointer to the function to be called. Caller sets SP and FP to
// appropriate positions and cleans up stack afterward. Callee returns
// from exec() the number of values to be returned to the caller.
        case OP_CALL: {
            int nargs = ip[1];
            if (!is_fn(&sp[-nargs-1].v))
                err("attempt to call non-function value");

            int arity, nret;

            // User-defined functions
            if (is_rfn(&sp[-nargs-1].v)) {
                rf_fn *fn = sp[-nargs-1].v.u.fn;
                arity = fn->arity;

                // If user called function with too few arguments,
                // nullify stack slots and increment SP.
                if (nargs < arity) {
                    for (int i = nargs; i < arity; ++i) {
                        assign_null(&sp++->v);
                    }
                }
                
                // If user called function with too many arguments,
                // decrement SP so it points to the appropriate slot
                // for control transfer.
                else if (nargs > arity) {
                    sp -= (nargs - arity);
                }

                // Pass SP-arity-1 as the FP for the succeeding call
                // frame. Since the function is already at this
                // location in the stack, the compiler can reserve the
                // slot to accommodate any references a named function
                // makes to itself without any other work required
                // from the VM here. This is completely necessary for
                // local named functions, but globals benefit as well.
                nret = exec(fn->code, sp, sp - arity - 1);
                sp -= arity;

                // Copy the function's return value to the stack top -
                // this should be where the caller pushed the original
                // function.
                sp[-1].v = sp[arity].v;
            }
            
            // Built-in/C functions
            else {
                c_fn *fn = sp[-nargs-1].v.u.cfn;
                arity = fn->arity;

                // Most library functions are somewhat variadic; their
                // arity refers to the minimum number of arguments
                // they require.
                if (arity && nargs < arity) {
                    // If user called function with too few arguments,
                    // nullify stack slots.
                    for (int i = nargs; i < arity; ++i) {
                        assign_null(&sp[i].v);
                    }
                }
                // Decrement SP to serve as the FP for the function
                // call. Library functions assign their own return
                // values to SP-1.
                sp -= nargs;
                nret = fn->fn(&sp->v, nargs);
            }
            ip += 2;
            // TODO - hacky
            // If callee returns 0 and the next instruction is PRINT1,
            // skip over the instruction. This facilitates user
            // functions which conditionally return something.
            if (!nret) { 
                assign_null(&sp[-1].v);
                if (*ip == OP_PRINT1) {
                    ++ip;
                    --sp;
                }
            }
            break;
        }

        case OP_RET: return 0;

        // Caller expects return value to be at its original SP +
        // arity of the function. "clean up" any created locals by
        // copying the return value to the appropriate slot.
        case OP_RET1:
            retp->v = sp[-1].v;
            return 1;

// Create a sequential array of x elements from the top
// of the stack. Leave the array rf_val on the stack.
// Arrays index at 0 by default.
#define new_array(x) \
    tp = v_newarr(); \
    for (int i = (x) - 1; i >= 0; --i) { \
        --sp; \
        a_insert_int(tp->u.a, i, &sp->v, 1, 1); \
    } \
    sp++->v = *tp;

        case OP_ARRAY0: new_array(0);    ++ip;    break;
        case OP_ARRAY:  new_array(ip[1]) ip += 2; break;
        case OP_ARRAYK:
            new_array(c->k[ip[1]].u.i);
            ip += 2;
            break;

        case OP_IDXV:
            for (int i = -ip[1] - 1; i < -1; ++i) {
                if (sp[i].t <= TYPE_CFN) {
                    z_idx(&sp[i].v, &sp[i+1].v);
                    sp[i+1].v = sp[i].v;
                    continue;
                }
                switch (sp[i].a->type) {

                // Create array if sp[-2].a is an uninitialized variable
                case TYPE_NULL:
                    *sp[i+1].a = *v_newarr();
                    // Fall-through
                case TYPE_ARR:
                    sp[i+1].v = *a_lookup(sp[i].a->u.a, &sp[i+1].v, 0, 0);
                    break;

                // Dereference and call z_idx().
                case TYPE_INT: case TYPE_FLT:
                case TYPE_STR: case TYPE_RFN:
                    sp[i].v = *sp[-i].a;
                    z_idx(&sp[i].v, &sp[i+1].v);
                    sp[i+1].v = sp[i].v;
                    break;
                case TYPE_CFN:
                    err("attempt to subscript a C function");
                default:
                    break;
                }
            }
            sp -= ip[1];
            sp[-1].v = sp[ip[1] - 1].v;
            ip += 2;
            break;

        case OP_IDXA:
            for (int i = -ip[1] - 1; i < -1; ++i) {
                if (sp[i].t <= TYPE_CFN)
                    tp = &sp[i].v;
                else
                    tp = sp[i].a;

                switch (tp->type) {

                // Create array if sp[i].a is an uninitialized variable
                case TYPE_NULL:
                    *tp = *v_newarr();
                    // Fall-through
                case TYPE_ARR:
                    sp[i+1].a = a_lookup(tp->u.a, &sp[i+1].v, 1, 0);
                    break;

                // IDXA is invalid for all other types
                default:
                    err("invalid assignment");
                }
            }
            sp -= ip[1];
            sp[-1].a = sp[ip[1] - 1].a;
            ip += 2;
            break;

        // IDXA
        // Perform the lookup and leave the corresponding element's
        // rf_val address on the stack.
        case OP_IDXA1:

            // Accomodate OP_IDXA calls when SP-2 is a raw value
            if (sp[-2].t <= TYPE_CFN)
                tp = &sp[-2].v;
            else
                tp = sp[-2].a;

            switch (tp->type) {

            // Create array if sp[-2].a is an uninitialized variable
            case TYPE_NULL:
                *tp = *v_newarr();
                // Fall-through
            case TYPE_ARR:
                sp[-2].a = a_lookup(tp->u.a, &sp[-1].v, 1, 0);
                break;

            // IDXA is invalid for all other types
            default:
                err("invalid assignment");
            }
            --sp;
            ++ip;
            break;

        // IDXV
        // Perform the lookup and leave a copy of the corresponding
        // element's value on the stack.
        case OP_IDXV1:

            // All expressions e.g. x[y] are compiled to push the
            // address of the identifier being subscripted. However
            // OP_IDXV is emitted for all expressions not requiring
            // the address of the set element to be left on the stack.
            // In the event the instruction is OP_IDXV and SP-2
            // contains a raw value (not a pointer), the high order 64
            // bits will be the type tag of the rf_val instead of a
            // memory address. When that happens, defer to z_idx().
            if (sp[-2].t <= TYPE_CFN) {
                binop(idx);
                break;
            }

            switch (sp[-2].a->type) {

            // Create array if sp[-2].a is an uninitialized variable
            case TYPE_NULL:
                *sp[-2].a = *v_newarr();
                // Fall-through
            case TYPE_ARR:
                sp[-2].v = *a_lookup(sp[-2].a->u.a, &sp[-1].v, 0, 0);
                --sp;
                ++ip;
                break;

            // Dereference and call z_idx().
            case TYPE_INT: case TYPE_FLT: case TYPE_STR: case TYPE_RFN:
                sp[-2].v = *sp[-2].a;
                binop(idx);
                break;
            case TYPE_CFN:
                err("attempt to subscript a C function");
            default:
                break;
            }
            break;

        case OP_ARGA:
            sp[-1].a = a_lookup(&argv, &sp[-1].v, 1, aos);
            ++ip;
            break;

        case OP_ARGV:
            sp[-1].v = *a_lookup(&argv, &sp[-1].v, 0, aos);
            ++ip;
            break;

// Create a new "sequence" value.
// There are 8 different valid forms of a sequence; each has their own
// instruction.
//   seq:   x..y        SP[-2]..SP[-1]
//   seqf:  x..         SP[-1]..INT_MAX
//   seqt:  ..y         0..SP[-1]
//   seqe:  ..          0..INT_MAX
//   sseq:  x..y,z      SP[-3]..SP[-2],SP[-1]
//   sseqf: x..,z       SP[-2]..INT_MAX,SP[-1]
//   sseqt: ..y,z       0..SP[-2],SP[-1]
//   sseqe: ..,z        0..INT_MAX,SP[-1]
// If `z` is not provided, the interval is set to -1 if x > y (downward
// sequences). Otherwise, the interval is set to 1 (upward
// sequences).
#define z_seq(f,t,i,s) { \
    rf_seq *seq = malloc(sizeof(rf_seq)); \
    rf_int from = seq->from = (f); \
    rf_int to   = seq->to = (t); \
    rf_int itvl = (i); \
    seq->itvl   = itvl ? itvl : from > to ? -1 : 1; \
    s = (rf_val) {TYPE_SEQ, .u.q = seq}; \
}
        // x..y
        case OP_SEQ:
            z_seq(intval(&sp[-2].v),
                  intval(&sp[-1].v),
                  0,
                  sp[-2].v);
            --sp;
            ++ip;
            break;
        // x..
        case OP_SEQF:
            z_seq(intval(&sp[-1].v),
                  INT64_MAX,
                  0,
                  sp[-1].v);
            ++ip;
            break;
        // ..y
        case OP_SEQT:
            z_seq(0,
                  intval(&sp[-1].v),
                  0,
                  sp[-1].v);
            ++ip;
            break;
        // ..
        case OP_SEQE:
            ++sp;
            z_seq(0,
                  INT64_MAX,
                  0,
                  sp[-1].v);
            ++ip;
            break;
        // x..y,z
        case OP_SSEQ:
            z_seq(intval(&sp[-3].v),
                  intval(&sp[-2].v),
                  intval(&sp[-1].v),
                  sp[-3].v);
            sp -= 2;
            ++ip;
            break;
        // x..,z
        case OP_SSEQF:
            z_seq(intval(&sp[-2].v),
                  INT64_MAX,
                  intval(&sp[-1].v),
                  sp[-2].v);
            --sp;
            ++ip;
            break;
        // ..y,z
        case OP_SSEQT:
            z_seq(0,
                  intval(&sp[-2].v),
                  intval(&sp[-1].v),
                  sp[-2].v);
            --sp;
            ++ip;
            break;
        // ..,z
        case OP_SSEQE:
            z_seq(0,
                  INT64_MAX,
                  intval(&sp[-1].v),
                  sp[-1].v);
            ++ip;
            break;

        // Simple assignment
        // copy SP[-1] to *SP[-2] and leave value on stack.
        case OP_SET:
            sp[-2].v = *sp[-2].a = sp[-1].v;
            --sp;
            ++ip;
            break;

        // Print a single element from the stack
        case OP_PRINT1:
            z_print(&sp[-1].v);
            printf("\n");
            --sp;
            ++ip;
            break;

        // Print (IP+1) elements from the stack
        case OP_PRINT:
            for (int i = ip[1]; i > 0; --i) {
                z_print(&sp[-i].v);
                if (i > 1)
                    printf(" ");
            }
            printf("\n");
            sp -= ip[1];
            ip += 2;
            break;
        case OP_EXIT:
            exit(0);
        default:
            break;
        }
    }
    return 0;
}
