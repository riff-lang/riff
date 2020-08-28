#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "lib.h"
#include "mem.h"
#include "vm.h"

static void err(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static hash_t   globals;
static rf_arr   argv;
static rf_int   aos;
static rf_stack stack[STACK_SIZE];

rf_int str2int(rf_str *s) {
    char *end;
    return (rf_int) strtoull(s->str, &end, 0);
}

rf_flt str2flt(rf_str *s) {
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
    case TYPE_RFN: case TYPE_CFN: return 1;
    default:       return 0;
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
    case TYPE_INT:
        assign_int(v, intval(v));
        break;
    case TYPE_FLT:
        assign_flt(v, fltval(v));
        break;
    case TYPE_STR:
        assign_flt(v, str2flt(v->u.s));
        break;
    default:
        assign_int(v, 0);
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

static inline void z_eq(rf_val *l, rf_val *r) { num_arith(l,r,==); }
static inline void z_ne(rf_val *l, rf_val *r) { num_arith(l,r,!=); }
static inline void z_gt(rf_val *l, rf_val *r) { num_arith(l,r,>);  }
static inline void z_ge(rf_val *l, rf_val *r) { num_arith(l,r,>=); }
static inline void z_lt(rf_val *l, rf_val *r) { num_arith(l,r,<);  }
static inline void z_le(rf_val *l, rf_val *r) { num_arith(l,r,<=); }

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
    case TYPE_CFN: // TODO?
    default: break;
    }
    assign_int(v, l);
}

static inline void z_test(rf_val *v) {
    assign_int(v, test(v));
}

// TODO free newly allocated strings after assigning concatenation
// result
static inline void z_cat(rf_val *l, rf_val *r) {
    switch (l->type) {
    case TYPE_NULL: l->u.s = s_newstr(NULL, 0, 0); break;
    case TYPE_INT:  l->u.s = s_int2str(l->u.i);    break;
    case TYPE_FLT:  l->u.s = s_flt2str(l->u.f);    break;
    default: break;
    }

    switch (r->type) {
    case TYPE_NULL: r->u.s = s_newstr(NULL, 0, 0); break;
    case TYPE_INT:  r->u.s = s_int2str(r->u.i);    break;
    case TYPE_FLT:  r->u.s = s_flt2str(r->u.f);    break;
    default: break;
    }

    assign_str(l, s_concat(l->u.s, r->u.s, 0));
}

// Potentially very slow for strings; allocates 2 new string objects
// for every int or float LHS
// TODO Index out of bounds handling, e.g. RHS > length of LHS
static inline void z_idx(rf_val *l, rf_val *r) {
    switch (l->type) {
    case TYPE_INT: {
        rf_str *is = s_int2str(l->u.i);
        assign_str(l, s_newstr(&is->str[intval(r)], 1, 0));
        m_freestr(is);
        break;
    }
    case TYPE_FLT: {
        rf_str *fs = s_flt2str(l->u.f);
        assign_str(l, s_newstr(&fs->str[intval(r)], 1, 0));
        m_freestr(fs);
        break;
    }
    case TYPE_STR: {
        assign_str(l, s_newstr(&l->u.s->str[intval(r)], 1, 0));
        break;
    }
    case TYPE_ARR:
        *l = *a_lookup(l->u.a, r, 0, 0);
        break;
    case TYPE_RFN:
        assign_int(l, l->u.fn->code->code[intval(r)]);
        break;
    default:
        assign_int(l, 0);
        break;
    }
}

