#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "mem.h"
#include "vm.h"

static void err(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static hash_t globals;
static rf_arr argv;
static rf_int aos;
static rf_val *stk[STACK_SIZE];    // Stack
static rf_val *res[STACK_SIZE];    // Reserve pointers

static rf_int str2int(rf_str *s) {
    char *end;
    return (rf_int) strtoull(s->str, &end, 0);
}

static rf_flt str2flt(rf_str *s) {
    char *end;
    return strtod(s->str, &end);
}

// Return logical result of value
static int test(rf_val *v) {
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
    case TYPE_FN:  return 1;
    default:       return 0;
    }
}

#define numval(x) (is_int(x) ? x->u.i : \
                   is_flt(x) ? x->u.f : \
                   is_str(x) ? str2flt(x->u.s) : 0)
#define intval(x) (is_int(x) ? x->u.i : \
                   is_flt(x) ? (rf_int) x->u.f : \
                   is_str(x) ? str2int(x->u.s) : 0)
#define fltval(x) (is_flt(x) ? x->u.f : \
                   is_int(x) ? (rf_flt) x->u.i : \
                   is_str(x) ? str2flt(x->u.s) : 0)

#define int_arith(l,r,op) \
    assign_int(l, (intval(l) op intval(r)));

#define flt_arith(l,r,op) \
    assign_flt(l, (numval(l) op numval(r)));

#define num_arith(l,r,op) \
    if (is_flt(l) || is_flt(r)) { \
        flt_arith(l,r,op); \
    } else { \
        int_arith(l,r,op); \
    }

void z_add(rf_val *l, rf_val *r) { num_arith(l,r,+); }
void z_sub(rf_val *l, rf_val *r) { num_arith(l,r,-); }
void z_mul(rf_val *l, rf_val *r) { num_arith(l,r,*); }

// Language comparison for division by zero:
// `inf`: lua, mawk
// error: pretty much all others
void z_div(rf_val *l, rf_val *r) {
    flt_arith(l,r,/);
}

// Language comparison for modulus by zero:
// `nan`: mawk
// error: pretty much all others
void z_mod(rf_val *l, rf_val *r) {
    rf_flt res = fmod(numval(l), numval(r));
    assign_flt(l, res < 0 ? res + numval(r) : res);
}

void z_pow(rf_val *l, rf_val *r) {
    assign_flt(l, pow(fltval(l), fltval(r)));
}

void z_and(rf_val *l, rf_val *r) { int_arith(l,r,&);  }
void z_or(rf_val *l, rf_val *r)  { int_arith(l,r,|);  }
void z_xor(rf_val *l, rf_val *r) { int_arith(l,r,^);  }
void z_shl(rf_val *l, rf_val *r) { int_arith(l,r,<<); }
void z_shr(rf_val *l, rf_val *r) { int_arith(l,r,>>); }

