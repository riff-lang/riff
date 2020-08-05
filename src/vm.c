#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "array.h"
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
    case TYPE_ARR: return !!(v->u.a->h->n); // TODO
    case TYPE_FN:  return 1;
    default:       return 0;
    }
}

static int_t str2int(str_t *s) {
    char *end;
    return (int_t) strtoull(s->str, &end, 0);
}

static flt_t str2flt(str_t *s) {
    char *end;
    return strtod(s->str, &end);
}

#define numval(x) (is_int(x) ? x->u.i : \
                   is_flt(x) ? x->u.f : \
                   is_str(x) ? str2flt(x->u.s) : 0)
#define intval(x) (is_int(x) ? x->u.i : \
                   is_flt(x) ? (int_t) x->u.f : \
                   is_str(x) ? str2int(x->u.s) : 0)
#define fltval(x) (is_flt(x) ? x->u.f : \
                   is_int(x) ? (flt_t) x->u.i : \
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

void z_add(val_t *l, val_t *r) { num_arith(l,r,+); }
void z_sub(val_t *l, val_t *r) { num_arith(l,r,-); }
void z_mul(val_t *l, val_t *r) { num_arith(l,r,*); }

// TODO?
// Language comparison for division by zero:
// `inf`: lua, mawk
// error: pretty much all others
void z_div(val_t *l, val_t *r) {
    if (!numval(r))
        err("division by zero");
    flt_arith(l,r,/);
}

// TODO?
// Language comparison for modulus by zero:
// `nan`: mawk
// error: pretty much all others
void z_mod(val_t *l, val_t *r) {
    if (!numval(r))
        err("modulus by zero");
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

void z_num(val_t *v) {
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

void z_neg(val_t *v) {
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
    case TYPE_ARR: l = v->u.a->h->n;         break; // TODO
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
    case TYPE_NULL: l->u.s = s_newstr(NULL, 0, 0); break;
    case TYPE_INT:  l->u.s = s_int2str(l->u.i); break;
    case TYPE_FLT:  l->u.s = s_flt2str(l->u.f); break;
    default: break;
    }

    switch (r->type) {
    case TYPE_NULL: r->u.s = s_newstr(NULL, 0, 0); break;
    case TYPE_INT:  r->u.s = s_int2str(r->u.i); break;
    case TYPE_FLT:  r->u.s = s_flt2str(r->u.f); break;
    default: break;
    }

    assign_str(l, s_concat(l->u.s, r->u.s, 0));
}

// Potentially very slow for strings; allocates 2 new string objects
// for every int or float LHS
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
    case TYPE_ARR: // TODO? e.g. {2,4,7,1}[0]
        break;
    default:
        assign_int(l, 0);
        break;
    }
}

// OP_PRINT functionality
// TODO Command-line switches for integer format? (hex, binary, etc)
static void put(val_t *v) {
    switch (v->type) {
    case TYPE_NULL: printf("null");              break;
    case TYPE_INT:  printf("%lld", v->u.i);      break;
    case TYPE_FLT:  printf("%g", v->u.f);        break;
    case TYPE_STR:  printf("%s", v->u.s->str);   break;
    case TYPE_ARR:  printf("array: %p", v->u.a); break;
    case TYPE_FN:   // TODO
    default: break;
    }
}

