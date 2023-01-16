#include "vm.h"

#include "conf.h"
#include "lib.h"
#include "mem.h"
#include "opcodes.h"
#include "string.h"
#include "util.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static inline void err(const char *msg) {
    fprintf(stderr, "riff: [vm] %s\n", msg);
    exit(1);
}

#include "ops.h"

static riff_htab  globals;
static riff_tab   argv;
static riff_tab   fldv;
static vm_iter   *iter = NULL;
static vm_stack   stack[VM_STACK_SIZE];

static inline void new_iter(riff_val *set, int kind) {
    vm_iter *new = malloc(sizeof(vm_iter));
    new->p = iter;
    iter = new;
    switch (set->type) {
    case TYPE_NULL:
        iter->t = LOOP_NULL;
        iter->n = 1;
        break;
    case TYPE_FLOAT:
        set->i = (riff_int) set->f;
        // Fall-through
    case TYPE_INT:
        iter->t = LOOP_RANGE_KV + kind;
        iter->st = 0;
        if (set->i >= 0) {
            iter->n = set->i + 1; // Inclusive
            iter->itvl = 1;
        } else {
            iter->n = -(set->i) + 1; // Inclusive
            iter->itvl = -1;
        }
        break;
    case TYPE_STR:
        iter->t = LOOP_STR_KV + kind;
        iter->n = riff_strlen(set->s);
        iter->str = set->s->str;
        break;
    case TYPE_REGEX:
        err("cannot iterate over regular expression");
    case TYPE_RANGE: {
        iter->t = LOOP_RANGE_KV + kind;
        iter->itvl = set->q->itvl;
        riff_int n = iter->itvl > 0
            ? (set->q->to - set->q->from) + 1
            : (set->q->from - set->q->to) + 1;
        iter->n = n <= 0 ? 0 : (riff_uint) ceil(fabs(n / (double) iter->itvl));
        iter->st = set->q->from;
        break;
    }
    case TYPE_TAB:
        iter->t = LOOP_TAB_KV + kind;
        iter->n = riff_tab_logical_size(set->t);
        iter->kp = iter->keys = riff_tab_collect_keys(set->t);
        iter->tab = set->t;
        break;
    case TYPE_RFN:
    case TYPE_CFN:
        err("cannot iterate over function");
    default:
        break;
    }
}

