#include "vm.h"

#include "conf.h"
#include "lib.h"
#include "mem.h"
#include "string.h"

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void err(const char *msg) {
    fprintf(stderr, "riff: [vm] %s\n", msg);
    exit(1);
}

static rf_htab   globals;
static rf_tab    argv;
static rf_tab    fldv;
static rf_iter  *iter;
static rf_stack  stack[VM_STACK_SIZE];

// Integer arithmetic (Bitwise ops)
#define int_arith(l,r,op) set_int(l, (intval(l) op intval(r)))

// Floating-point arithmetic (div)
#define flt_arith(l,r,op) set_flt(l, (fltval(l) op fltval(r)))

// "Polymorphic" arithmetic (add, sub, mul)
#define num_arith(l,r,op) \
    if (is_int(l) && is_int(r)) { \
        l->i = (l->i op r->i); \
    } else { \
        flt_arith(l,r,op); \
    }

// Return boolean result of value (0/1)
static inline int test(rf_val *v) {
    switch (v->type) {
    case TYPE_INT: return !!(v->i);
    case TYPE_FLT: return !!(v->f);

    // If entire string is a numeric value, return logical result of
    // the number. Otherwise, return whether the string is longer than
    // 0.
    case TYPE_STR: {
        char *end;
        rf_flt f = u_str2d(v->s->str, &end, 0);
        if (!*end) {
            // Check for literal '0' character in string
            if (f == 0.0 && s_haszero(v->s))
                return 0;
            else
                return !!f;
        }
        return !!s_len(v->s);
    }
    case TYPE_TAB: return !!t_logical_size(v->t);
    case TYPE_RE:
    case TYPE_RNG:
    case TYPE_RFN:
    case TYPE_CFN:
        return 1;
    default: return 0;
    }
}

#define Z_UOP(op)   static inline void z_##op(rf_val *v)
#define Z_BINOP(op) static inline void z_##op(rf_val *l, rf_val *r)

Z_BINOP(add) { num_arith(l,r,+); }
Z_BINOP(sub) { num_arith(l,r,-); }
Z_BINOP(mul) { num_arith(l,r,*); }

// Language comparison for division by zero:
// 0/0 = nan; 1/0 = inf : lua, mawk
// error: pretty much all others
Z_BINOP(div) { flt_arith(l,r,/); }

// Language comparison for modulus by zero:
// `nan`: mawk
// error: pretty much all others
Z_BINOP(mod) {
    rf_flt res = fmod(numval(l), numval(r));
    set_flt(l, res < 0 ? res + numval(r) : res);
}

Z_BINOP(pow) { set_flt(l, pow(fltval(l), fltval(r))); }

Z_BINOP(and) { int_arith(l,r,&);  }
Z_BINOP(or)  { int_arith(l,r,|);  }
Z_BINOP(xor) { int_arith(l,r,^);  }
Z_BINOP(shl) { int_arith(l,r,<<); }
Z_BINOP(shr) { int_arith(l,r,>>); }

Z_UOP(num) {
    switch (v->type) {
    case TYPE_INT:
    case TYPE_FLT:
        break;
    case TYPE_STR:
        set_flt(v, str2flt(v->s));
        break;
    default:
        set_int(v, 0);
        break;
    }
}

Z_UOP(neg) {
    switch (v->type) {
    case TYPE_INT: v->i = -v->i;               break;
    case TYPE_FLT: v->f = -v->f;               break;
    case TYPE_STR: set_flt(v, -str2flt(v->s)); break;
    default:       set_int(v, 0);              break;
    }
}

Z_UOP(not) { set_int(v, ~intval(v)); }

// == and != operators
#define cmp_eq(l,r,op) \
    if (is_int(l) && is_int(r)) { \
        l->i = (l->i op r->i); \
    } else if (is_null(l) ^ is_null(r)) { \
        set_int(l, !(0 op 0)); \
    } else if (is_str(l) && is_str(r)) { \
        set_int(l, ((l->s) op (r->s))); \
    } else if (is_str(l) && !is_str(r)) { \
        if (!s_len(l->s)) { \
            set_int(l, !(0 op 0)); \
            return; \
        } \
        char *end; \
        rf_flt f = u_str2d(l->s->str, &end, 0); \
        if (*end) { \
            set_int(l, 0); \
        } else { \
            set_int(l, (f op numval(r))); \
        } \
    } else if (!is_str(l) && is_str(r)) { \
        if (!s_len(r->s)) { \
            set_int(l, !(0 op 0)); \
            return; \
        } \
        char *end; \
        rf_flt f = u_str2d(r->s->str, &end, 0); \
        if (*end) { \
            set_int(l, 0); \
        } else { \
            set_int(l, (numval(l) op f)); \
        } \
    } else { \
        num_arith(l,r,op); \
    }

// >, <, >= and <= operators
#define cmp_rel(l,r,op) \
    if (is_int(l) && is_int(r)) { \
        l->i = (l->i op r->i); \
    } else { \
        set_int(l, (numval(l) op numval(r))); \
    }