// Main interpreter loop
int z_exec(code_t *c) {
    h_init(&globals);
    val_t *stk[STACK_SIZE]; // Stack
    val_t *res[STACK_SIZE]; // Reserve pointers

    // Allocate and save pointers
    for (int i = 0; i < STACK_SIZE; i++)
        res[i] = stk[i] = malloc(sizeof(val_t));
    val_t *tp; // Temp pointer
    register int      sp = 0;
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

        case OP_CALL: break; // TODO

// Pre-increment/decrement
// stk[sp-1] is address of some variable's val_t. Increment/decrement
// this value directly and replace the stack element with a copy of
// the value.
#define pre(x) \
    switch (stk[sp-1]->type) { \
    case TYPE_INT: stk[sp-1]->u.i += x; break; \
    case TYPE_FLT: stk[sp-1]->u.f += x; break; \
    case TYPE_STR: \
        assign_flt(stk[sp-1], str2flt(stk[sp-1]->u.s) + x); \
        break;\
    default: \
        assign_int(stk[sp-1], x); \
        break; \
    } \
    res[sp-1]->type = stk[sp-1]->type; \
    res[sp-1]->u    = stk[sp-1]->u; \
    stk[sp-1]       = res[sp-1]; \
    ++ip;

        case OP_PREINC: pre(1);  break;
        case OP_PREDEC: pre(-1); break;

// Post-increment/decrement
// stk[sp-1] is address of some variable's val_t. Create a copy of the
// raw value, then increment/decrement the val_t at the given address.
// Replace the stack element with the previously made copy and coerce
// to a numeric value if needed.
#define post(x) \
    res[sp-1]->type = stk[sp-1]->type; \
    res[sp-1]->u    = stk[sp-1]->u; \
    switch (stk[sp-1]->type) { \
    case TYPE_INT: stk[sp-1]->u.i += x; break; \
    case TYPE_FLT: stk[sp-1]->u.f += x; break; \
    case TYPE_STR: \
        assign_flt(stk[sp-1], str2flt(stk[sp-1]->u.s) + x); \
        break;\
    default: \
        assign_int(stk[sp-1], x); \
        break; \
    } \
    stk[sp-1] = res[sp-1]; \
    unop(num);

        case OP_POSTINC: post(1);  break;
        case OP_POSTDEC: post(-1); break;

// Compound assignment operations
// stk[sp-2] is address of some variable's val_t. Save the address and
// replace stk[sp-2] with a copy of the value. Perform the binary
// operation x and assign the result to the saved address.
#define cbinop(x) \
    tp              = stk[sp-2]; \
    res[sp-2]->type = stk[sp-2]->type; \
    res[sp-2]->u    = stk[sp-2]->u; \
    stk[sp-2]       = res[sp-2]; \
    binop(x); \
    tp->type = stk[sp-1]->type; \
    tp->u    = stk[sp-1]->u;

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

// Push immediate
// Assign integer value x to the top of the stack.
#define pushi(x) \
    assign_int(stk[sp], x); \
    ++sp;

        case OP_PUSHI: pushi(ip[1]); ip += 2; break;
        case OP_PUSH0: pushi(0);     ++ip;    break;
        case OP_PUSH1: pushi(1);     ++ip;    break;
        case OP_PUSH2: pushi(2);     ++ip;    break;

// Push constant
// Copy constant x from code object's constant table to the top of the
// stack.
#define pushk(x) \
    stk[sp]->type = c->k.v[(x)]->type; \
    stk[sp]->u    = c->k.v[(x)]->u; \
    ++sp;

        case OP_PUSHK:  pushk(ip[1]); ip += 2; break;
        case OP_PUSHK0: pushk(0);     ++ip;    break;
        case OP_PUSHK1: pushk(1);     ++ip;    break;
        case OP_PUSHK2: pushk(2);     ++ip;    break;

// Push address
// Assign the address of variable x's val_t in the globals table.
// h_lookup() will create an entry if needed, accommodating
// undeclared/uninitialized variable usage.
// Parser signals for this opcode for assignment or pre/post ++/--.
#define pusha(x) \
    stk[sp] = h_lookup(&globals, c->k.v[(x)]->u.s); \
    ++sp;

        case OP_PUSHA:  pusha(ip[1]); ip += 2; break;
        case OP_PUSHA0: pusha(0);     ++ip;    break;
        case OP_PUSHA1: pusha(1);     ++ip;    break;
        case OP_PUSHA2: pusha(2);     ++ip;    break;

// Push value
// Copy the value of variable x to the top of the stack. h_lookup()
// will create an entry if needed, accommodating
// undeclared/uninitialized variable usage.
// Parser signals for this opcode to be used when only needing the
// value, e.g. arithmetic.
#define pushv(x) \
    tp = h_lookup(&globals, c->k.v[(x)]->u.s); \
    stk[sp]->type = tp->type; \
    stk[sp]->u    = tp->u; \
    ++sp;

        case OP_PUSHV:  pushv(ip[1]); ip += 2; break;
        case OP_PUSHV0: pushv(0);     ++ip;    break;
        case OP_PUSHV1: pushv(1);     ++ip;    break;
        case OP_PUSHV2: pushv(2);     ++ip;    break;

        case OP_RET:  exit(0); // TODO
        case OP_RET1: break;   // TODO

        // Create a sequential array of (IP+1) elements from the top
        // of the stack. Leave the array val_t on the stack.
        // Arrays index at 0 by default.
        case OP_ARRAY:
            tp = v_newarr();
            for (int i = ip[1] - 1; i >= 0; --i) {
                a_insert_int(tp->u.a, i, stk[--sp]);
            }
            stk[sp++] = tp;
            ip += 2;
            break;

        // IDXA
        // Perform the lookup and leave the corresponding element's
        // val_t address on the stack.
        case OP_IDXA:
            switch (stk[sp-2]->type) {

            // Create array if stk[sp-2] is an uninitialized variable
            case TYPE_NULL:
                tp              = v_newarr();
                stk[sp-2]->type = tp->type;
                stk[sp-2]->u    = tp->u;
                // Fall-through
            case TYPE_ARR:
                stk[sp-2] = a_lookup(stk[sp-2]->u.a, stk[sp-1]);
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
                tp = a_lookup(stk[sp-2]->u.a, stk[sp-1]);
                res[sp-2]->type = tp->type;
                res[sp-2]->u    = tp->u;
                stk[sp-2]       = res[sp-2];
                --sp;
                ++ip;
                break;

            // TODO parser should be signaling OP_PUSHV, this handles
            // OP_PUSHA usage for now
            case TYPE_INT: case TYPE_FLT: case TYPE_STR:
                res[sp-2]->type = stk[sp-2]->type;
                res[sp-2]->u    = stk[sp-2]->u;
                stk[sp-2]       = res[sp-2];
                binop(idx);
                break;
            default:
                break;
            }
            break;

        case OP_SET:
            tp              = stk[sp-2];
            res[sp-2]->type = stk[sp-1]->type;
            res[sp-2]->u    = stk[sp-1]->u;
            stk[sp-2]       = res[sp-2];
            tp->type        = res[sp-2]->type;
            tp->u           = res[sp-2]->u;
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
#undef pusha
#undef pushi
#undef pushk
#undef pushv