// OP_PRINT functionality
static inline void put(rf_val *v) {
    switch (v->type) {
    case TYPE_NULL: printf("null");              break;
    case TYPE_INT:  printf("%lld", v->u.i);      break;
    case TYPE_FLT:  printf("%g", v->u.f);        break;
    case TYPE_STR:  printf("%s", v->u.s->str);   break;
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

static int exec(rf_code *c, rf_stack *sp, rf_stack *fp);

// VM entry point/initialization
int z_exec(rf_env *e) {
    h_init(&globals);
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
        // TODO check for impending stack overflow
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

// Nullify slot at FP[x] for use as a local var
// Increment SP to reserve slot
#define lcl(x) assign_null(&fp[(x)].v); ++sp;

        case OP_LCL:  lcl(ip[1]); ip += 2; break;
        case OP_LCL0: lcl(0);     ++ip;    break;
        case OP_LCL1: lcl(1);     ++ip;    break;
        case OP_LCL2: lcl(2);     ++ip;    break;

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

                // If user called function with too few arguments, nullify
                // stack elements and increment SP.
                if (nargs < arity) {
                    for (int i = nargs; i < arity; ++i) {
                        assign_null(&sp[i].v);
                    }
                }
                
                // If user called function with too many arguments,
                // decrement SP so it points to the appropriate slot for
                // control transfer.
                else if (nargs > arity) {
                    sp -= (nargs - arity);
                }

                // Pass SP-arity-1 as the FP for the succeeding call
                // frame. Since the function is already at this
                // location in the stack, the compiler can reserve the
                // slot to accommodate any references a named function
                // makes to itself without any other work required
                // from the VM here. This is completely necessary for
                // local named functions, but globals benefit as
                // well.
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

                // Fully variadic library functions have an arity of 0
                if (arity && nargs < arity) {
                    // If user called function with too few arguments,
                    // nullify stack elements and increment SP.
                    for (int i = nargs; i < arity; ++i) {
                        assign_null(&sp[i].v);
                    }
                }
                sp -= nargs;
                nret = fn->fn(&sp->v, nargs);
            }
            ip += 2;
            // TODO - hacky
            // If callee returns 0 and the next instruction is PRINT1,
            // skip over the instruction. This facilitates user
            // functions which conditionally return something.
            if (!nret && *ip == OP_PRINT1) {
                ++ip;
                --sp;
            }
            break;
        }

        case OP_RET:
            assign_null(&retp->v);
            return 0;

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

        // IDXA
        // Perform the lookup and leave the corresponding element's
        // rf_val address on the stack.
        case OP_IDXA:
            if (sp[-2].t <= 64) {
                err("invalid assignment");
            }

            switch (sp[-2].a->type) {

            // Create array if sp[-2].a is an uninitialized variable
            case TYPE_NULL:
                *sp[-2].a = *v_newarr();
                // Fall-through
            case TYPE_ARR:
                sp[-2].a = a_lookup(sp[-2].a->u.a, &sp[-1].v, 1, 0);
                break;

            // TODO?
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
        case OP_IDXV:

            // All expressions e.g. x[y] are compiled to push the
            // address of the identifier being subscripted. However
            // OP_IDXV is emitted for all expressions not requiring
            // the address of the set element to be left on the stack.
            // In the event the instruction is OP_IDXV and SP-2
            // contains a raw value (not a pointer), the high order 64
            // bits will be the type tag of the rf_val instead of a
            // memory address. When that happens, defer to z_idx().
            if (sp[-2].t <= 64) {
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

        case OP_SET:
            sp[-2].v = *sp[-2].a = sp[-1].v;
            --sp;
            ++ip;
            break;

        // Print a single element from the stack
        case OP_PRINT1:
            put(&sp[-1].v);
            printf("\n");
            --sp;
            ++ip;
            break;

        // Print (IP+1) elements from the stack
        case OP_PRINT:
            for (int i = ip[1]; i > 0; --i) {
                put(&sp[-i].v);
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

#undef j8
#undef j16
#undef jc8
#undef jc16
#undef xjc8
#undef xjc16
#undef unop
#undef binop
#undef pre
#undef post
#undef cbinop
#undef imm8
#undef pushk
#undef gbla
#undef gblv
#undef lcl
#undef lcla
#undef lclv
#undef new_array