Z_BINOP(eq) { cmp_eq(l,r,==);  }
Z_BINOP(ne) { cmp_eq(l,r,!=);  }
Z_BINOP(gt) { cmp_rel(l,r,>);  }
Z_BINOP(ge) { cmp_rel(l,r,>=); }
Z_BINOP(lt) { cmp_rel(l,r,<);  }
Z_BINOP(le) { cmp_rel(l,r,<=); }

Z_UOP(lnot) { set_int(v, !test(v)); }

Z_UOP(len) {
    rf_int l = 0;
    switch (v->type) {

    // For integers:
    //   #x = ⌊log10(x)⌋  + 1 for x > 0
    //        ⌊log10(-x)⌋ + 2 for x < 0
    case TYPE_INT:
        if (v->i == INT64_MIN) {
            l = 20;
        } else {
            l = v->i > 0 ? (rf_int) log10(v->i)  + 1 :
                v->i < 0 ? (rf_int) log10(-v->i) + 2 : 1;
        }
        v->i = l;
        return;
    case TYPE_FLT:
        l = (rf_int) snprintf(NULL, 0, "%g", v->f);
        break;
    case TYPE_STR: l = s_len(v->s); break;
    case TYPE_TAB: l = t_logical_size(v->t); break;
    case TYPE_RE:   // TODO - extract something from PCRE pattern?
    case TYPE_RNG:  // TODO
    case TYPE_RFN:
    case TYPE_CFN:
        l = 1;
        break;
    default: break;
    }
    set_int(v, l);
}

Z_BINOP(cat) {
    char *lhs, *rhs;
    char temp_lhs[32];
    char temp_rhs[32];
    if (!is_str(l)) {
        switch (l->type) {
        case TYPE_INT: u_int2str(l->i, temp_lhs); break;
        case TYPE_FLT: u_flt2str(l->f, temp_lhs); break;
        default:       temp_lhs[0] = '\0';        break;
        }
        lhs = temp_lhs;
    } else {
        lhs = l->s->str;
    }
    if (!is_str(r)) {
        switch (r->type) {
        case TYPE_INT: u_int2str(r->i, temp_rhs); break;
        case TYPE_FLT: u_flt2str(r->f, temp_rhs); break;
        default:       temp_rhs[0] = '\0';        break;
        }
        rhs = temp_rhs;
    } else {
        rhs = r->s->str;
    }
    set_str(l, s_new_concat(lhs, rhs));
}

static inline rf_int match(rf_val *l, rf_val *r) {
    // Common case: LHS string, RHS regex
    if (is_str(l) && is_re(r))
        return re_match(l->s->str, s_len(l->s), r->r, 1);
    char *lhs;
    size_t len = 0;
    char temp_lhs[32];
    char temp_rhs[32];
    if (!is_str(l)) {
        switch (l->type) {
        case TYPE_INT: len = u_int2str(l->i, temp_lhs); break;
        case TYPE_FLT: len = u_flt2str(l->f, temp_lhs); break;
        default:       temp_lhs[0] = '\0';              break;
        }
        lhs = temp_lhs;
    } else {
        lhs = l->s->str;
        len = s_len(l->s);
    }
    if (!is_re(r)) {
        rf_re *temp_re;
        rf_int res;
        int errcode;
        int capture = 0;
        switch (r->type) {
        case TYPE_INT: u_int2str(r->i, temp_rhs); break;
        case TYPE_FLT: u_flt2str(r->f, temp_rhs); break;
        case TYPE_STR:
            capture = 1;
            temp_re = re_compile(r->s->str, s_len(r->s), 0, &errcode);
            goto do_match;
        default:       temp_rhs[0] = '\0'; break;
        }
        temp_re = re_compile(temp_rhs, PCRE2_ZERO_TERMINATED, 0, &errcode);
do_match:
        // Check for invalid regex in RHS
        // TODO treat as literal string in this case? (PCRE2_LITERAL)
        if (errcode != 100) {
            PCRE2_UCHAR errstr[0x200];
            pcre2_get_error_message(errcode, errstr, 0x200);
            err((const char *) errstr);
        }
        res = re_match(lhs, len, temp_re, capture);
        re_free(temp_re);
        return res;
    } else {
        return re_match(lhs, len, r->r, 1);
    }
}

Z_BINOP(match)  { set_int(l,  match(l, r)); }
Z_BINOP(nmatch) { set_int(l, !match(l, r)); }

