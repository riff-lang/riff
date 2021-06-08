#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "conf.h"
#include "lib.h"
#include "mem.h"
#include "table.h"
#include "vm.h"

static void err(const char *msg) {
    fprintf(stderr, "riff: [vm] %s\n", msg);
    exit(1);
}

static rf_htbl   globals;
static rf_tbl    argv;
static rf_iter  *iter;
static rf_stack  stack[VM_STACK_SIZE];

// Coerce string to int unconditionally
inline rf_int str2int(rf_str *s) {
    char *end;
    return u_str2i64(s->str, &end, 0);
}

// Coerce string to float unconditionally
inline rf_flt str2flt(rf_str *s) {
    char *end;
    return u_str2d(s->str, &end, 0);
}

// Return character pointed to by `end`, signaling whether the entire
// string was consumed or not.
static int str2num(rf_val *v) {
    char *end;
    rf_flt f = u_str2d(v->u.s->str, &end, 0);
    rf_int i = (rf_int) f;
    if (f == i) {
        assign_int(v, i);
    } else {
        assign_flt(v, f);
    }
    return *end;
}

// Integer arithmetic (Bitwise ops)
#define int_arith(l,r,op) \
    if (is_int(l) && is_int(r)) { \
        l->u.i = (l->u.i op r->u.i); \
    } else { \
        assign_int(l, (intval(l) op intval(r))); \
    }

// Floating-point arithmetic
#define flt_arith(l,r,op) \
    assign_flt(l, (numval(l) op numval(r)))

// "Polymorphic" arithmetic
#define num_arith(l,r,op) \
    if (is_int(l) && is_int(r)) { \
        l->u.i = (l->u.i op r->u.i); \
    } else { \
        rf_flt f = (numval(l) op numval(r)); \
        if (f == (rf_int) f) { \
            assign_int(l, ((rf_int) f)); \
        } else { \
            assign_flt(l, (f)); \
        } \
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
        rf_flt f = u_str2d(v->u.s->str, &end, 0);
        if (*end == '\0')
            return !!f;
        return !!v->u.s->l;
    }
    case TYPE_TBL: return !!t_length(v->u.t);
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
    case TYPE_NULL: case TYPE_TBL:
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
        v->u.i = -v->u.i;
        break;
    case TYPE_FLT:
        v->u.f = -v->u.f;
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
    case TYPE_FLT:
        l = (rf_int) snprintf(NULL, 0, "%g", v->u.f);
        break;
    case TYPE_STR: l = v->u.s->l;        break;
    case TYPE_TBL: l = t_length(v->u.t); break;
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
    case TYPE_SEQ: case TYPE_TBL:
    case TYPE_RFN: case TYPE_CFN:
        err("concatenation with incompatible type(s)");
    default: break;
    }

    switch (r->type) {
    case TYPE_NULL: rhs = s_newstr(NULL, 0, 0); break;
    case TYPE_INT:  rhs = s_int2str(r->u.i);    break;
    case TYPE_FLT:  rhs = s_flt2str(r->u.f);    break;
    case TYPE_STR:  rhs = r->u.s;               break;
    case TYPE_SEQ: case TYPE_TBL:
    case TYPE_RFN: case TYPE_CFN:
        err("concatenation with incompatible type(s)");
    default: break;
    }

    assign_str(l, s_concat(lhs, rhs, 0));
    if (!is_str(l)) { m_freestr(lhs); }
    if (!is_str(r)) { m_freestr(rhs); }
}

static inline void z_match(rf_val *l, rf_val *r) {
    assign_int(l, re_match(l->u.s, r->u.r));
}