void z_num(rf_val *v) {
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

void z_neg(rf_val *v) {
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

void z_not(rf_val *v) {
    assign_int(v, ~intval(v));
}

void z_eq(rf_val *l, rf_val *r) { num_arith(l,r,==); }
void z_ne(rf_val *l, rf_val *r) { num_arith(l,r,!=); }
void z_gt(rf_val *l, rf_val *r) { num_arith(l,r,>);  }
void z_ge(rf_val *l, rf_val *r) { num_arith(l,r,>=); }
void z_lt(rf_val *l, rf_val *r) { num_arith(l,r,<);  }
void z_le(rf_val *l, rf_val *r) { num_arith(l,r,<=); }

void z_lnot(rf_val *v) {
    assign_int(v, !numval(v));
}

void z_len(rf_val *v) {
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
    case TYPE_FN:  // TODO
    default: break;
    }
    assign_int(v, l);
}

void z_test(rf_val *v) {
    assign_int(v, test(v));
}

// TODO free newly allocated strings after assigning concatenation
// result
void z_cat(rf_val *l, rf_val *r) {
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
void z_idx(rf_val *l, rf_val *r) {
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
    case TYPE_ARR: // TODO? e.g. {2,4,7,1}[0]
        break;
    default:
        assign_int(l, 0);
        break;
    }
}

// OP_PRINT functionality
static void put(rf_val *v) {
    switch (v->type) {
    case TYPE_NULL: printf("null");              break;
    case TYPE_INT:  printf("%lld", v->u.i);      break;
    case TYPE_FLT:  printf("%g", v->u.f);        break;
    case TYPE_STR:  printf("%s", v->u.s->str);   break;
    case TYPE_ARR:  printf("array: %p", v->u.a); break;
    case TYPE_FN:   printf("fn: %p", v->u.fn);   break;
    default: break;
    }
}

// TODO
#include <string.h>
static void init_argv(rf_arr *a, int argc, char **argv) {
    a_init(a);
    for (int i = 0; i < argc; ++i) {
        rf_str *s = s_newstr(argv[i], strlen(argv[i]), 1);
        a_insert_int(a, i, v_newstr(s), 1, 1);
        m_freestr(s);
    }
}

static int exec(rf_code *c, register unsigned int sp, unsigned int fp);

// VM entry point/initialization
int z_exec(rf_env *e) {
    h_init(&globals);
    init_argv(&argv, e->argc, e->argv);

    // Offset for the argv; $0 should be the first user-provided arg
    aos = e->ff ? 3 : 2;

    // Add user-defined functions to the global hash table
    for (int i = 0; i < e->nf; ++i) {
        // Don't add anonymous functions to globals (rf_str should not
        // have a computed hash)
        if (!e->fn[i]->name->hash) continue;
        rf_val *fn = malloc(sizeof(rf_val));
        *fn = (rf_val) {TYPE_FN, .u.fn = e->fn[i]};
        h_insert(&globals, e->fn[i]->name, fn, 1);
    }

    // Allocate and save pointers
    for (int i = 0; i < STACK_SIZE; i++)
        res[i] = stk[i] = malloc(sizeof(rf_val));

    return exec(e->main.code, 0, 0);
}

// VM interpreter loop
static int exec(rf_code *c,
                register unsigned int sp,
                unsigned int fp) {
    unsigned int retp = sp; // Save original SP
    rf_val *tp; // Temp pointer
    register uint8_t *ip = c->code;
    while (1) {
        if (sp >= STACK_SIZE)
            err("stack limit reached");
        switch (*ip) {

// Unconditional jumps
#define j8  (ip += (int8_t) ip[1])
#define j16 (ip += (int16_t) ((ip[1] << 8) + ip[2]))

        case OP_JMP8:  j8;  break;
        case OP_JMP16: j16; break;

// Conditional jumps (pop stack unconditionally)
#define jc8(x)  (x ? j8  : (ip += 2)); --sp;
#define jc16(x) (x ? j16 : (ip += 3)); --sp;

        case OP_JNZ8:  jc8(test(stk[sp-1]));   break;
        case OP_JNZ16: jc16(test(stk[sp-1]));  break;
        case OP_JZ8:   jc8(!test(stk[sp-1]));  break;
        case OP_JZ16:  jc16(!test(stk[sp-1])); break;

// Conditional jumps (pop stack if jump not taken)
#define xjc8(x)  if (x) j8;  else {--sp; ip += 2;}
#define xjc16(x) if (x) j16; else {--sp; ip += 3;}

        case OP_XJNZ8:  xjc8(test(stk[sp-1]));   break;
        case OP_XJNZ16: xjc16(test(stk[sp-1]));  break;
        case OP_XJZ8:   xjc8(!test(stk[sp-1]));  break;
        case OP_XJZ16:  xjc16(!test(stk[sp-1])); break;

// Unary operations
// stk[sp-1] is assumed to be safe to overwrite
#define unop(x) \
    z_##x(stk[sp-1]); \
    ++ip;

        case OP_LEN:  unop(len);  break;
        case OP_LNOT: unop(lnot); break;
        case OP_NEG:  unop(neg);  break;
        case OP_NOT:  unop(not);  break;
        case OP_NUM:  unop(num);  break;
        case OP_TEST: unop(test); break;

// Standard binary operations
// stk[sp-2] and stk[sp-1] are assumed to be safe to overwrite
#define binop(x) \
    z_##x(stk[sp-2], stk[sp-1]); \
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
// stk[sp-1] is address of some variable's rf_val.
// Increment/decrement this value directly and replace the stack
// element with a copy of the value.
#define pre(x) \
    switch (stk[sp-1]->type) { \
    case TYPE_INT: stk[sp-1]->u.i += x; break; \
    case TYPE_FLT: stk[sp-1]->u.f += x; break; \
    case TYPE_STR: \
        assign_flt(stk[sp-1], str2flt(stk[sp-1]->u.s) + x); \
        break; \
    default: \
        assign_int(stk[sp-1], x); \
        break; \
    } \
    *res[sp-1] = *stk[sp-1]; \
    stk[sp-1]  = res[sp-1]; \
    ++ip;

        case OP_PREINC: pre(1);  break;
        case OP_PREDEC: pre(-1); break;

// Post-increment/decrement
// stk[sp-1] is address of some variable's rf_val. Create a copy of
// the raw value, then increment/decrement the rf_val at the given
// address.  Replace the stack element with the previously made copy
// and coerce to a numeric value if needed.
#define post(x) \
    *res[sp-1] = *stk[sp-1]; \
    switch (stk[sp-1]->type) { \
    case TYPE_INT: stk[sp-1]->u.i += x; break; \
    case TYPE_FLT: stk[sp-1]->u.f += x; break; \
    case TYPE_STR: \
        assign_flt(stk[sp-1], str2flt(stk[sp-1]->u.s) + x); \
        break; \
    default: \
        assign_int(stk[sp-1], x); \
        break; \
    } \
    stk[sp-1] = res[sp-1]; \
    unop(num);

        case OP_POSTINC: post(1);  break;
        case OP_POSTDEC: post(-1); break;

// Compound assignment operations
// stk[sp-2] is address of some variable's rf_val. Save the address
// and replace stk[sp-2] with a copy of the value. Perform the binary
// operation x and assign the result to the saved address.
#define cbinop(x) \
    tp         = stk[sp-2]; \
    *res[sp-2] = *stk[sp-2]; \
    stk[sp-2]  = res[sp-2]; \
    binop(x); \
    *tp = *stk[sp-1];

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
        case OP_NULL: assign_null(stk[sp++]); ++ip; break;

// Push immediate
// Assign integer value x to the top of the stack.
#define imm8(x) \
    assign_int(stk[sp], x); \
    ++sp;

        case OP_IMM8: imm8(ip[1]); ip += 2; break;
        case OP_IMM0: imm8(0);     ++ip;    break;
        case OP_IMM1: imm8(1);     ++ip;    break;
        case OP_IMM2: imm8(2);     ++ip;    break;

// Push constant
// Copy constant x from code object's constant table to the top of the
// stack.
#define pushk(x) *stk[sp++] = c->k[(x)];

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
#define gbla(x) \
    stk[sp++] = h_lookup(&globals, c->k[(x)].u.s, 1);

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
#define gblv(x) \
    *stk[sp++] = *h_lookup(&globals, c->k[(x)].u.s, 0);

        case OP_GBLV:  gblv(ip[1]); ip += 2; break;
        case OP_GBLV0: gblv(0);     ++ip;    break;
        case OP_GBLV1: gblv(1);     ++ip;    break;
        case OP_GBLV2: gblv(2);     ++ip;    break;

// Nullify slot at stack[FP+x] for use as a local var
// Increment SP to reserve slot
#define lcl(x) \
    assign_null(stk[fp+(x)]); \
    ++sp;

        case OP_LCL:  lcl(ip[1]); ip += 2; break;
        case OP_LCL0: lcl(0);     ++ip;    break;
        case OP_LCL1: lcl(1);     ++ip;    break;
        case OP_LCL2: lcl(2);     ++ip;    break;

// Push local address
// Push the address of stk[FP+x] to the top of the stack.
#define lcla(x) stk[sp++] = stk[fp+(x)];

        case OP_LCLA:  lcla(ip[1]) ip += 2; break;
        case OP_LCLA0: lcla(0);    ++ip;    break;
        case OP_LCLA1: lcla(1);    ++ip;    break;
        case OP_LCLA2: lcla(2);    ++ip;    break;

// Push local value
// Copy the value of stk[FP+x] to the top of the stack.
#define lclv(x) *stk[sp++] = *stk[fp+(x)];

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
            unsigned int nargs = ip[1];
            if (!is_fn(stk[sp-(nargs+1)]))
                err("attempt to call non-function value");
            rf_fn *fn = stk[sp-(nargs+1)]->u.fn;

            // If user called function with too few arguments, nullify
            // stack elements and increment SP.
            if (nargs < fn->arity) {
                for (int i = nargs; i < fn->arity; ++i) {
                    assign_null(stk[sp++]);
                }
            }
            
            // If user called function with too many arguments,
            // decrement SP so it points to the appropriate slot for
            // control transfer.
            else if (nargs > fn->arity) {
                sp -= (nargs - fn->arity);
            }
            unsigned int nret = exec(fn->code, sp, sp - fn->arity);
            ip += 2;
            sp -= fn->arity;

            // Copy the function's return value to the stack top -
            // this should be where the caller pushed the original
            // function.
            *stk[sp-1] = *stk[sp+fn->arity];

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
            assign_null(stk[retp]);
            return 0;

        // Caller expects return value to be at its original SP +
        // arity of the function. "clean up" any created locals by
        // copying the return value to the appropriate slot.
        case OP_RET1:
            *stk[retp] = *stk[sp-1];
            return 1;
            

// Create a sequential array of x elements from the top
// of the stack. Leave the array rf_val on the stack.
// Arrays index at 0 by default.
#define new_array(x) \
    tp = v_newarr(); \
    for (int i = (x) - 1; i >= 0; --i) { \
        a_insert_int(tp->u.a, i, stk[--sp], 1, 1); \
    } \
    stk[sp++] = tp;

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
            switch (stk[sp-2]->type) {

            // Create array if stk[sp-2] is an uninitialized variable
            case TYPE_NULL:
                *stk[sp-2] = *v_newarr();
                // Fall-through
            case TYPE_ARR:
                stk[sp-2] = a_lookup(stk[sp-2]->u.a, stk[sp-1], 1, 0);
                break;

            // IDXA is invalid for all other types
            case TYPE_INT: case TYPE_FLT: case TYPE_STR:
            case TYPE_FN:
                err("idxa called with invalid type");
            default: break;
            }
            --sp;
            ++ip;
            break;

        // IDXV
        // Perform the lookup and leave a copy of the corresponding
        // element's value on the stack.
        case OP_IDXV:
            switch (stk[sp-2]->type) {

            // Create array if stk[sp-2] is an uninitialized variable
            case TYPE_NULL:
                stk[sp-2] = v_newarr();
                // Fall-through
            case TYPE_ARR:
                *res[sp-2] = *a_lookup(stk[sp-2]->u.a, stk[sp-1], 0, 0);
                stk[sp-2]  = res[sp-2];
                --sp;
                ++ip;
                break;

            // TODO parser should be signaling OP_GBLV, this handles
            // OP_GBLA usage for now
            case TYPE_INT: case TYPE_FLT: case TYPE_STR:
                *res[sp-2] = *stk[sp-2];
                stk[sp-2]  = res[sp-2];
                binop(idx);
                break;
            default:
                break;
            }
            break;

        case OP_ARGA:
            stk[sp-1] = a_lookup(&argv, stk[sp-1], 1, aos);
            ++ip;
            break;

        case OP_ARGV:
            *res[sp-1] = *a_lookup(&argv, stk[sp-1], 0, aos);
            stk[sp-1]  = res[sp-1];
            ++ip;
            break;

        case OP_SET:
            tp         = stk[sp-2];
            *res[sp-2] = *stk[sp-1];
            stk[sp-2]  = res[sp-2];
            *tp        = *res[sp-2];
            --sp;
            ++ip;
            break;

        // Print a single element from the stack
        case OP_PRINT1:
            put(stk[sp-1]);
            printf("\n");
            --sp;
            ++ip;
            break;

        // Print (IP+1) elements from the stack
        case OP_PRINT:
            for (int i = ip[1]; i > 0; --i) {
                put(stk[sp-i]);
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

#undef numval
#undef intval
#undef fltval
#undef int_arith
#undef flt_arith
#undef num_arith
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