Z_BINOP(idx) {
    char temp[32];
    switch (l->type) {
    case TYPE_INT: {
        u_int2str(l->i, temp);
        if (is_rng(r)) {
            set_str(l, s_substr(temp, r->q->from, r->q->to, r->q->itvl));
        } else {
            rf_int r1  = intval(r);
            rf_int len = (rf_int) strlen(temp);
            if (r1 < 0)
                r1 += len;
            if (r1 > len - 1 || r1 < 0)
                set_null(l);
            else
                set_str(l, s_new(temp + r1, 1));
        }
        break;
    }
    case TYPE_FLT: {
        u_flt2str(l->f, temp);
        if (is_rng(r)) {
            set_str(l, s_substr(temp, r->q->from, r->q->to, r->q->itvl));
        } else {
            rf_int r1  = intval(r);
            rf_int len = (rf_int) strlen(temp);
            if (r1 < 0)
                r1 += len;
            if (r1 > len - 1 || r1 < 0)
                set_null(l);
            else
                set_str(l, s_new(temp + r1, 1));
        }
        break;
    }
    case TYPE_STR: {
        if (is_rng(r)) {
            l->s = s_substr(l->s->str, r->q->from, r->q->to, r->q->itvl);
        } else {
            rf_int r1  = intval(r);
            rf_int len = (rf_int) s_len(l->s);
            if (r1 < 0)
                r1 += len;
            if (r1 > len - 1 || r1 < 0)
                set_null(l);
            else
                l->s = s_new(&l->s->str[r1], 1);
        }
        break;
    }
    case TYPE_TAB:
        *l = *t_lookup(l->t, r, 0);
        break;
    default:
        set_null(l);
        break;
    }
}

static inline void new_iter(rf_val *set) {
    rf_iter *new = malloc(sizeof(rf_iter));
    new->p = iter;
    iter = new;
    switch (set->type) {
    case TYPE_NULL:
        iter->t = LOOP_NULL;
        iter->keys = NULL;
        iter->n = 1;
        break;
    case TYPE_FLT:
        set->i = (rf_int) set->f;
        // Fall-through
    case TYPE_INT:
        iter->keys = NULL;
        iter->t = LOOP_RNG;
        iter->st = 0;
        if (set->i >= 0) {
            iter->n = set->i + 1; // Inclusive
            iter->set.itvl = 1;
        } else {
            iter->n = -(set->i) + 1; // Inclusive
            iter->set.itvl = -1;
        }
        break;
    case TYPE_STR:
        iter->t = LOOP_STR;
        iter->n = s_len(set->s);
        iter->keys = NULL;
        iter->set.str = set->s->str;
        break;
    case TYPE_RE:
        err("cannot iterate over regular expression");
    case TYPE_RNG:
        iter->keys = NULL;
        iter->t = LOOP_RNG;
        iter->set.itvl = set->q->itvl;
        if (iter->set.itvl > 0)
            iter->on = (set->q->to - set->q->from) + 1;
        else
            iter->on = (set->q->from - set->q->to) + 1;
        if (iter->on <= 0)
            iter->n = UINT64_MAX; // TODO "Infinite" loop
        else
            iter->n = (rf_uint) ceil(fabs(iter->on / (double) iter->set.itvl));
        iter->st = set->q->from;
        break;
    case TYPE_TAB:
        iter->t = LOOP_TAB;
        iter->on = iter->n = t_logical_size(set->t);
        iter->keys = t_collect_keys(set->t);
        iter->set.tab = set->t;
        break;
    case TYPE_RFN:
    case TYPE_CFN:
        err("cannot iterate over function");
    default: break;
    }
}

static inline void destroy_iter(void) {
    rf_iter *old = iter;
    iter = iter->p;
    if (old->t == LOOP_TAB) {
        if (!(old->n + 1)) // Loop completed?
            free(old->keys - old->on);
        else
            free(old->keys - (old->on - old->n));
    }
    free(old);
}

static inline void init_argv(rf_tab *t, rf_int arg0, int rf_argc, char **rf_argv) {
    t_init(t);
    for (rf_int i = 0; i < rf_argc; ++i) {
        rf_val v = (rf_val) {
            TYPE_STR,
            .s = s_new(rf_argv[i], strlen(rf_argv[i]))
        };
        rf_int idx = i - arg0;
        if (idx < 0)
            ht_insert_val(t->h, &(rf_val){TYPE_INT, .i = idx}, &v);
        else
            t_insert_int(t, idx, &v);
    }
}

static inline int exec(uint8_t *, rf_val *, rf_stack *, rf_stack *);

#define add_user_funcs() \
    for (int i = 0; i < e->nf; ++i) { \
        if (e->fn[i]->name->hash) { \
            ht_insert_str(&globals, e->fn[i]->name, &(rf_val){TYPE_RFN, .fn = e->fn[i]}); \
        } \
    }

// VM entry point/initialization
int z_exec(rf_env *e) {
    ht_init(&globals);
    iter = NULL;
    t_init(&fldv);
    re_register_fldv(&fldv);
    init_argv(&argv, e->arg0, e->argc, e->argv);
    ht_insert_cstr(&globals, "arg", &(rf_val){TYPE_TAB, .t = &argv});
    l_register_builtins(&globals);
    // Add user-defined functions to the global hash table
    add_user_funcs();
    return exec(e->main.code->code, e->main.code->k, stack, stack);
}

