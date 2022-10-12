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

static riff_htab  globals;
static riff_tab   argv;
static riff_tab   fldv;
static vm_iter   *iter;
static vm_stack   stack[VM_STACK_SIZE];

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
static inline int test(riff_val *v) {
    switch (v->type) {
    case TYPE_INT: return !!(v->i);
    case TYPE_FLOAT: return !!(v->f);

    // If entire string is a numeric value, return logical result of the number.
    // Otherwise, return whether the string is longer than 0.
    case TYPE_STR: {
        char *end;
        riff_float f = riff_strtod(v->s->str, &end, 0);
        if (!*end) {
            // Check for literal '0' character in string
            if (f == 0.0 && riff_str_haszero(v->s))
                return 0;
            else
                return !!f;
        }
        return !!riff_strlen(v->s);
    }
    case TYPE_TAB: return !!riff_tab_logical_size(v->t);
    case TYPE_REGEX:
    case TYPE_RANGE:
    case TYPE_RFN:
    case TYPE_CFN:
        return 1;
    default: return 0;
    }
}

#define VM_UOP(op)   static inline void vm_##op(riff_val *v)
#define VM_BINOP(op) static inline void vm_##op(riff_val *l, riff_val *r)

VM_BINOP(add) { num_arith(l,r,+); }
VM_BINOP(sub) { num_arith(l,r,-); }
VM_BINOP(mul) { num_arith(l,r,*); }

// Language comparison for division by zero:
// 0/0 = nan; 1/0 = inf : lua, mawk
// error: pretty much all others
VM_BINOP(div) { flt_arith(l,r,/); }

// Language comparison for modulus by zero:
// `nan`: mawk
// error: pretty much all others
VM_BINOP(mod) {
    riff_float res = fmod(numval(l), numval(r));
    set_flt(l, res < 0 ? res + numval(r) : res);
}

VM_BINOP(pow) { set_flt(l, pow(fltval(l), fltval(r))); }

VM_BINOP(and) { int_arith(l,r,&);  }
VM_BINOP(or)  { int_arith(l,r,|);  }
VM_BINOP(xor) { int_arith(l,r,^);  }
VM_BINOP(shl) { int_arith(l,r,<<); }
VM_BINOP(shr) { int_arith(l,r,>>); }

VM_UOP(num) {
    switch (v->type) {
    case TYPE_INT:
    case TYPE_FLOAT:
        break;
    case TYPE_STR:
        set_flt(v, str2flt(v->s));
        break;
    default:
        set_int(v, 0);
        break;
    }
}

VM_UOP(neg) {
    switch (v->type) {
    case TYPE_INT:   v->i = -v->i;               break;
    case TYPE_FLOAT: v->f = -v->f;               break;
    case TYPE_STR:   set_flt(v, -str2flt(v->s)); break;
    default:         set_int(v, 0);              break;
    }
}

VM_UOP(not) { set_int(v, ~intval(v)); }