static inline void z_nmatch(rf_val *l, rf_val *r) {
    assign_int(l, !re_match(l->u.s, r->u.r));
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
            l->u.s =  s_substr(l->u.s, r->u.q->from, r->u.q->to, r->u.q->itvl);
        } else {
            rf_int r1 = intval(r);
            if (r1 > l->u.s->l - 1 || r1 < 0)
                assign_null(l);
            else
                l->u.s = s_newstr(&l->u.s->str[r1], 1, 0);
        }
        break;
    }
    case TYPE_TBL:
        *l = *t_lookup(l->u.t, r, 0);
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
    case TYPE_NULL: printf("null");                 break;
    case TYPE_INT:  printf("%"PRId64, v->u.i);      break;
    case TYPE_FLT:  printf(FLT_PRINT_FMT, v->u.f);  break;
    case TYPE_STR:  printf("%s", v->u.s->str);      break;
    case TYPE_SEQ:
        printf("seq: %"PRId64"..%"PRId64":%"PRId64,
                v->u.q->from, v->u.q->to, v->u.q->itvl);
        break;
    case TYPE_TBL:  printf("table: %p", v->u.t);    break;
    case TYPE_RFN:  printf("fn: %p", v->u.fn);      break;
    case TYPE_CFN:  printf("fn: %p", v->u.cfn);     break;
    default: break;
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
    case TYPE_TBL:
        iter->t = LOOP_TBL;
        iter->on = iter->n = t_length(set->u.t);
        iter->keys = t_collect_keys(set->u.t);
        iter->set.tbl = set->u.t;
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
    if (old->t == LOOP_TBL) {
        if (!(old->n + 1)) // Loop completed?
            free(old->keys - old->on);
        else
            free(old->keys - (old->on - old->n));
    }
    free(old);
}

// TODO
#include <string.h>
static inline void init_argv(rf_tbl *t, rf_int os, int rf_argc, char **rf_argv) {
    t_init(t);
    for (rf_int i = 0; i < rf_argc; ++i) {
        rf_str *s = s_newstr(rf_argv[i], strlen(rf_argv[i]), 1);
        // TODO - this doesn't work correctly without directly
        // deferring to h_insert for negative indices NOR without
        // forcing insertion for non-negeative indices.
        if (i-os-1 < 0)
            h_insert(t->h, s_int2str(i-os-1), v_newstr(s), 1);
        else
            t_insert_int(t, (rf_int)i-os-1, v_newstr(s), 1, 1);
        m_freestr(s);
    }
}

static int exec(rf_code *c, rf_stack *sp, rf_stack *fp);