// Reentry point for eval()
int z_exec_reenter(rf_env *e, rf_stack *fp) {
    // Add user-defined functions to the global hash table
    add_user_funcs();
    return exec(e->main.code->code, e->main.code->k, fp, fp);
}

#ifndef COMPUTED_GOTO
#ifdef __GNUC__
#define COMPUTED_GOTO
#endif
#endif

// VM interpreter loop
static inline int exec(uint8_t *ep, rf_val *k, rf_stack *sp, rf_stack *fp) {
    if (sp - stack >= VM_STACK_SIZE)
        err("stack overflow");
    rf_stack *retp = sp; // Save original SP
    rf_val   *tp;        // Temp pointer
    register uint8_t *ip = ep;

#ifndef COMPUTED_GOTO
// Use standard while loop with switch/case if computed goto is
// disabled or unavailable
#define L(l) case OP_##l:
#define BREAK   break
    while (1) { switch (*ip) {
#else
#define L(l)  L_##l:
#define BREAK    DISPATCH()
#define DISPATCH() goto *dispatch_labels[*ip]
    static void *dispatch_labels[] = {
#define OPCODE(x,y,z) &&L_##x
#include "opcodes.h"
    };
    DISPATCH();
#endif

// Unconditional jumps
#define j8()  (ip += (int8_t) ip[1])
#define j16() (ip += (int16_t) ((ip[1] << 8) + ip[2]))

L(JMP8)     j8();  BREAK;
L(JMP16)    j16(); BREAK;

// Conditional jumps (pop stack unconditionally)
#define jc8(x)  (x ? j8()  : (ip += 2)); --sp
#define jc16(x) (x ? j16() : (ip += 3)); --sp

L(JNZ8)     jc8(test(&sp[-1].v));   BREAK;
L(JNZ16)    jc16(test(&sp[-1].v));  BREAK;
L(JZ8)      jc8(!test(&sp[-1].v));  BREAK;
L(JZ16)     jc16(!test(&sp[-1].v)); BREAK;


// Conditional jumps (pop stack if jump not taken)
#define xjc8(x)  if (x) j8();  else {--sp; ip += 2;}
#define xjc16(x) if (x) j16(); else {--sp; ip += 3;}

L(XJNZ8)    xjc8(test(&sp[-1].v));   BREAK;
L(XJNZ16)   xjc16(test(&sp[-1].v));  BREAK;
L(XJZ8)     xjc8(!test(&sp[-1].v));  BREAK;
L(XJZ16)    xjc16(!test(&sp[-1].v)); BREAK;

// Initialize/cycle current iterator
L(LOOP8)
L(LOOP16) {
    int jmp16 = *ip == OP_LOOP16;
    if (!iter->n--) {
        if (jmp16)
            ip += 3;
        else
            ip += 2;
        BREAK;
    }
    switch (iter->t) {
    case LOOP_RNG:
        if (is_null(iter->v))
            *iter->v = (rf_val) {TYPE_INT, .i = iter->st};
        else
            iter->v->i += iter->set.itvl;
        break;
    case LOOP_STR:
        if (iter->k != NULL) {
            if (is_null(iter->k)) {
                set_int(iter->k, 0);
            } else {
                iter->k->i += 1;
            }
        }
        if (!is_null(iter->v)) {
            iter->v->s = s_new(iter->set.str++, 1);
        } else {
            *iter->v = (rf_val) {TYPE_STR, .s = s_new(iter->set.str++, 1)};
        }
        break;
    case LOOP_TAB:
        if (iter->k != NULL) {
            *iter->k = *iter->keys;
        }
        *iter->v = *t_lookup(iter->set.tab, iter->keys, 0);
        iter->keys++;
        break;
    case LOOP_NULL:
        set_null(iter->v);
        if (iter->k != NULL) {
            set_null(iter->k);
        }
        break;
    default: break;
    }

    // Treat byte(s) following OP_LOOP as unsigned since jumps are always
    // backward
    if (jmp16)
        ip -= (ip[1] << 8) + ip[2];
    else
        ip -= ip[1];
    BREAK;
}

// Destroy the current iterator struct
L(POPL)     destroy_iter();
            ++ip;
            BREAK;

// Create iterator and jump to the corresponding OP_LOOP instruction for
// initialization
L(ITERV)
L(ITERKV) {
    int k = *ip == OP_ITERKV;
    new_iter(&sp[-1].v); 
    --sp;
    set_null(&sp++->v);

    // Reserve extra stack slot for k,v iterators
    if (k) {
        set_null(&sp++->v);
        iter->k = &sp[-2].v;
    } else {
        iter->k = NULL;
    }
    iter->v = &sp[-1].v;
    j16();
    BREAK;
}

// Unary operations
// sp[-1].v is assumed to be safe to overwrite
#define unop(x) z_##x(&sp[-1].v); ++ip

L(LEN)      unop(len);  BREAK;
L(LNOT)     unop(lnot); BREAK;
L(NEG)      unop(neg);  BREAK;
L(NOT)      unop(not);  BREAK;
L(NUM)      unop(num);  BREAK;

// Standard binary operations
// sp[-2].v and sp[-1].v are assumed to be safe to overwrite
#define binop(x) z_##x(&sp[-2].v, &sp[-1].v); --sp; ++ip

L(ADD)      binop(add);    BREAK;
L(SUB)      binop(sub);    BREAK;
L(MUL)      binop(mul);    BREAK;
L(DIV)      binop(div);    BREAK;
L(MOD)      binop(mod);    BREAK;
L(POW)      binop(pow);    BREAK;
L(AND)      binop(and);    BREAK;
L(OR)       binop(or);     BREAK;
L(XOR)      binop(xor);    BREAK;
L(SHL)      binop(shl);    BREAK;
L(SHR)      binop(shr);    BREAK;
L(EQ)       binop(eq);     BREAK;
L(NE)       binop(ne);     BREAK;
L(GT)       binop(gt);     BREAK;
L(GE)       binop(ge);     BREAK;
L(LT)       binop(lt);     BREAK;
L(LE)       binop(le);     BREAK;
L(CAT)      binop(cat);    BREAK;
L(MATCH)    binop(match);  BREAK;
L(NMATCH)   binop(nmatch); BREAK;

// Pre-increment/decrement
// sp[-1].a is address of some variable's rf_val.
// Increment/decrement this value directly and replace the stack element with a
// copy of the value.
#define pre(x) \
    switch (sp[-1].a->type) { \
    case TYPE_INT: sp[-1].a->i += x; break; \
    case TYPE_FLT: sp[-1].a->f += x; break; \
    case TYPE_STR: \
        set_flt(sp[-1].a, str2flt(sp[-1].a->s) + x); \
        break; \
    default: \
        set_int(sp[-1].a, x); \
        break; \
    } \
    sp[-1].v = *sp[-1].a; \
    ++ip

L(PREINC)   pre(1);  BREAK;
L(PREDEC)   pre(-1); BREAK;

// Post-increment/decrement
// sp[-1].a is address of some variable's rf_val. Create a copy of the raw
// value, then increment/decrement the rf_val at the given address. Replace the
// stack element with the previously made copy and coerce to a numeric value if
// needed.
#define post(x) \
    tp = sp[-1].a; \
    sp[-1].v = *tp; \
    switch (tp->type) { \
    case TYPE_INT: tp->i += x; break; \
    case TYPE_FLT: tp->f += x; break; \
    case TYPE_STR: \
        set_flt(tp, str2flt(tp->s) + x); \
        break; \
    default: \
        set_int(tp, x); \
        break; \
    } \
    unop(num)

L(POSTINC)  post(1);  BREAK;
L(POSTDEC)  post(-1); BREAK;

// Compound assignment operations
// sp[-2].a is address of some variable's rf_val. Save the address and place a
// copy of the value in sp[-2].v. Perform the binary operation x and assign the
// result to the saved address.
#define cbinop(x) \
    tp = sp[-2].a; \
    sp[-2].v = *tp; \
    binop(x); \
    *tp = sp[-1].v

L(ADDX)     cbinop(add); BREAK;
L(SUBX)     cbinop(sub); BREAK;
L(MULX)     cbinop(mul); BREAK;
L(DIVX)     cbinop(div); BREAK;
L(MODX)     cbinop(mod); BREAK;
L(CATX)     cbinop(cat); BREAK;
L(POWX)     cbinop(pow); BREAK;
L(ANDX)     cbinop(and); BREAK;
L(ORX)      cbinop(or);  BREAK;
L(SHLX)     cbinop(shl); BREAK;
L(SHRX)     cbinop(shr); BREAK;
L(XORX)     cbinop(xor); BREAK;

// Simple pop operation
L(POP)      --sp; ++ip; BREAK;

// Pop IP+1 values from stack
L(POPI)     sp -= ip[1]; ip += 2; BREAK;

// Push null literal on stack
L(NUL)      set_null(&sp++->v); ++ip; BREAK;

// Push immediate
// Assign integer value x to the top of the stack.
#define imm(x) set_int(&sp++->v, x)

L(IMM8)     imm(ip[1]);              ip += 2; BREAK;
L(IMM16)    imm((ip[1]<<8) + ip[2]); ip += 3; BREAK;
L(IMM0)     imm(0);                  ++ip;    BREAK;
L(IMM1)     imm(1);                  ++ip;    BREAK;
L(IMM2)     imm(2);                  ++ip;    BREAK;

// Push constant
// Copy constant x from code object's constant table to the top of the stack.
#define pushk(x) sp++->v = k[(x)]

L(CONST)    pushk(ip[1]); ip += 2; BREAK;
L(CONST0)   pushk(0);     ++ip;    BREAK;
L(CONST1)   pushk(1);     ++ip;    BREAK;
L(CONST2)   pushk(2);     ++ip;    BREAK;

// Push global address
// Assign the address of global variable x's rf_val in the globals table.
// The lookup will create an entry if needed, accommodating
// undeclared/uninitialized variable usage.
// Parser signals for this opcode for assignment or pre/post ++/--.
#define gbla(x) sp++->a = ht_lookup_str(&globals, k[(x)].s)

L(GBLA)     gbla(ip[1]); ip += 2; BREAK;
L(GBLA0)    gbla(0);     ++ip;    BREAK;
L(GBLA1)    gbla(1);     ++ip;    BREAK;
L(GBLA2)    gbla(2);     ++ip;    BREAK;

// Push global value
// Copy the value of global variable x to the top of the stack.
// The lookup will create an entry if needed, accommodating
// undeclared/uninitialized variable usage.
// Parser signals for this opcode to be used when only needing the value, e.g.
// arithmetic.
#define gblv(x) sp++->v = *ht_lookup_str(&globals, k[(x)].s)

L(GBLV)     gblv(ip[1]); ip += 2; BREAK;
L(GBLV0)    gblv(0);     ++ip;    BREAK;
L(GBLV1)    gblv(1);     ++ip;    BREAK;
L(GBLV2)    gblv(2);     ++ip;    BREAK;

// Push local address
// Push the address of FP[x] to the top of the stack.
#define lcla(x) sp++->a = &fp[(x)].v

L(LCLA)     lcla(ip[1]); ip += 2; BREAK;
L(LCLA0)    lcla(0);     ++ip;    BREAK;
L(LCLA1)    lcla(1);     ++ip;    BREAK;
L(LCLA2)    lcla(2);     ++ip;    BREAK;

// Push local value
// Copy the value of FP[x] to the top of the stack.
#define lclv(x) sp++->v = fp[(x)].v

L(LCLV)     lclv(ip[1]); ip += 2; BREAK;
L(LCLV0)    lclv(0);     ++ip;    BREAK;
L(LCLV1)    lclv(1);     ++ip;    BREAK;
L(LCLV2)    lclv(2);     ++ip;    BREAK;

// Tailcalls
// Recycle current call frame
L(TCALL) {
    int nargs = ip[1] + 1;
    if (!is_fn(&sp[-nargs].v))
        err("attempt to call non-function value");
    if (is_rfn(&sp[-nargs].v)) {
        sp -= nargs;
        rf_fn *fn = sp->v.fn;
        int ar1 = sp - fp - 1;  // Current frame's "arity"
        int ar2 = fn->arity;    // Callee's arity

        // Recycle call frame
        for (int i = 0; i <= ar2; ++i) {
            fp[i].v = sp[i].v;
        }

        // Increment SP without nullifying slots (preserving values) if number
        // of arguments exceeds the frame's current "arity"
        if (nargs-1 > ar1) {
            sp += (nargs - ar1 - 1);
            ar1 = nargs - 1;
        }

        // In the case of direct recursion and no call frame adjustments needed,
        // quickly reset IP and dispatch control
        if (ep == fn->code->code && ar1 == ar2) {
            ip = ep;
            BREAK;
        }

        // If callee's arity is larger than the current frame, create stack
        // space and nullify slots
        if (ar2 > ar1) {
            while (ar1++ < ar2)
                set_null(&sp++->v);
        }

        // Else, if the current frame is too large for the next call, decrement
        // SP
        else if (ar2 < ar1) {
            sp -= ar1 - ar2;
        }

        // Else else, if the size of the call frame is fine, but the user didn't
        // provide enough arguments, create stack space and nullify slots
        else if (nargs <= ar2) {
            while (nargs++ <= ar2)
                set_null(&sp++->v);
        }
        ip = ep = fn->code->code;
        k  = fn->code->k;
        BREAK;
    }

    // Fall-through to OP_CALL for C function calls
}

// Calling convention
// Arguments are pushed in-order following the rf_val containing a pointer to
// the function to be called.  Caller sets SP and FP to appropriate positions
// and cleans up stack afterward. Callee returns from exec() the number of
// values to be returned to the caller.
L(CALL) {
    int nargs = ip[1];
    if (!is_fn(&sp[-nargs-1].v))
        err("attempt to call non-function value");

    int arity, nret;

    // User-defined functions
    if (is_rfn(&sp[-nargs-1].v)) {

    rf_fn *fn = sp[-nargs-1].v.fn;
    arity = fn->arity;

    // If user called function with too few arguments, nullify stack slots and
    // increment SP.
    if (nargs < arity) {
        for (int i = nargs; i < arity; ++i) {
            set_null(&sp++->v);
        }
    }
    
    // If user called function with too many arguments, decrement SP so it
    // points to the appropriate slot for control transfer.
    else if (nargs > arity) {
        sp -= (nargs - arity);
    }

        // Pass SP-arity-1 as the FP for the succeeding call frame. Since the
        // function is already at this location in the stack, the compiler can
        // reserve the slot to accommodate any references a named function makes
        // to itself without any other work required from the VM here. This is
        // completely necessary for local named functions, but globals benefit
        // as well.
        nret = exec(fn->code->code, fn->code->k, sp, sp - arity - 1);
        sp -= arity;

        // Copy the function's return value to the stack top - this should be
        // where the caller pushed the original function.
        sp[-1].v = sp[arity].v;
    }
            
    // Built-in/C functions
    else {
        c_fn *fn = sp[-nargs-1].v.cfn;
        arity = fn->arity;

        // Most library functions are somewhat variadic; their arity refers to
        // the minimum number of arguments they require.
        if (arity && nargs < arity) {
            // If user called function with too few arguments, nullify stack
            // slots.
            for (int i = nargs; i < arity; ++i) {
                set_null(&sp[i].v);
            }
        }
        // Decrement SP to serve as the FP for the function call. Library
        // functions assign their own return values to SP-1.
        sp -= nargs;
        nret = fn->fn(&sp->v, nargs);
    }
    ip += 2;
    // Nulllify stack slot if callee returns nothing
    if (!nret) { 
        set_null(&sp[-1].v);
    }
    BREAK;
}

L(RET)      return 0;

// Caller expects return value to be at its original SP + arity of the function.
// "clean up" any created locals by copying the return value to the appropriate
// slot.
L(RET1)     retp->v = sp[-1].v;
            return 1;

// Create a sequential table of x elements from the top of the stack. Leave the
// table rf_val on the stack. Tables index at 0 by default.
#define new_tab(x) \
    tp = v_newtab(x); \
    for (int i = (x) - 1; i >= 0; --i) { \
        --sp; \
        t_insert_int(tp->t, i, &sp->v); \
    } \
    sp++->v = *tp

L(TAB0)     new_tab(0);          ++ip;    BREAK;
L(TAB)      new_tab(ip[1]);      ip += 2; BREAK;
L(TABK)     new_tab(k[ip[1]].i); ip += 2; BREAK;

L(IDXV)
    for (int i = -ip[1] - 1; i < -1; ++i) {
        if (sp[i].t <= TYPE_CFN) {
            z_idx(&sp[i].v, &sp[i+1].v);
            sp[i+1].v = sp[i].v;
            continue;
        }
        switch (sp[i].a->type) {

        // Create table if sp[-2].a is an uninitialized variable
        case TYPE_NULL:
            *sp[i+1].a = *v_newtab(0);
            // Fall-through
        case TYPE_TAB:
            sp[i+1].v = *t_lookup(sp[i].a->t, &sp[i+1].v, 0);
            break;

        // Dereference and call z_idx().
        case TYPE_INT:
        case TYPE_FLT:
        case TYPE_STR:
            sp[i].v = *sp[-i].a;
            z_idx(&sp[i].v, &sp[i+1].v);
            sp[i+1].v = sp[i].v;
            break;
        case TYPE_RFN:
        case TYPE_CFN:
            err("attempt to subscript a function");
        default:
            break;
        }
    }
    sp -= ip[1];
    sp[-1].v = sp[ip[1] - 1].v;
    ip += 2;
    BREAK;

L(IDXA)
    for (int i = -ip[1] - 1; i < -1; ++i) {
        if (sp[i].t <= TYPE_CFN)
            tp = &sp[i].v;
        else
            tp = sp[i].a;

        switch (tp->type) {

        // Create table if sp[i].a is an uninitialized variable
        case TYPE_NULL:
            *tp = *v_newtab(0);
            // Fall-through
        case TYPE_TAB:
            sp[i+1].a = t_lookup(tp->t, &sp[i+1].v, 1);
            break;

        // IDXA is invalid for all other types
        default:
            err("invalid assignment");
        }
    }
    sp -= ip[1];
    sp[-1].a = sp[ip[1] - 1].a;
    ip += 2;
    BREAK;

// IDXA
// Perform the lookup and leave the corresponding element's rf_val address on
// the stack.
L(IDXA1)

    // Accomodate OP_IDXA calls when SP-2 is a raw value
    if (sp[-2].t <= TYPE_CFN)
        tp = &sp[-2].v;
    else
        tp = sp[-2].a;

    switch (tp->type) {

    // Create table if sp[-2].a is an uninitialized variable
    case TYPE_NULL:
        *tp = *v_newtab(0);
        // Fall-through
    case TYPE_TAB:
        sp[-2].a = t_lookup(tp->t, &sp[-1].v, 1);
        break;

    // IDXA is invalid for all other types
    default:
        err("invalid assignment");
    }
    --sp;
    ++ip;
    BREAK;

// IDXV
// Perform the lookup and leave a copy of the corresponding element's value on
// the stack.
L(IDXV1)

    // All expressions e.g. x[y] are compiled to push the address of the
    // identifier being subscripted. However OP_IDXV is emitted for all
    // expressions not requiring the address of the set element to be left on
    // the stack. In the event the instruction is OP_IDXV and SP-2 contains a
    // raw value (not a pointer), the high order 64 bits will be the type tag of
    // the rf_val instead of a memory address. When that happens, defer to
    // z_idx().
    if (sp[-2].t <= TYPE_CFN) {
        binop(idx);
        BREAK;
    }

    switch (sp[-2].a->type) {

    // Create table if sp[-2].a is an uninitialized variable
    case TYPE_NULL:
        *sp[-2].a = *v_newtab(0);
        // Fall-through
    case TYPE_TAB:
        sp[-2].v = *t_lookup(sp[-2].a->t, &sp[-1].v, 0);
        --sp;
        ++ip;
        break;

    // Dereference and call z_idx().
    case TYPE_INT:
    case TYPE_FLT:
    case TYPE_STR:
        sp[-2].v = *sp[-2].a;
        binop(idx);
        break;
    case TYPE_RFN:
    case TYPE_CFN:
        err("attempt to subscript a function");
    default:
        break;
    }
    BREAK;

// Fast paths for table lookups with string literal keys
L(SIDXA)
    if (sp[-1].t <= TYPE_CFN)
        tp = &sp[-1].v;
    else
        tp = sp[-1].a;

    switch (tp->type) {

    // Create table if sp[-2].a is an uninitialized variable
    case TYPE_NULL:
        *tp = *v_newtab(0);
        // Fall-through
    case TYPE_TAB:
        sp[-1].a = ht_lookup_val(tp->t->h, &k[ip[1]]);
        break;
    default:
        err("invalid assignment");
    }
    ip += 2;
    BREAK;

L(SIDXV)
    if (sp[-1].t <= TYPE_CFN) {
        binop(idx);
        BREAK;
    }

    switch (sp[-1].a->type) {

    // Create table if sp[-2].a is an uninitialized variable
    case TYPE_NULL:
        *sp[-1].a = *v_newtab(0);
        // Fall-through
    case TYPE_TAB:
        sp[-1].v = *ht_lookup_val(sp[-1].a->t->h, &k[ip[1]]);
        ip += 2;
        break;
    default:
        err("invalid member access (non-table value)");
    }
    BREAK;

L(FLDA)     sp[-1].a = t_lookup(&fldv, &sp[-1].v, 1);
            ++ip;
            BREAK;

L(FLDV)     sp[-1].v = *t_lookup(&fldv, &sp[-1].v, 0);
            ++ip;
            BREAK;

// Create a new "range" value.
// There are 8 different valid forms of a range; each has their own instruction.
//   rng:   x..y        SP[-2]..SP[-1]
//   rngf:  x..         SP[-1]..INT_MAX
//   rngt:  ..y         0..SP[-1]
//   rnge:  ..          0..INT_MAX
//   srng:  x..y:z      SP[-3]..SP[-2]:SP[-1]
//   srngf: x..:z       SP[-2]..INT_MAX:SP[-1]
//   srngt: ..y:z       0..SP[-2]:SP[-1]
//   srnge: ..:z        0..INT_MAX:SP[-1]
// If `z` is not provided, the interval is set to -1 if x > y (downward ranges).
// Otherwise, the interval is set to 1 (upward ranges).
#define z_range(f,t,i,s) { \
    rf_rng *rng = malloc(sizeof(rf_rng)); \
    rf_int from = rng->from = (f); \
    rf_int to   = rng->to = (t); \
    rf_int itvl = (i); \
    rng->itvl   = itvl ? itvl : from > to ? -1 : 1; \
    s = (rf_val) {TYPE_RNG, .q = rng}; \
}
// x..y
L(RNG)      z_range(intval(&sp[-2].v), intval(&sp[-1].v), 0, sp[-2].v);
            --sp;
            ++ip;
            BREAK;

// x..
L(RNGF)     z_range(intval(&sp[-1].v), INT64_MAX, 0, sp[-1].v);
            ++ip;
            BREAK;

// ..y
L(RNGT)     z_range(0, intval(&sp[-1].v), 0, sp[-1].v);
            ++ip;
            BREAK;

// ..
L(RNGI)     ++sp;
            z_range(0, INT64_MAX, 0, sp[-1].v);
            ++ip;
            BREAK;

// x..y:z
L(SRNG)     z_range(intval(&sp[-3].v), intval(&sp[-2].v), intval(&sp[-1].v), sp[-3].v);
            sp -= 2;
            ++ip;
            BREAK;

// x..:z
L(SRNGF)    z_range(intval(&sp[-2].v), INT64_MAX, intval(&sp[-1].v), sp[-2].v);
            --sp;
            ++ip;
            BREAK;

// ..y:z
L(SRNGT)    z_range(0, intval(&sp[-2].v), intval(&sp[-1].v), sp[-2].v);
            --sp;
            ++ip;
            BREAK;

// ..:z
L(SRNGI)    z_range(0, INT64_MAX, intval(&sp[-1].v), sp[-1].v);
            ++ip;
            BREAK;

// Simple assignment
// copy SP[-1] to *SP[-2] and leave value on stack.
L(SET)      sp[-2].v = *sp[-2].a = sp[-1].v;
            --sp;
            ++ip;
            BREAK;

#ifndef COMPUTED_GOTO
    } }
#endif
    return 0;
}