// == and != operators
#define cmp_eq(l,r,op) \
    if (is_int(l) && is_int(r)) { \
        l->i = (l->i op r->i); \
    } else if (is_null(l) ^ is_null(r)) { \
        set_int(l, !(0 op 0)); \
    } else if (is_str(l) && is_str(r)) { \
        set_int(l, ((l->s) op (r->s))); \
    } else if (is_str(l) && !is_str(r)) { \
        if (!riff_strlen(l->s)) { \
            set_int(l, !(0 op 0)); \
            return; \
        } \
        char *end; \
        riff_float f = riff_strtod(l->s->str, &end, 0); \
        if (*end) { \
            set_int(l, 0); \
        } else { \
            set_int(l, (f op numval(r))); \
        } \
    } else if (!is_str(l) && is_str(r)) { \
        if (!riff_strlen(r->s)) { \
            set_int(l, !(0 op 0)); \
            return; \
        } \
        char *end; \
        riff_float f = riff_strtod(r->s->str, &end, 0); \
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

VM_BINOP(eq) { cmp_eq(l,r,==);  }
VM_BINOP(ne) { cmp_eq(l,r,!=);  }
VM_BINOP(gt) { cmp_rel(l,r,>);  }
VM_BINOP(ge) { cmp_rel(l,r,>=); }
VM_BINOP(lt) { cmp_rel(l,r,<);  }
VM_BINOP(le) { cmp_rel(l,r,<=); }

VM_UOP(lnot) { set_int(v, !test(v)); }

VM_UOP(len) {
    riff_int l = 0;
    switch (v->type) {

    // For integers:
    //   #x = ⌊log10(x)⌋  + 1 for x > 0
    //        ⌊log10(-x)⌋ + 2 for x < 0
    case TYPE_INT:
        if (v->i == INT64_MIN) {
            l = 20;
        } else {
            l = v->i > 0 ? (riff_int) log10(v->i)  + 1 :
                v->i < 0 ? (riff_int) log10(-v->i) + 2 : 1;
        }
        v->i = l;
        return;
    case TYPE_FLOAT:
        l = (riff_int) snprintf(NULL, 0, "%g", v->f);
        break;
    case TYPE_STR: l = riff_strlen(v->s); break;
    case TYPE_TAB: l = riff_tab_logical_size(v->t); break;
    case TYPE_REGEX:   // TODO - extract something from PCRE pattern?
    case TYPE_RANGE:  // TODO
    case TYPE_RFN:
    case TYPE_CFN:
        l = 1;
        break;
    default: break;
    }
    set_int(v, l);
}

VM_BINOP(cat) {
    char lbuf[STR_BUF_SZ];
    char rbuf[STR_BUF_SZ];
    char *ls = lbuf, *rs = rbuf;
    size_t llen = riff_tostr(l, &ls);
    size_t rlen = riff_tostr(r, &rs);
    set_str(l, riff_strcat(ls, rs, llen, rlen));
}

static inline void vm_catn(vm_stack *fp, int n) {
    char buf[STR_BUF_SZ];
    char tbuf[STR_BUF_SZ];
    size_t len = 0;
    for (int i = -n; i <= -1; ++i) {
        char *p = tbuf;
        size_t tlen = riff_tostr(&fp[i].v, &p);
        memcpy(buf + len, p, tlen);
        len += tlen;
    }
    set_str(&fp[-n].v, riff_str_new(buf, len));
}

static inline riff_int match(riff_val *l, riff_val *r) {
    // Common case: LHS string, RHS regex
    if (LIKELY(is_str(l) && is_regex(r)))
        return re_match(l->s->str, riff_strlen(l->s), r->r, 1);
    char *lhs;
    size_t len = 0;
    char temp_lhs[32];
    char temp_rhs[32];
    if (!is_str(l)) {
        switch (l->type) {
        case TYPE_INT:   len = riff_lltostr(l->i, temp_lhs); break;
        case TYPE_FLOAT: len = riff_dtostr(l->f, temp_lhs); break;
        default:         temp_lhs[0] = '\0';              break;
        }
        lhs = temp_lhs;
    } else {
        lhs = l->s->str;
        len = riff_strlen(l->s);
    }
    if (!is_regex(r)) {
        riff_regex *temp_re;
        riff_int res;
        int errcode;
        int capture = 0;
        switch (r->type) {
        case TYPE_INT:   riff_lltostr(r->i, temp_rhs); break;
        case TYPE_FLOAT: riff_dtostr(r->f, temp_rhs); break;
        case TYPE_STR:
            capture = 1;
            temp_re = re_compile(r->s->str, riff_strlen(r->s), 0, &errcode);
            goto do_match;
        default: temp_rhs[0] = '\0'; break;
        }
        temp_re = re_compile(temp_rhs, PCRE2_ZERO_TERMINATED, 0, &errcode);
do_match:
        // Check for invalid regex in RHS
        // TODO treat as literal string in this case? (PCRE2_LITERAL)
        if (UNLIKELY(errcode != 100)) {
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

VM_BINOP(match)  { set_int(l,  match(l, r)); }
VM_BINOP(nmatch) { set_int(l, !match(l, r)); }

VM_BINOP(idx) {
    switch (l->type) {
    case TYPE_INT:
    case TYPE_FLOAT: {
        char temp[32];
        char *p = temp;
        riff_int len = (riff_int) riff_tostr(l, &p);
        if (is_range(r)) {
            set_str(l, riff_substr(temp, r->q->from, r->q->to, r->q->itvl));
        } else {
            riff_int r1  = intval(r);
            if (r1 < 0)
                r1 += len;
            if (r1 > len - 1 || r1 < 0)
                set_null(l);
            else
                set_str(l, riff_str_new(temp + r1, 1));
        }
        break;
    }
    case TYPE_STR: {
        if (is_range(r)) {
            l->s = riff_substr(l->s->str, r->q->from, r->q->to, r->q->itvl);
        } else {
            riff_int r1  = intval(r);
            riff_int len = (riff_int) riff_strlen(l->s);
            if (r1 < 0)
                r1 += len;
            if (r1 > len - 1 || r1 < 0)
                set_null(l);
            else
                l->s = riff_str_new(&l->s->str[r1], 1);
        }
        break;
    }
    case TYPE_NULL:
        *l = *v_newtab(0);
        // Fall-through
    case TYPE_TAB:
        *l = *riff_tab_lookup(l->t, r, 0);
        break;
    default:
        set_null(l);
        break;
    }
}

static inline void new_iter(riff_val *set) {
    vm_iter *new = malloc(sizeof(vm_iter));
    new->p = iter;
    iter = new;
    switch (set->type) {
    case TYPE_NULL:
        iter->t = LOOP_NULL;
        iter->keys = NULL;
        iter->n = 1;
        break;
    case TYPE_FLOAT:
        set->i = (riff_int) set->f;
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
        iter->n = riff_strlen(set->s);
        iter->keys = NULL;
        iter->set.str = set->s->str;
        break;
    case TYPE_REGEX:
        err("cannot iterate over regular expression");
    case TYPE_RANGE:
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
            iter->n = (riff_uint) ceil(fabs(iter->on / (double) iter->set.itvl));
        iter->st = set->q->from;
        break;
    case TYPE_TAB:
        iter->t = LOOP_TAB;
        iter->on = iter->n = riff_tab_logical_size(set->t);
        iter->keys = riff_tab_collect_keys(set->t);
        iter->set.tab = set->t;
        break;
    case TYPE_RFN:
    case TYPE_CFN:
        err("cannot iterate over function");
    default: break;
    }
}

static inline void destroy_iter(void) {
    vm_iter *old = iter;
    iter = iter->p;
    if (old->t == LOOP_TAB) {
        if (!(old->n + 1)) // Loop completed?
            free(old->keys - old->on);
        else
            free(old->keys - (old->on - old->n));
    }
    free(old);
}

static inline void init_argv(riff_tab *t, riff_int arg0, int rf_argc, char **rf_argv) {
    riff_tab_init(t);
    for (riff_int i = 0; i < rf_argc; ++i) {
        riff_val v = (riff_val) {
            TYPE_STR,
            .s = riff_str_new(rf_argv[i], strlen(rf_argv[i]))
        };
        riff_int idx = i - arg0;
        if (idx < 0)
            riff_htab_insert_val(t->h, &(riff_val){TYPE_INT, .i = idx}, &v);
        else
            riff_tab_insert_int(t, idx, &v);
    }
}

static inline int exec(uint8_t *, riff_val *, vm_stack *, vm_stack *);

#define add_user_funcs() \
    for (int i = 0; i < e->nf; ++i) { \
        if (e->fn[i]->name->hash) { \
            riff_htab_insert_str(&globals, e->fn[i]->name, &(riff_val){TYPE_RFN, .fn = e->fn[i]}); \
        } \
    }

// VM entry point/initialization
int vm_exec(riff_state *e) {
    riff_htab_init(&globals);
    iter = NULL;
    riff_tab_init(&fldv);
    re_register_fldv(&fldv);
    init_argv(&argv, e->arg0, e->argc, e->argv);
    riff_htab_insert_cstr(&globals, "arg", &(riff_val){TYPE_TAB, .t = &argv});
    l_register_builtins(&globals);
    // Add user-defined functions to the global hash table
    add_user_funcs();
    return exec(e->main.code->code, e->main.code->k, stack, stack);
}

// Reentry point for eval()
int vm_exec_reenter(riff_state *e, vm_stack *fp) {
    // Add user-defined functions to the global hash table
    add_user_funcs();
    return exec(e->main.code->code, e->main.code->k, fp, fp);
}

#ifndef COMPUTED_GOTO
#ifdef __GNUC__
#define COMPUTED_GOTO
#endif
#endif

#ifndef COMPUTED_GOTO
#define L(l)       case OP_##l
#define BREAK      break
#else
#define L(l)       L_##l
#define BREAK      DISPATCH()
#define DISPATCH() goto *dispatch_labels[*ip]
#endif

// VM interpreter loop
static inline int exec(uint8_t *ep, riff_val *k, vm_stack *sp, vm_stack *fp) {
    if (UNLIKELY(sp - stack >= VM_STACK_SIZE)) {
        err("stack overflow");
    }
    vm_stack *retp = sp; // Save original SP
    riff_val   *tp;      // Temp pointer
    register uint8_t *ip = ep;

#ifndef COMPUTED_GOTO
    // Use standard while loop with switch/case if computed goto is disabled or
    // unavailable
    while (1) { switch (*ip) {
#else
    static void *dispatch_labels[] = {
#define OPCODE(x,y,z) &&L_##x
#include "opcodes.h"
    };
    DISPATCH();
#endif

// Unconditional jumps
#define JUMP8()  (ip +=  (int8_t)     ip[1])
#define JUMP16() (ip += *(int16_t *) &ip[1])

L(JMP8):    JUMP8();  BREAK;
L(JMP16):   JUMP16(); BREAK;

// Conditional jumps (pop stack unconditionally)
#define JUMPCOND8(x)  (x ? JUMP8()  : (ip += 2)); --sp
#define JUMPCOND16(x) (x ? JUMP16() : (ip += 3)); --sp

L(JNZ8):    JUMPCOND8(test(&sp[-1].v));   BREAK;
L(JNZ16):   JUMPCOND16(test(&sp[-1].v));  BREAK;
L(JZ8):     JUMPCOND8(!test(&sp[-1].v));  BREAK;
L(JZ16):    JUMPCOND16(!test(&sp[-1].v)); BREAK;


// Conditional jumps (pop stack if jump not taken)
#define XJUMPCOND8(x)  if (x) JUMP8();  else {--sp; ip += 2;}
#define XJUMPCOND16(x) if (x) JUMP16(); else {--sp; ip += 3;}

L(XJNZ8):   XJUMPCOND8(test(&sp[-1].v));   BREAK;
L(XJNZ16):  XJUMPCOND16(test(&sp[-1].v));  BREAK;
L(XJZ8):    XJUMPCOND8(!test(&sp[-1].v));  BREAK;
L(XJZ16):   XJUMPCOND16(!test(&sp[-1].v)); BREAK;

// Initialize/cycle current iterator
L(LOOP8):
L(LOOP16):{
    int jmp16 = *ip == OP_LOOP16;
    if (UNLIKELY(!iter->n--)) {
        if (jmp16)
            ip += 3;
        else
            ip += 2;
        BREAK;
    }
    switch (iter->t) {
    case LOOP_RNG:
        if (UNLIKELY(is_null(iter->v)))
            *iter->v = (riff_val) {TYPE_INT, .i = iter->st};
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
            iter->v->s = riff_str_new(iter->set.str++, 1);
        } else {
            *iter->v = (riff_val) {TYPE_STR, .s = riff_str_new(iter->set.str++, 1)};
        }
        break;
    case LOOP_TAB:
        if (iter->k != NULL) {
            *iter->k = *iter->keys;
        }
        *iter->v = *riff_tab_lookup(iter->set.tab, iter->keys, 0);
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
    if (jmp16) {
        ip -= *(uint16_t *) &ip[1];
    } else {
        ip -= ip[1];
    }
    BREAK;
}

// Destroy the current iterator struct
L(POPL):    destroy_iter();
            ++ip;
            BREAK;

// Create iterator and jump to the corresponding OP_LOOP instruction for
// initialization
L(ITERV):
    new_iter(&sp[-1].v); 
    --sp;
    set_null(&sp++->v);
    iter->k = NULL;
    iter->v = &sp[-1].v;
    JUMP16();
    BREAK;

L(ITERKV):
    new_iter(&sp[-1].v); 
    --sp;
    set_null(&sp++->v);

    // Reserve extra stack slot for k,v iterators
    set_null(&sp++->v);
    iter->k = &sp[-2].v;
    iter->v = &sp[-1].v;
    JUMP16();
    BREAK;

// Unary operations
// sp[-1].v is assumed to be safe to overwrite
#define UNARYOP(x) vm_##x(&sp[-1].v); ++ip

L(LEN):     UNARYOP(len);  BREAK;
L(LNOT):    UNARYOP(lnot); BREAK;
L(NEG):     UNARYOP(neg);  BREAK;
L(NOT):     UNARYOP(not);  BREAK;
L(NUM):     UNARYOP(num);  BREAK;

// Standard binary operations
// sp[-2].v and sp[-1].v are assumed to be safe to overwrite
#define BINOP(x) vm_##x(&sp[-2].v, &sp[-1].v); --sp; ++ip

L(ADD):     BINOP(add);    BREAK;
L(SUB):     BINOP(sub);    BREAK;
L(MUL):     BINOP(mul);    BREAK;
L(DIV):     BINOP(div);    BREAK;
L(MOD):     BINOP(mod);    BREAK;
L(POW):     BINOP(pow);    BREAK;
L(AND):     BINOP(and);    BREAK;
L(OR):      BINOP(or);     BREAK;
L(XOR):     BINOP(xor);    BREAK;
L(SHL):     BINOP(shl);    BREAK;
L(SHR):     BINOP(shr);    BREAK;
L(EQ):      BINOP(eq);     BREAK;
L(NE):      BINOP(ne);     BREAK;
L(GT):      BINOP(gt);     BREAK;
L(GE):      BINOP(ge);     BREAK;
L(LT):      BINOP(lt);     BREAK;
L(LE):      BINOP(le);     BREAK;
L(MATCH):   BINOP(match);  BREAK;
L(NMATCH):  BINOP(nmatch); BREAK;
L(CAT):     BINOP(cat);    BREAK;

L(CATI):    vm_catn(sp, ip[1]);
            sp -= ip[1] - 1;
            ip += 2;
            BREAK;

L(VIDXV):   BINOP(idx);    BREAK;

// Pre-increment/decrement
// sp[-1].a is address of some variable's riff_val.
// Increment/decrement this value directly and replace the stack element with a
// copy of the value.
#define PRE(x) \
    switch (sp[-1].a->type) { \
    case TYPE_INT: sp[-1].a->i += x; break; \
    case TYPE_FLOAT: sp[-1].a->f += x; break; \
    case TYPE_STR: \
        set_flt(sp[-1].a, str2flt(sp[-1].a->s) + x); \
        break; \
    default: \
        set_int(sp[-1].a, x); \
        break; \
    } \
    sp[-1].v = *sp[-1].a; \
    ++ip

L(PREINC):  PRE(1);  BREAK;
L(PREDEC):  PRE(-1); BREAK;

// Post-increment/decrement
// sp[-1].a is address of some variable's riff_val. Create a copy of the raw
// value, then increment/decrement the riff_val at the given address. Replace the
// stack element with the previously made copy and coerce to a numeric value if
// needed.
#define POST(x) \
    tp = sp[-1].a; \
    sp[-1].v = *tp; \
    switch (tp->type) { \
    case TYPE_INT: tp->i += x; break; \
    case TYPE_FLOAT: tp->f += x; break; \
    case TYPE_STR: \
        set_flt(tp, str2flt(tp->s) + x); \
        break; \
    default: \
        set_int(tp, x); \
        break; \
    } \
    UNARYOP(num)

L(POSTINC): POST(1);  BREAK;
L(POSTDEC): POST(-1); BREAK;

// Compound assignment operations
// sp[-2].a is address of some variable's riff_val. Save the address and place a
// copy of the value in sp[-2].v. Perform the binary operation x and assign the
// result to the saved address.
#define COMPOUNDBINOP(x) \
    tp = sp[-2].a; \
    sp[-2].v = *tp; \
    BINOP(x); \
    *tp = sp[-1].v

L(ADDX):    COMPOUNDBINOP(add); BREAK;
L(SUBX):    COMPOUNDBINOP(sub); BREAK;
L(MULX):    COMPOUNDBINOP(mul); BREAK;
L(DIVX):    COMPOUNDBINOP(div); BREAK;
L(MODX):    COMPOUNDBINOP(mod); BREAK;
L(CATX):    COMPOUNDBINOP(cat); BREAK;
L(POWX):    COMPOUNDBINOP(pow); BREAK;
L(ANDX):    COMPOUNDBINOP(and); BREAK;
L(ORX):     COMPOUNDBINOP(or);  BREAK;
L(SHLX):    COMPOUNDBINOP(shl); BREAK;
L(SHRX):    COMPOUNDBINOP(shr); BREAK;
L(XORX):    COMPOUNDBINOP(xor); BREAK;

// Simple pop operation
L(POP):     --sp; ++ip; BREAK;

// Pop IP+1 values from stack
L(POPI):    sp -= ip[1]; ip += 2; BREAK;

// Push null literal on stack
L(NUL):     set_null(&sp++->v); ++ip; BREAK;

// Push immediate
// Assign integer value x to the top of the stack.
#define PUSHIMM(x) set_int(&sp++->v, (x))

L(IMM8):    PUSHIMM(ip[1]);                ip += 2; BREAK;
L(IMM16):   PUSHIMM(*(uint16_t *) &ip[1]); ip += 3; BREAK;
L(IMM0):    PUSHIMM(0);                    ++ip;    BREAK;
L(IMM1):    PUSHIMM(1);                    ++ip;    BREAK;
L(IMM2):    PUSHIMM(2);                    ++ip;    BREAK;

// Push constant
// Copy constant x from code object's constant table to the top of the stack.
#define PUSHCONST(x) sp++->v = k[(x)]

L(CONST):   PUSHCONST(ip[1]); ip += 2; BREAK;
L(CONST0):  PUSHCONST(0);     ++ip;    BREAK;
L(CONST1):  PUSHCONST(1);     ++ip;    BREAK;
L(CONST2):  PUSHCONST(2);     ++ip;    BREAK;

// Push global address
// Assign the address of global variable x's riff_val in the globals table.
// The lookup will create an entry if needed, accommodating
// undeclared/uninitialized variable usage.
// Parser signals for this opcode for assignment or pre/post ++/--.
#define PUSHGLOBALADDR(x) sp++->a = riff_htab_lookup_str(&globals, k[(x)].s)

L(GBLA):    PUSHGLOBALADDR(ip[1]); ip += 2; BREAK;
L(GBLA0):   PUSHGLOBALADDR(0);     ++ip;    BREAK;
L(GBLA1):   PUSHGLOBALADDR(1);     ++ip;    BREAK;
L(GBLA2):   PUSHGLOBALADDR(2);     ++ip;    BREAK;

// Push global value
// Copy the value of global variable x to the top of the stack.
// The lookup will create an entry if needed, accommodating
// undeclared/uninitialized variable usage.
// Parser signals for this opcode to be used when only needing the value, e.g.
// arithmetic.
#define PUSHGLOBALVAL(x) sp++->v = *riff_htab_lookup_str(&globals, k[(x)].s)

L(GBLV):    PUSHGLOBALVAL(ip[1]); ip += 2; BREAK;
L(GBLV0):   PUSHGLOBALVAL(0);     ++ip;    BREAK;
L(GBLV1):   PUSHGLOBALVAL(1);     ++ip;    BREAK;
L(GBLV2):   PUSHGLOBALVAL(2);     ++ip;    BREAK;

// Push local address
// Push the address of FP[x] to the top of the stack.
#define PUSHLOCALADDR(x) sp++->a = &fp[(x)].v

L(LCLA):    PUSHLOCALADDR(ip[1]); ip += 2; BREAK;
L(LCLA0):   PUSHLOCALADDR(0);     ++ip;    BREAK;
L(LCLA1):   PUSHLOCALADDR(1);     ++ip;    BREAK;
L(LCLA2):   PUSHLOCALADDR(2);     ++ip;    BREAK;

L(DUPA):    set_null(&sp->v);
            sp[1].a = &sp->v;
            sp += 2;
            ++ip;
            BREAK;

// Push local value
// Copy the value of FP[x] to the top of the stack.
#define PUSHLOCALVAL(x) sp++->v = fp[(x)].v

L(LCLV):    PUSHLOCALVAL(ip[1]); ip += 2; BREAK;
L(LCLV0):   PUSHLOCALVAL(0);     ++ip;    BREAK;
L(LCLV1):   PUSHLOCALVAL(1);     ++ip;    BREAK;
L(LCLV2):   PUSHLOCALVAL(2);     ++ip;    BREAK;

// Tailcalls
// Recycle current call frame
L(TCALL): {
    int nargs = ip[1] + 1;
    if (UNLIKELY(!is_fn(&sp[-nargs].v)))
        err("attempt to call non-function value");
    if (is_rfn(&sp[-nargs].v)) {
        sp -= nargs;
        riff_fn *fn = sp->v.fn;
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
// Arguments are pushed in-order following the riff_val containing a pointer to
// the function to be called.  Caller sets SP and FP to appropriate positions
// and cleans up stack afterward. Callee returns from exec() the number of
// values to be returned to the caller.
L(CALL): {
    int nargs = ip[1];
    if (UNLIKELY(!is_fn(&sp[-nargs-1].v)))
        err("attempt to call non-function value");

    int arity, nret;

    // User-defined functions
    if (is_rfn(&sp[-nargs-1].v)) {

        riff_fn *fn = sp[-nargs-1].v.fn;
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
        riff_cfn *fn = sp[-nargs-1].v.cfn;
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

L(RET):     return 0;

// Caller expects return value to be at its original SP + arity of the function.
// "clean up" any created locals by copying the return value to the appropriate
// slot.
L(RET1):    retp->v = sp[-1].v;
            return 1;

// Create a sequential table of x elements from the top of the stack. Leave the
// table riff_val on the stack. Tables index at 0 by default.
#define INITTABLE(x) \
    tp = v_newtab(x); \
    for (int i = (x) - 1; i >= 0; --i) { \
        --sp; \
        riff_tab_insert_int(tp->t, i, &sp->v); \
    } \
    sp++->v = *tp

L(TAB0):    INITTABLE(0);          ++ip;    BREAK;
L(TAB):     INITTABLE(ip[1]);      ip += 2; BREAK;
L(TABK):    INITTABLE(k[ip[1]].i); ip += 2; BREAK;


L(IDXA): {
    for (int i = -ip[1] - 1; i < -1; ++i) {
        switch (sp[i].a->type) {
        // Create table if sp[i].a is an uninitialized variable
        case TYPE_NULL:
            *sp[i].a = *v_newtab(0);
            // Fall-through
        case TYPE_TAB:
            sp[i+1].a = riff_tab_lookup(sp[i].a->t, &sp[i+1].v, 1);
            break;
        // IDXA is invalid for all other types
        default:
            err("invalid assignment");
        }
    }
    sp -= ip[1];
    sp[-1].a = sp[ip[1]-1].a;
    ip += 2;
    BREAK;
}

L(IDXV): {
    int i = -ip[1] - 1;
    if (is_null(sp[i].a)) {
        *sp[i].a = *v_newtab(0);
    }
    sp[i].v = *sp[i].a;
    for (; i < -1; ++i) {
        vm_idx(&sp[i].v, &sp[i+1].v);
        sp[i+1].v = sp[i].v;
    }
    sp -= ip[1];
    sp[-1].v = sp[ip[1]-1].v;
    ip += 2;
    BREAK;
}

// IDXA
// Perform the lookup and leave the corresponding element's riff_val address on
// the stack.
L(IDXA1):
    switch (sp[-2].a->type) {
    // Create table if sp[-2].a is an uninitialized variable
    case TYPE_NULL:
        *sp[-2].a = *v_newtab(0);
        // Fall-through
    case TYPE_TAB:
        sp[-2].a = riff_tab_lookup(sp[-2].a->t, &sp[-1].v, 1);
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
L(IDXV1):
    switch (sp[-2].a->type) {
    // Create table if sp[-2].a is an uninitialized variable
    case TYPE_NULL:
        *sp[-2].a = *v_newtab(0);
        // Fall-through
    case TYPE_TAB:
        sp[-2].v = *riff_tab_lookup(sp[-2].a->t, &sp[-1].v, 0);
        --sp;
        ++ip;
        break;
    // Dereference and call vm_idx().
    case TYPE_INT:
    case TYPE_FLOAT:
    case TYPE_STR:
        sp[-2].v = *sp[-2].a;
        BINOP(idx);
        break;
    case TYPE_RFN:
    case TYPE_CFN:
        err("invalid function subscript");
    default:
        break;
    }
    BREAK;

// Fast paths for table lookups with string literal keys
L(SIDXA):
    switch (sp[-1].a->type) {
    // Create table if sp[-1].a is an uninitialized variable
    case TYPE_NULL:
        *sp[-1].a = *v_newtab(0);
        // Fall-through
    case TYPE_TAB:
        sp[-1].a = riff_htab_lookup_val(sp[-1].a->t->h, &k[ip[1]]);
        break;
    default:
        err("invalid member access (non-table value)");
    }
    ip += 2;
    BREAK;

L(SIDXV):
    switch (sp[-1].a->type) {
    // Create table if sp[-1].a is an uninitialized variable
    case TYPE_NULL:
        *sp[-1].a = *v_newtab(0);
        // Fall-through
    case TYPE_TAB:
        sp[-1].v = *riff_htab_lookup_val(sp[-1].a->t->h, &k[ip[1]]);
        break;
    default:
        err("invalid member access (non-table value)");
    }
    ip += 2;
    BREAK;

L(FLDA):    sp[-1].a = riff_tab_lookup(&fldv, &sp[-1].v, 1);
            ++ip;
            BREAK;

L(FLDV):    sp[-1].v = *riff_tab_lookup(&fldv, &sp[-1].v, 0);
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
#define PUSHRANGE(f,t,i,s) { \
    riff_range *r = malloc(sizeof(riff_range)); \
    riff_int from = r->from = (f); \
    riff_int to   = r->to = (t); \
    riff_int itvl = (i); \
    r->itvl       = itvl ? itvl : from > to ? -1 : 1; \
    s = (riff_val) {TYPE_RANGE, .q = r}; \
}

// x..y
L(RNG):     PUSHRANGE(intval(&sp[-2].v), intval(&sp[-1].v), 0, sp[-2].v);
            --sp;
            ++ip;
            BREAK;

// x..
L(RNGF):    PUSHRANGE(intval(&sp[-1].v), INT64_MAX, 0, sp[-1].v);
            ++ip;
            BREAK;

// ..y
L(RNGT):    PUSHRANGE(0, intval(&sp[-1].v), 0, sp[-1].v);
            ++ip;
            BREAK;

// ..
L(RNGI):    ++sp;
            PUSHRANGE(0, INT64_MAX, 0, sp[-1].v);
            ++ip;
            BREAK;

// x..y:z
L(SRNG):    PUSHRANGE(intval(&sp[-3].v), intval(&sp[-2].v), intval(&sp[-1].v), sp[-3].v);
            sp -= 2;
            ++ip;
            BREAK;

// x..:z
L(SRNGF):   PUSHRANGE(intval(&sp[-2].v), INT64_MAX, intval(&sp[-1].v), sp[-2].v);
            --sp;
            ++ip;
            BREAK;

// ..y:z
L(SRNGT):   PUSHRANGE(0, intval(&sp[-2].v), intval(&sp[-1].v), sp[-2].v);
            --sp;
            ++ip;
            BREAK;

// ..:z
L(SRNGI):   PUSHRANGE(0, INT64_MAX, intval(&sp[-1].v), sp[-1].v);
            ++ip;
            BREAK;

// Simple assignment
// copy SP[-1] to *SP[-2] and leave value on stack.
L(SET):     sp[-2].v = *sp[-2].a = sp[-1].v;
            --sp;
            ++ip;
            BREAK;

// Set and pop
L(SETP):    *sp[-2].a = sp[-1].v;
            sp -= 2;
            ++ip;
            BREAK;

#ifndef COMPUTED_GOTO
    }}
#endif
    return 0;
}