// VM entry point/initialization
int z_exec(rf_env *e) {
    h_init(&globals);
    iter = NULL;
    init_argv(&argv, e->ff, e->argc, e->argv);
    h_insert(&globals, s_newstr("arg", 3, 1), &(rf_val){TYPE_TBL, .u.t = &argv}, 1); 

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

#ifndef COMPUTED_GOTO
#ifdef __GNUC__
#define COMPUTED_GOTO
#endif
#endif

// VM interpreter loop
static int exec(rf_code *c, rf_stack *sp, rf_stack *fp) {
    if (sp - stack >= VM_STACK_SIZE)
        err("stack overflow");
    rf_stack *retp = sp; // Save original SP
    rf_val *tp; // Temp pointer
    register uint8_t *ip = c->code;

#ifndef COMPUTED_GOTO
// Use standard while loop with switch/case if computed goto is
// disabled or unavailable
#define z_case(l) case OP_##l:
#define z_break   break
    while (1) { switch (*ip) {
#else
#include "labels.h"
    dispatch();
#endif

// Unconditional jumps
#define j8  (ip += (int8_t) ip[1])
#define j16 (ip += (int16_t) ((ip[1] << 8) + ip[2]))

    z_case(JMP8)  j8;  z_break;
    z_case(JMP16) j16; z_break;

// Conditional jumps (pop stack unconditionally)
#define jc8(x)  (x ? j8  : (ip += 2)); --sp;
#define jc16(x) (x ? j16 : (ip += 3)); --sp;

    z_case(JNZ8)  jc8(test(&sp[-1].v));   z_break;
    z_case(JNZ16) jc16(test(&sp[-1].v));  z_break;
    z_case(JZ8)   jc8(!test(&sp[-1].v));  z_break;
    z_case(JZ16)  jc16(!test(&sp[-1].v)); z_break;

// Conditional jumps (pop stack if jump not taken)
#define xjc8(x)  if (x) j8;  else {--sp; ip += 2;}
#define xjc16(x) if (x) j16; else {--sp; ip += 3;}

    z_case(XJNZ8)  xjc8(test(&sp[-1].v));   z_break;
    z_case(XJNZ16) xjc16(test(&sp[-1].v));  z_break;
    z_case(XJZ8)   xjc8(!test(&sp[-1].v));  z_break;
    z_case(XJZ16)  xjc16(!test(&sp[-1].v)); z_break;

    // Initialize/cycle current iterator
    z_case(LOOP8) z_case(LOOP16) {
        int jmp16 = *ip == OP_LOOP16;
        if (!iter->n--) {
            if (jmp16)
                ip += 3;
            else
                ip += 2;
            z_break;
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
        case LOOP_TBL:
            if (iter->k != NULL) {
                *iter->k = *iter->keys;
            }
            *iter->v = *t_lookup(iter->set.tbl, iter->keys, 0);
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
        z_break;
    }

    // Destroy the current iterator struct
    z_case(POPL) {
        destroy_iter();
        ++ip;
        z_break;
    }

    // Create iterator and jump to the corresponding OP_LOOP
    // instruction for initialization
    z_case(ITERV) z_case(ITERKV) {
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
        z_break;
    }

// Unary operations
// sp[-1].v is assumed to be safe to overwrite
#define unop(x) \
    z_##x(&sp[-1].v); \
    ++ip;

    z_case(LEN)  unop(len);  z_break;
    z_case(LNOT) unop(lnot); z_break;
    z_case(NEG)  unop(neg);  z_break;
    z_case(NOT)  unop(not);  z_break;
    z_case(NUM)  unop(num);  z_break;
    z_case(TEST) unop(test); z_break;

// Standard binary operations
// sp[-2].v and sp[-1].v are assumed to be safe to overwrite
#define binop(x) \
    z_##x(&sp[-2].v, &sp[-1].v); \
    --sp; \
    ++ip;

    z_case(ADD)    binop(add);    z_break;
    z_case(SUB)    binop(sub);    z_break;
    z_case(MUL)    binop(mul);    z_break;
    z_case(DIV)    binop(div);    z_break;
    z_case(MOD)    binop(mod);    z_break;
    z_case(POW)    binop(pow);    z_break;
    z_case(AND)    binop(and);    z_break;
    z_case(OR)     binop(or);     z_break;
    z_case(XOR)    binop(xor);    z_break;
    z_case(SHL)    binop(shl);    z_break;
    z_case(SHR)    binop(shr);    z_break;
    z_case(EQ)     binop(eq);     z_break;
    z_case(NE)     binop(ne);     z_break;
    z_case(GT)     binop(gt);     z_break;
    z_case(GE)     binop(ge);     z_break;
    z_case(LT)     binop(lt);     z_break;
    z_case(LE)     binop(le);     z_break;
    z_case(CAT)    binop(cat);    z_break;
    z_case(MATCH)  binop(match);  z_break;
    z_case(NMATCH) binop(nmatch); z_break;

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

    z_case(PREINC) pre(1);  z_break;
    z_case(PREDEC) pre(-1); z_break;

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

    z_case(POSTINC) post(1);  z_break;
    z_case(POSTDEC) post(-1); z_break;

// Compound assignment operations
// sp[-2].a is address of some variable's rf_val. Save the address
// and place a copy of the value in sp[-2].v. Perform the binary
// operation x and assign the result to the saved address.
#define cbinop(x) \
    tp = sp[-2].a; \
    sp[-2].v = *tp; \
    binop(x); \
    *tp = sp[-1].v;

    z_case(ADDX) cbinop(add); z_break;
    z_case(SUBX) cbinop(sub); z_break;
    z_case(MULX) cbinop(mul); z_break;
    z_case(DIVX) cbinop(div); z_break;
    z_case(MODX) cbinop(mod); z_break;
    z_case(CATX) cbinop(cat); z_break;
    z_case(POWX) cbinop(pow); z_break;
    z_case(ANDX) cbinop(and); z_break;
    z_case(ORX)  cbinop(or);  z_break;
    z_case(SHLX) cbinop(shl); z_break;
    z_case(SHRX) cbinop(shr); z_break;
    z_case(XORX) cbinop(xor); z_break;

    // Simple pop operation
    z_case(POP) --sp; ++ip; z_break;

    // Pop IP+1 values from stack
    z_case(POPI) sp -= ip[1]; ip += 2; z_break;

    // Push null literal on stack
    z_case(NULL) assign_null(&sp++->v); ++ip; z_break;

// Push immediate
// Assign integer value x to the top of the stack.
#define imm(x) assign_int(&sp++->v, x);

    z_case(IMM8)  imm(ip[1]);              ip += 2; z_break;
    z_case(IMM16) imm((ip[1]<<8) + ip[2]); ip += 3; z_break;
    z_case(IMM0)  imm(0);                  ++ip;    z_break;
    z_case(IMM1)  imm(1);                  ++ip;    z_break;
    z_case(IMM2)  imm(2);                  ++ip;    z_break;

// Push constant
// Copy constant x from code object's constant table to the top of the
// stack.
#define pushk(x) sp++->v = c->k[(x)];

    z_case(PUSHK)  pushk(ip[1]); ip += 2; z_break;
    z_case(PUSHK0) pushk(0);     ++ip;    z_break;
    z_case(PUSHK1) pushk(1);     ++ip;    z_break;
    z_case(PUSHK2) pushk(2);     ++ip;    z_break;

// Push global address
// Assign the address of global variable x's rf_val in the globals
// table.
// h_lookup() will create an entry if needed, accommodating
// undeclared/uninitialized variable usage.
// Parser signals for this opcode for assignment or pre/post ++/--.
#define gbla(x) sp++->a = h_lookup(&globals, c->k[(x)].u.s, 1);

    z_case(GBLA)  gbla(ip[1]); ip += 2; z_break;
    z_case(GBLA0) gbla(0);     ++ip;    z_break;
    z_case(GBLA1) gbla(1);     ++ip;    z_break;
    z_case(GBLA2) gbla(2);     ++ip;    z_break;

// Push global value
// Copy the value of global variable x to the top of the stack.
// h_lookup() will create an entry if needed, accommodating
// undeclared/uninitialized variable usage.
// Parser signals for this opcode to be used when only needing the
// value, e.g. arithmetic.
#define gblv(x) sp++->v = *h_lookup(&globals, c->k[(x)].u.s, 0);

    z_case(GBLV)  gblv(ip[1]); ip += 2; z_break;
    z_case(GBLV0) gblv(0);     ++ip;    z_break;
    z_case(GBLV1) gblv(1);     ++ip;    z_break;
    z_case(GBLV2) gblv(2);     ++ip;    z_break;

// Push local address
// Push the address of FP[x] to the top of the stack.
#define lcla(x) sp++->a = &fp[(x)].v;

    z_case(LCLA)  lcla(ip[1]) ip += 2; z_break;
    z_case(LCLA0) lcla(0);    ++ip;    z_break;
    z_case(LCLA1) lcla(1);    ++ip;    z_break;
    z_case(LCLA2) lcla(2);    ++ip;    z_break;

// Push local value
// Copy the value of FP[x] to the top of the stack.
#define lclv(x) sp++->v = fp[(x)].v;

    z_case(LCLV)  lclv(ip[1]) ip += 2; z_break;
    z_case(LCLV0) lclv(0);    ++ip;    z_break;
    z_case(LCLV1) lclv(1);    ++ip;    z_break;
    z_case(LCLV2) lclv(2);    ++ip;    z_break;


    // Tailcalls
    // Recycle current call frame
    z_case(TCALL) {
        int nargs = ip[1] + 1;
        if (!is_fn(&sp[-nargs].v))
            err("attempt to call non-function value");
        if (is_rfn(&sp[-nargs].v)) {
            sp -= nargs;
            rf_fn *fn = sp->v.u.fn;
            int ar1 = sp - fp - 1;  // Current frame's "arity"
            int ar2 = fn->arity;    // Callee's arity

            // Recycle call frame
            for (int i = 0; i <= ar2; ++i) {
                fp[i].v = sp[i].v;
            }

            // Increment SP without nullifying slots (preserving
            // values) if number of arguments exceeds the frame's
            // current "arity"
            if (nargs-1 > ar1) {
                sp += (nargs - ar1 - 1);
                ar1 = nargs - 1;
            }

            // In the case of direct recursion and no call frame
            // adjustments needed, quickly reset IP and dispatch
            // control
            if (c == fn->code && ar1 == ar2) {
                ip = c->code;
                z_break;
            }

            // If callee's arity is larger than the current frame,
            // create stack space and nullify slots
            if (ar2 > ar1) {
                while (ar1++ < ar2)
                    assign_null(&sp++->v);
            }

            // Else, if the current frame is too large for the next
            // call, decrement SP
            else if (ar2 < ar1) {
                sp -= ar1 - ar2;
            }

            // Else else, if the size of the call frame is fine, but
            // the user didn't provide enough arguments, create stack
            // space and nullify slots
            else if (nargs <= ar2) {
                while (nargs++ <= ar2)
                    assign_null(&sp++->v);
            }
            c  = fn->code;
            ip = c->code;
            z_break;
        }

        // Fall-through to OP_CALL for C function calls
    }

    // Calling convention Arguments are pushed in-order following the
    // rf_val containing a pointer to the function to be called.
    // Caller sets SP and FP to appropriate positions and cleans up
    // stack afterward. Callee returns from exec() the number of
    // values to be returned to the caller.
    z_case(CALL) {
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
            // frame. Since the function is already at this location
            // in the stack, the compiler can reserve the slot to
            // accommodate any references a named function makes to
            // itself without any other work required from the VM
            // here. This is completely necessary for local named
            // functions, but globals benefit as well.
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
        z_break;
    }


    z_case(RET) return 0;

    // Caller expects return value to be at its original SP +
    // arity of the function. "clean up" any created locals by
    // copying the return value to the appropriate slot.
    z_case(RET1)
        retp->v = sp[-1].v;
        return 1;

// Create a sequential table of x elements from the top
// of the stack. Leave the table rf_val on the stack.
// Tables index at 0 by default.
#define new_tbl(x) \
    tp = v_newtbl(); \
    for (int i = (x) - 1; i >= 0; --i) { \
        --sp; \
        t_insert_int(tp->u.t, i, &sp->v, 1, 1); \
    } \
    sp++->v = *tp;

    z_case(TBL0) new_tbl(0);    ++ip;    z_break;
    z_case(TBL)  new_tbl(ip[1]) ip += 2; z_break;
    z_case(TBLK)
        new_tbl(c->k[ip[1]].u.i);
        ip += 2;
        z_break;

    z_case(IDXV)
        for (int i = -ip[1] - 1; i < -1; ++i) {
            if (sp[i].t <= TYPE_CFN) {
                z_idx(&sp[i].v, &sp[i+1].v);
                sp[i+1].v = sp[i].v;
                continue;
            }
            switch (sp[i].a->type) {

            // Create array if sp[-2].a is an uninitialized variable
            case TYPE_NULL:
                *sp[i+1].a = *v_newtbl();
                // Fall-through
            case TYPE_TBL:
                sp[i+1].v = *t_lookup(sp[i].a->u.t, &sp[i+1].v, 0);
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
        z_break;

    z_case(IDXA)
        for (int i = -ip[1] - 1; i < -1; ++i) {
            if (sp[i].t <= TYPE_CFN)
                tp = &sp[i].v;
            else
                tp = sp[i].a;

            switch (tp->type) {

            // Create array if sp[i].a is an uninitialized variable
            case TYPE_NULL:
                *tp = *v_newtbl();
                // Fall-through
            case TYPE_TBL:
                sp[i+1].a = t_lookup(tp->u.t, &sp[i+1].v, 1);
                break;

            // IDXA is invalid for all other types
            default:
                err("invalid assignment");
            }
        }
        sp -= ip[1];
        sp[-1].a = sp[ip[1] - 1].a;
        ip += 2;
        z_break;

    // IDXA
    // Perform the lookup and leave the corresponding element's
    // rf_val address on the stack.
    z_case(IDXA1)

        // Accomodate OP_IDXA calls when SP-2 is a raw value
        if (sp[-2].t <= TYPE_CFN)
            tp = &sp[-2].v;
        else
            tp = sp[-2].a;

        switch (tp->type) {

        // Create array if sp[-2].a is an uninitialized variable
        case TYPE_NULL:
            *tp = *v_newtbl();
            // Fall-through
        case TYPE_TBL:
            sp[-2].a = t_lookup(tp->u.t, &sp[-1].v, 1);
            break;

        // IDXA is invalid for all other types
        default:
            err("invalid assignment");
        }
        --sp;
        ++ip;
        z_break;

    // IDXV
    // Perform the lookup and leave a copy of the corresponding
    // element's value on the stack.
    z_case(IDXV1)

        // All expressions e.g. x[y] are compiled to push the address
        // of the identifier being subscripted. However OP_IDXV is
        // emitted for all expressions not requiring the address of
        // the set element to be left on the stack.  In the event the
        // instruction is OP_IDXV and SP-2 contains a raw value (not a
        // pointer), the high order 64 bits will be the type tag of
        // the rf_val instead of a memory address. When that happens,
        // defer to z_idx().
        if (sp[-2].t <= TYPE_CFN) {
            binop(idx);
            z_break;
        }

        switch (sp[-2].a->type) {

        // Create array if sp[-2].a is an uninitialized variable
        case TYPE_NULL:
            *sp[-2].a = *v_newtbl();
            // Fall-through
        case TYPE_TBL:
            sp[-2].v = *t_lookup(sp[-2].a->u.t, &sp[-1].v, 0);
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
        z_break;

    z_case(ARGA)
        sp[-1].a = t_lookup(&argv, &sp[-1].v, 1);
        ++ip;
        z_break;

    z_case(ARGV)
        sp[-1].v = *t_lookup(&argv, &sp[-1].v, 0);
        ++ip;
        z_break;

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
    z_case(SEQ)
        z_seq(intval(&sp[-2].v),
              intval(&sp[-1].v),
              0,
              sp[-2].v);
        --sp;
        ++ip;
        z_break;
    // x..
    z_case(SEQF)
        z_seq(intval(&sp[-1].v),
              INT64_MAX,
              0,
              sp[-1].v);
        ++ip;
        z_break;
    // ..y
    z_case(SEQT)
        z_seq(0,
              intval(&sp[-1].v),
              0,
              sp[-1].v);
        ++ip;
        z_break;
    // ..
    z_case(SEQE)
        ++sp;
        z_seq(0,
              INT64_MAX,
              0,
              sp[-1].v);
        ++ip;
        z_break;
    // x..y,z
    z_case(SSEQ)
        z_seq(intval(&sp[-3].v),
              intval(&sp[-2].v),
              intval(&sp[-1].v),
              sp[-3].v);
        sp -= 2;
        ++ip;
        z_break;
    // x..,z
    z_case(SSEQF)
        z_seq(intval(&sp[-2].v),
              INT64_MAX,
              intval(&sp[-1].v),
              sp[-2].v);
        --sp;
        ++ip;
        z_break;
    // ..y,z
    z_case(SSEQT)
        z_seq(0,
              intval(&sp[-2].v),
              intval(&sp[-1].v),
              sp[-2].v);
        --sp;
        ++ip;
        z_break;
    // ..,z
    z_case(SSEQE)
        z_seq(0,
              INT64_MAX,
              intval(&sp[-1].v),
              sp[-1].v);
        ++ip;
        z_break;

    // Simple assignment
    // copy SP[-1] to *SP[-2] and leave value on stack.
    z_case(SET)
        sp[-2].v = *sp[-2].a = sp[-1].v;
        --sp;
        ++ip;
        z_break;

    // Print a single element from the stack
    z_case(PRINT1)
        z_print(&sp[-1].v);
        printf("\n");
        --sp;
        ++ip;
        z_break;

    // Print (IP+1) elements from the stack
    z_case(PRINT)
        for (int i = ip[1]; i > 0; --i) {
            z_print(&sp[-i].v);
            if (i > 1)
                printf(" ");
        }
        printf("\n");
        sp -= ip[1];
        ip += 2;
        z_break;
    z_case(EXIT)
        exit(0);
#ifndef COMPUTED_GOTO
    } }
#endif
    return 0;
}