static inline void destroy_iter(void) {
    vm_iter *old = iter;
    iter = iter->p;
    if (old->t == LOOP_TAB_KV || old->t == LOOP_TAB_V) {
        free(old->keys);
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

#define add_user_funcs()                                                           \
    RIFF_VEC_FOREACH((&state->global_fn), i) {                                     \
        riff_fn *fn = RIFF_VEC_GET(&state->global_fn, i);                          \
        riff_htab_insert_str(&globals, fn->name, &(riff_val){TYPE_RFN, .fn = fn}); \
    }

static inline void register_lib(void) {
    riff_lib_register_base(&globals);
    riff_lib_register_io(&globals);
    riff_lib_register_math(&globals);
    riff_lib_register_os(&globals);
    riff_lib_register_prng(&globals);
    riff_lib_register_str(&globals);
}

// VM entry point/initialization
int riff_exec(riff_state *state) {
    riff_htab_init(&globals);
    riff_tab_init(&fldv);
    re_register_fldv(&fldv);
    init_argv(&argv, state->arg0, state->argc, state->argv);
    riff_htab_insert_cstr(&globals, "arg", &(riff_val){TYPE_TAB, .t = &argv});
    register_lib();
    // Add user-defined functions to the global hash table
    add_user_funcs();
    return exec(state->main.code.code, state->main.code.k, stack, stack);
}

// Reentry point for eval()
int riff_exec_reenter(riff_state *state, vm_stack *fp) {
    // Add user-defined functions to the global hash table
    add_user_funcs();
    return exec(state->main.code.code, state->main.code.k, fp, fp);
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
    if (riff_unlikely(sp - stack >= VM_STACK_SIZE)) {
        err("stack overflow");
    }
    vm_stack *retp = sp; // Save original SP
    riff_val *tp;        // Temp pointer
    register uint8_t *ip = ep;

#ifndef COMPUTED_GOTO
    // Use standard while loop with switch/case if computed goto is disabled or
    // unavailable
    while (1) { switch (*ip) {
#else
    static void *dispatch_labels[] = {
#define LABEL_ENUM(s,a)   &&L_##s,
        OPCODE_DEF(LABEL_ENUM)
    };
    DISPATCH();
#endif

// Unconditional jumps
#define JUMP8()  (ip +=  (int8_t)     ip[1])
#define JUMP16() (ip += *(int16_t *) &ip[1])

L(JMP):     JUMP8();  BREAK;
L(JMP16):   JUMP16(); BREAK;

// Conditional jumps (pop stack unconditionally)
#define JUMPCOND8(x)  (x ? JUMP8()  : (ip += 2)); --sp
#define JUMPCOND16(x) (x ? JUMP16() : (ip += 3)); --sp

L(JNZ):     JUMPCOND8(riff_op_test(&sp[-1].v));   BREAK;
L(JNZ16):   JUMPCOND16(riff_op_test(&sp[-1].v));  BREAK;
L(JZ):      JUMPCOND8(!riff_op_test(&sp[-1].v));  BREAK;
L(JZ16):    JUMPCOND16(!riff_op_test(&sp[-1].v)); BREAK;


// Conditional jumps (pop stack if jump not taken)
#define XJUMPCOND8(x)  if (x) JUMP8();  else {--sp; ip += 2;}
#define XJUMPCOND16(x) if (x) JUMP16(); else {--sp; ip += 3;}

L(XJNZ):    XJUMPCOND8(riff_op_test(&sp[-1].v));   BREAK;
L(XJNZ16):  XJUMPCOND16(riff_op_test(&sp[-1].v));  BREAK;
L(XJZ):     XJUMPCOND8(!riff_op_test(&sp[-1].v));  BREAK;
L(XJZ16):   XJUMPCOND16(!riff_op_test(&sp[-1].v)); BREAK;

// Initialize/cycle current iterator
L(LOOP):
L(LOOP16): {
    int jmp16 = *ip - OP_LOOP;
    if (riff_unlikely(!iter->n--)) {
        ip += 2 + jmp16;
        BREAK;
    }
    switch (iter->t) {
    case LOOP_RANGE_KV:
        if (riff_likely(is_int(iter->k)))
            ++iter->k->i;
        else
            set_int(iter->k, 0);
        // Fall-through
    case LOOP_RANGE_V:
        if (riff_likely(is_int(iter->v)))
            iter->v->i += iter->itvl;
        else
            *iter->v = (riff_val) {TYPE_INT, .i = iter->st};
        break;
    case LOOP_STR_KV:
        if (riff_likely(is_int(iter->k)))
            ++iter->k->i;
        else
            set_int(iter->k, 0);
        // Fall-through
    case LOOP_STR_V:
        if (riff_likely(is_str(iter->v)))
            iter->v->s = riff_str_new(iter->str++, 1);
        else
            *iter->v = (riff_val) {TYPE_STR, .s = riff_str_new(iter->str++, 1)};
        break;
    case LOOP_TAB_KV:
        *iter->k = *iter->kp;
        // Fall-through
    case LOOP_TAB_V:
        *iter->v = *riff_tab_lookup(iter->tab, iter->kp++);
        break;
    default:
        break;
    }

    // Treat byte(s) following OP_LOOP as unsigned since jumps are always
    // backward
    ip -= jmp16 ? *(uint16_t *) &ip[1] : ip[1];
    BREAK;
}

// Destroy the current iterator struct
L(POPL):    destroy_iter();
            ++ip;
            BREAK;

// Create iterator and jump to the corresponding OP_LOOP instruction for
// initialization
L(ITERV):
    new_iter(&sp[-1].v, 1); 
    set_null(&sp[-1].v);
    iter->v = &sp[-1].v;
    JUMP16();
    BREAK;

L(ITERKV):
    new_iter(&sp[-1].v, 0); 
    set_null(&sp[-1].v);

    // Reserve extra stack slot for k,v iterators
    set_null(&sp++->v);
    iter->k = &sp[-2].v;
    iter->v = &sp[-1].v;
    JUMP16();
    BREAK;

// Unary operations
// sp[-1].v is assumed to be safe to overwrite
#define UNARYOP(x)              \
    do {                        \
        riff_op_##x(&sp[-1].v); \
        ++ip;                   \
    } while (0)

L(LEN):     UNARYOP(len);  BREAK;
L(LNOT):    UNARYOP(lnot); BREAK;
L(NEG):     UNARYOP(neg);  BREAK;
L(NOT):     UNARYOP(not);  BREAK;
L(NUM):     UNARYOP(num);  BREAK;

// Standard binary operations
// sp[-2].v and sp[-1].v are assumed to be safe to overwrite
#define BINOP(x)                           \
    do {                                   \
        riff_op_##x(&sp[-2].v, &sp[-1].v); \
        --sp;                              \
        ++ip;                              \
    } while (0)

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

L(CATI):    riff_op_catn(sp, ip[1]);
            sp -= ip[1] - 1;
            ip += 2;
            BREAK;

L(VIDXV):   BINOP(idx);    BREAK;

// Pre-increment/decrement
// sp[-1].a is address of some variable's riff_val.
// Increment/decrement this value directly and replace the stack element with a
// copy of the value.
#define PRE(x)                                           \
    do {                                                 \
        switch (sp[-1].a->type) {                        \
        case TYPE_INT: sp[-1].a->i += x; break;          \
        case TYPE_FLOAT: sp[-1].a->f += x; break;        \
        case TYPE_STR:                                   \
            set_flt(sp[-1].a, str2flt(sp[-1].a->s) + x); \
            break;                                       \
        default:                                         \
            set_int(sp[-1].a, x);                        \
            break;                                       \
        }                                                \
        sp[-1].v = *sp[-1].a;                            \
        ++ip;                                            \
    } while (0)

L(PREINC):  PRE(1);  BREAK;
L(PREDEC):  PRE(-1); BREAK;

// Post-increment/decrement
// sp[-1].a is address of some variable's riff_val. Create a copy of the raw
// value, then increment/decrement the riff_val at the given address. Replace the
// stack element with the previously made copy and coerce to a numeric value if
// needed.
#define POST(x)                              \
    do {                                     \
        tp = sp[-1].a;                       \
        sp[-1].v = *tp;                      \
        switch (tp->type) {                  \
        case TYPE_INT: tp->i += x; break;    \
        case TYPE_FLOAT: tp->f += x; break;  \
        case TYPE_STR:                       \
            set_flt(tp, str2flt(tp->s) + x); \
            break;                           \
        default:                             \
            set_int(tp, x);                  \
            break;                           \
        }                                    \
        UNARYOP(num);                        \
    } while (0)

L(POSTINC): POST(1);  BREAK;
L(POSTDEC): POST(-1); BREAK;

// Compound assignment operations
// sp[-2].a is address of some variable's riff_val. Save the address and place a
// copy of the value in sp[-2].v. Perform the binary operation x and assign the
// result to the saved address.
#define COMPOUNDBINOP(x) \
    do {                 \
        tp = sp[-2].a;   \
        sp[-2].v = *tp;  \
        BINOP(x);        \
        *tp = sp[-1].v;  \
    } while (0)

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
L(POP):     --sp;
            ++ip;
            BREAK;

// Pop IP+1 values from stack
L(POPI):    sp -= ip[1];
            ip += 2;
            BREAK;

// Push null literal on stack
L(NULL):    set_null(&sp++->v);
            ++ip;
            BREAK;

// Push immediate
// Assign integer value x to the top of the stack.
#define PUSHIMM(x) set_int(&sp++->v, (x))

L(IMM):     PUSHIMM(ip[1]);                ip += 2; BREAK;
L(IMM16):   PUSHIMM(*(uint16_t *) &ip[1]); ip += 3; BREAK;
L(ZERO):    PUSHIMM(0);                    ++ip;    BREAK;
L(ONE):     PUSHIMM(1);                    ++ip;    BREAK;

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
// Compiler emits this opcode for assignment or pre/post ++/--.
#define PUSHGLOBALADDR(x) \
    sp++->a = riff_htab_lookup_str(&globals, k[(x)].s)

L(GBLA):    PUSHGLOBALADDR(ip[1]); ip += 2; BREAK;
L(GBLA0):   PUSHGLOBALADDR(0);     ++ip;    BREAK;
L(GBLA1):   PUSHGLOBALADDR(1);     ++ip;    BREAK;
L(GBLA2):   PUSHGLOBALADDR(2);     ++ip;    BREAK;

// Push global value
// Copy the value of global variable x to the top of the stack.
// The lookup will create an entry if needed, accommodating
// undeclared/uninitialized variable usage.
// Compiler emits this opcode when only needing the value, e.g. arithmetic.
#define PUSHGLOBALVAL(x) \
    sp++->v = *riff_htab_lookup_str(&globals, k[(x)].s)

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
    if (riff_unlikely(!is_fn(&sp[-nargs].v)))
        err("attempt to call non-function value");
    if (is_rfn(&sp[-nargs].v)) {
        sp -= nargs;
        riff_fn *fn = sp->v.fn;
        int ar1 = sp - fp - 1;  // Current frame's "arity"
        int ar2 = fn->arity;    // Callee's arity

        // Recycle call frame
        for (int i = 0; i <= ar2; ++i)
            fp[i].v = sp[i].v;

        // Increment SP without nullifying slots (preserving values) if number
        // of arguments exceeds the frame's current "arity"
        if (nargs-1 > ar1) {
            sp += (nargs - ar1 - 1);
            ar1 = nargs - 1;
        }

        // In the case of direct recursion and no call frame adjustments needed,
        // quickly reset IP and dispatch control
        if (ep == fn->code.code && ar1 == ar2) {
            ip = ep;
            BREAK;
        }

        // If callee's arity is larger than the current frame, create stack
        // space and nullify slots
        if (ar2 > ar1)
            while (ar1++ < ar2)
                set_null(&sp++->v);

        // Else, if the current frame is too large for the next call, decrement
        // SP
        else if (ar2 < ar1)
            sp -= ar1 - ar2;

        // Else else, if the size of the call frame is fine, but the user didn't
        // provide enough arguments, create stack space and nullify slots
        else if (nargs <= ar2)
            while (nargs++ <= ar2)
                set_null(&sp++->v);

        ip = ep = fn->code.code;
        k  = fn->code.k;
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
    if (riff_unlikely(!is_fn(&sp[-nargs-1].v)))
        err("attempt to call non-function value");

    int arity, nret;

    // User-defined functions
    if (is_rfn(&sp[-nargs-1].v)) {

        riff_fn *fn = sp[-nargs-1].v.fn;
        arity = fn->arity;

        // If user called function with too few arguments, nullify stack slots and
        // increment SP.
        if (nargs < arity)
            for (int i = nargs; i < arity; ++i)
                set_null(&sp++->v);
        
        // If user called function with too many arguments, decrement SP so it
        // points to the appropriate slot for control transfer.
        else if (nargs > arity)
            sp -= (nargs - arity);

        // Pass SP-arity-1 as the FP for the succeeding call frame. Since the
        // function is already at this location in the stack, the compiler can
        // reserve the slot to accommodate any references a named function makes
        // to itself without any other work required from the VM here. This is
        // completely necessary for local named functions, but globals benefit
        // as well.
        nret = exec(fn->code.code, fn->code.k, sp, sp - arity - 1);
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
        if (arity && nargs < arity)
            // If user called function with too few arguments, nullify stack
            // slots.
            for (int i = nargs; i < arity; ++i)
                set_null(&sp[i].v);

        // Decrement SP to serve as the FP for the function call. Library
        // functions assign their own return values to SP-1.
        sp -= nargs;
        nret = fn->fn(&sp->v, nargs);
    }
    ip += 2;
    // Nulllify stack slot if callee returns nothing
    if (!nret)
        set_null(&sp[-1].v);
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
#define INITTABLE(x)                               \
    do {                                           \
        tp = v_newtab(x);                          \
        for (int i = (x) - 1; i >= 0; --i) {       \
            --sp;                                  \
            riff_tab_insert_int(tp->t, i, &sp->v); \
        }                                          \
        sp++->v = *tp;                             \
    } while (0)

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
            sp[i].a->t->hint = 1;
            sp[i+1].a = riff_tab_lookup(sp[i].a->t, &sp[i+1].v);
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
    if (is_null(sp[i].a))
        *sp[i].a = *v_newtab(0);
    sp[i].v = *sp[i].a;
    for (; i < -1; ++i) {
        riff_op_idx(&sp[i].v, &sp[i+1].v);
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
        sp[-2].a->t->hint = 1;
        sp[-2].a = riff_tab_lookup(sp[-2].a->t, &sp[-1].v);
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
        sp[-2].v = *riff_tab_lookup(sp[-2].a->t, &sp[-1].v);
        --sp;
        ++ip;
        break;
    // Dereference and call riff_op_idx().
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

L(FLDA):    sp[-1].a = riff_tab_lookup(&fldv, &sp[-1].v);
            fldv.hint = 1;
            ++ip;
            BREAK;

L(FLDV):    sp[-1].v = *riff_tab_lookup(&fldv, &sp[-1].v);
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
#define PUSHRANGE(f,t,i,s)                                \
    do {                                                  \
        riff_range *r = malloc(sizeof(riff_range));       \
        riff_int from = r->from = (f);                    \
        riff_int to   = r->to = (t);                      \
        riff_int itvl = (i);                              \
        r->itvl       = itvl ? itvl : from > to ? -1 : 1; \
        s = (riff_val) {TYPE_RANGE, .q = r};              \
    } while (0)

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
