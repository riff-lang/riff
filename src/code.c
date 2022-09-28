#include "code.h"

#include "mem.h"
#include "string.h"

#include <stdio.h>

#define push(x) c_push(c, x)

#define LAST_INS_IDX(arity) (c->n-(arity)-1)

static void err(riff_code *c, const char *msg) {
    fprintf(stderr, "riff: [compile] %s\n", msg);
    exit(1);
}

void c_init(riff_code *c) {
    c->code = NULL;
    c->k    = NULL;
    c->last = 0;
    c->n    = 0;
    c->cap  = 0;
    c->nk   = 0;
    c->kcap = 0;
}

void c_push(riff_code *c, uint8_t b) {
    m_growarray(c->code, c->n, c->cap, uint8_t);
    c->code[c->n++] = b;
}

static void push_i16(riff_code *c, int16_t i) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    push((int8_t) (i & 0xff));
    push((int8_t) ((i >> 8) & 0xff));
#else
    push((int8_t) ((i >> 8) & 0xff));
    push((int8_t) (i & 0xff));
#endif
}

static void push_u16(riff_code *c, uint16_t i) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    push((uint8_t) (i & 0xff));
    push((uint8_t) ((i >> 8) & 0xff));
#else
    push((uint8_t) ((i >> 8) & 0xff));
    push((uint8_t) (i & 0xff));
#endif
}

// Push a jump instruction and return the location of the byte to be
// patched
int c_prep_jump(riff_code *c, int type) {
    switch (type) {
    case JMP:  push(OP_JMP16);  break;
    case JZ:   push(OP_JZ16);   break; 
    case JNZ:  push(OP_JNZ16);  break;
    case XJZ:  push(OP_XJZ16);  break;
    case XJNZ: push(OP_XJNZ16); break;
    default: break;
    }
    // Reserve two bytes for 16-bit jump
    push(0x00);
    push(0x00);
    return c->n - 2;
}

int c_prep_loop(riff_code *c, int type) {
    if (type)
        push(OP_ITERKV);
    else
        push(OP_ITERV);
    // Reserve two bytes for 16-bit jump
    push(0x00);
    push(0x00);
    return c->n - 2;
}

void c_end_loop(riff_code *c) {
    push(OP_POPL);
    c->last = LAST_INS_IDX(0);
}

// Simple backward jumps. Encode a 2-byte offset if necessary.
void c_jump(riff_code *c, int type, int l) {
    int d = l - c->n;
    if (d <= INT8_MAX && d >= INT8_MIN) {
        switch (type) {
        case JMP:  push(OP_JMP8);  break;
        case JZ:   push(OP_JZ8);   break; 
        case JNZ:  push(OP_JNZ8);  break;
        case XJZ:  push(OP_XJZ8);  break;
        case XJNZ: push(OP_XJNZ8); break;
        default: break;
        }
        push((int8_t) d);
        c->last = LAST_INS_IDX(1);
    } else if (d <= INT16_MAX && d >= INT16_MIN) {
        switch (type) {
        case JMP:  push(OP_JMP16);  break;
        case JZ:   push(OP_JZ16);   break; 
        case JNZ:  push(OP_JNZ16);  break;
        case XJZ:  push(OP_XJZ16);  break;
        case XJNZ: push(OP_XJNZ16); break;
        default: break;
        }
        push_i16(c, (int16_t) d);
        c->last = LAST_INS_IDX(2);
    } else {
        err(c, "backward jump larger than INT16_MAX");
    }
}

void c_loop(riff_code *c, int l) {
    int d = c->n - l;
    if (d <= UINT8_MAX) {
        push(OP_LOOP8);
        push((uint8_t) d);
        c->last = LAST_INS_IDX(1);
    } else if (d <= UINT16_MAX) {
        push(OP_LOOP16);
        push_u16(c, (uint16_t) d);
        c->last = LAST_INS_IDX(2);
    } else {
        err(c, "backward loop too large");
    }
}

void c_range(riff_code *c, int from, int to, int step) {
    if (step) {
        if (from) {
            push(to ? OP_SRNG : OP_SRNGF);
        } else {
            push(to ? OP_SRNGT : OP_SRNGI);
        }
    } else {
        if (from) {
            push(to ? OP_RNG : OP_RNGF);
        } else {
            push(to ? OP_RNGT : OP_RNGI);
        }
    }
    c->last = LAST_INS_IDX(0);
}

// Overwrite the bytes at location l and l+1 with the distance between the code
// object's "current" instruction and `l`. Useful for patching forward jumps.
void c_patch(riff_code *c, int l) {
    int jmp = c->n - l + 1;
    if (jmp > INT16_MAX) {
        err(c, "forward jump too large to patch");
    }
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    c->code[l]   = (jmp & 0xff);
    c->code[l+1] = ((jmp >> 8) & 0xff);
#else
    c->code[l]   = ((jmp >> 8) & 0xff);
    c->code[l+1] = (jmp & 0xff);
#endif
}

static void push_constant(riff_code *c, int i) {
    switch (i) {
    case 0:
    case 1:
    case 2:
        push(OP_CONST0 + i);
        c->last = LAST_INS_IDX(0);
        break;
    default:
        push(OP_CONST);
        push((uint8_t) i);
        c->last = LAST_INS_IDX(1);
    }
}

void c_fn_constant(riff_code *c, riff_fn *fn) {
    m_growarray(c->k, c->nk, c->kcap, riff_val);
    c->k[c->nk++] = (riff_val) {TYPE_RFN, .fn = fn};
    if (c->nk > (UINT8_MAX + 1))
        err(c, "Exceeded max number of unique literals");
    push_constant(c, c->nk - 1);
}

static int find_constant(riff_code *c, riff_token *tk) {
    for (int i = 0; i < c->nk; ++i) {
        switch (tk->kind) {
        case TK_INT:
            if (is_int(&c->k[i]) && tk->i == c->k[i].i)
                return i;
            break;
        case TK_FLOAT:
            if (is_float(&c->k[i]) && tk->f == c->k[i].f)
                return i;
            break;
        case TK_STR:
        case TK_IDENT:
            if (is_str(&c->k[i]) && riff_str_eq(tk->s, c->k[i].s))
                return i;
            break;
        }
    }
    return -1;
}

// Add a riff_val literal to a code object's constant table, if necessary
void c_constant(riff_code *c, riff_token *tk) {
    if (tk->kind == TK_NULL) {
        push(OP_NUL);
        c->last = LAST_INS_IDX(0);
        return;
    } else if (tk->kind == TK_REGEX) {
        m_growarray(c->k, c->nk, c->kcap, riff_val);
        c->k[c->nk++] = (riff_val) {TYPE_REGEX, .r = tk->r};
        if (c->nk > (UINT8_MAX + 1))
            err(c, "Exceeded max number of unique literals");
        push_constant(c, c->nk - 1);
        return;
    }

    int index = find_constant(c, tk);
    if (index >= 0) {
        push_constant(c, index);
        return;
    }

    // Provided literal does not already exist in constant table
    switch (tk->kind) {
    case TK_FLOAT:
        m_growarray(c->k, c->nk, c->kcap, riff_val);
        c->k[c->nk++] = (riff_val) {TYPE_FLOAT, .f = tk->f};
        break;
    case TK_INT: {
        riff_int i = tk->i;
        switch (i) {
        case 0:
        case 1:
        case 2:
            push(OP_IMM0 + i);
            c->last = LAST_INS_IDX(0);
            return;
        default:
            if (i >= 3 && i <= UINT8_MAX) {
                push(OP_IMM8);
                push((uint8_t) i);
                c->last = LAST_INS_IDX(1);
                return;
            } else if (i >= 256 && i <= UINT16_MAX) {
                push(OP_IMM16);
                push_u16(c, (uint16_t) i);
                c->last = LAST_INS_IDX(2);
                return;
            } else {
                m_growarray(c->k, c->nk, c->kcap, riff_val);
                c->k[c->nk++] = (riff_val) {TYPE_INT, .i = i};
            }
            break;
        }
        break;
    }
    case TK_STR: case TK_IDENT: {
        m_growarray(c->k, c->nk, c->kcap, riff_val);
        c->k[c->nk++] = (riff_val) {TYPE_STR, .s = tk->s};
        break;
    }
    default: break;
    }

    if (c->nk > (UINT8_MAX + 1))
        err(c, "Exceeded max number of unique literals");
    push_constant(c, c->nk - 1);
}

static void push_global_addr(riff_code *c, int i) {
    switch (i) {
    case 0:
    case 1:
    case 2:
        push(OP_GBLA0 + i);
        c->last = LAST_INS_IDX(0);
        break;
    default:
        push(OP_GBLA);
        push((uint8_t) i);
        c->last = LAST_INS_IDX(1);
    }
}

static void push_global_val(riff_code *c, int i) {
    switch (i) {
    case 0:
    case 1:
    case 2:
        push(OP_GBLV0 + i);
        c->last = LAST_INS_IDX(0);
        break;
    default:
        push(OP_GBLV);
        push((uint8_t) i);
        c->last = LAST_INS_IDX(1);
    }
}

// mode = 1 => VM will push the pointer to the riff_val in the global
//             hash table onto the stack
// mode = 0 => VM will make a copy of the global's riff_val and push it
//             onto the stack
void c_global(riff_code *c, riff_token *tk, int mode) {

    // Search for existing symbol
    for (int i = 0; i < c->nk; ++i) {
        if (is_str(&c->k[i]) && riff_str_eq(tk->s, c->k[i].s)) {
            mode ? push_global_addr(c, i) : push_global_val(c, i);
            return;
        }
    }
    m_growarray(c->k, c->nk, c->kcap, riff_val);
    c->k[c->nk++] = (riff_val) {TYPE_STR, .s = tk->s};
    if (c->nk > (UINT8_MAX + 1))
        err(c, "Exceeded max number of unique literals");
    mode ? push_global_addr(c, c->nk - 1) : push_global_val(c, c->nk - 1);
}

static void push_local_addr(riff_code *c, int i) {
    switch (i) {
    case 0:
    case 1:
    case 2:
        push(OP_LCLA0 + i);
        c->last = LAST_INS_IDX(0);
        break;
    default:
        push(OP_LCLA);
        push((uint8_t) i);
        c->last = LAST_INS_IDX(1);
    }
}

static void push_local_val(riff_code *c, int i) {
    switch (i) {
    case 0:
    case 1:
    case 2:
        push(OP_LCLV0 + i);
        c->last = LAST_INS_IDX(0);
        break;
    default:
        push(OP_LCLV);
        push((uint8_t) i);
        c->last = LAST_INS_IDX(1);
    }
}

// mode = 1 => VM will push the pointer to the riff_val at stack[i]
// mode = 0 => VM will make a copy of the riff_val at stack[i] and
//             push onto the stack
void c_local(riff_code *c, int i, int mode, int reserve) {
    if (mode) {
        if (reserve) {
            push(OP_DUPA);
            c->last = LAST_INS_IDX(0);
        } else {
            push_local_addr(c, i);
        }
    } else {
        push_local_val(c, i);
    }
}

void c_table(riff_code *c, int n) {
    if (!n) {
        push(OP_TAB0);
        c->last = LAST_INS_IDX(0);
        return;
    } else if (n <= 0xff) {
        push(OP_TAB);
        push((uint8_t) n);
        c->last = LAST_INS_IDX(1);
        return;
    } else {
        push(OP_TABK);

        // Search for exisitng `n` in the constants pool
        for (int i = 0; i < c->nk; ++i) {
            if (is_int(&c->k[i]) && c->k[i].i == n) {
                push((uint8_t) i);
                c->last = LAST_INS_IDX(1);
                return;
            }
        }

        // Otherwise, add `n` to constants pool
        m_growarray(c->k, c->nk, c->kcap, riff_val);
        c->k[c->nk++] = (riff_val) {TYPE_INT, .i = (riff_int) n};
        if (c->nk > (UINT8_MAX + 1)) {
            err(c, "Exceeded max number of unique literals");
        }
        push((uint8_t) c->nk - 1);
    }
    c->last = LAST_INS_IDX(1);
}

static int is_addr(uint8_t opcode) {
    switch (opcode) {
    case OP_DUPA:
    case OP_GBLA:
    case OP_GBLA0:
    case OP_GBLA1:
    case OP_GBLA2:
    case OP_LCLA:
    case OP_LCLA0:
    case OP_LCLA1:
    case OP_LCLA2:
    case OP_IDXA:
    case OP_IDXA1:
    case OP_SIDXA:
    case OP_FLDA:
        return 1;
    }
    return 0;
}

static void patch_val_to_addr(uint8_t *opcode) {
    switch (*opcode) {
    case OP_GBLV:
    case OP_GBLV0:
    case OP_GBLV1:
    case OP_GBLV2:
        *opcode = OP_GBLA + (*opcode - OP_GBLV);
        break;
    case OP_LCLV:
    case OP_LCLV0:
    case OP_LCLV1:
    case OP_LCLV2:
        *opcode = OP_LCLA + (*opcode - OP_LCLV);
        break;
    case OP_IDXV:
        *opcode = OP_IDXA;
        break;
    case OP_IDXV1:
        *opcode = OP_IDXA1;
        break;
    case OP_SIDXV:
        *opcode = OP_SIDXA;
        break;
    case OP_FLDV:
        *opcode = OP_FLDA;
        break;
    }
}

void c_index(riff_code *c, int prev_idx, int n, int addr) {
    uint8_t *prev = &c->code[prev_idx];
    patch_val_to_addr(prev);
    if (n == 1) {
        if (!is_addr(*prev)) {
            if (addr) {
                err(c, "syntax error");
            }
            push(OP_VIDXV);
        } else {
            push(addr ? OP_IDXA1 : OP_IDXV1);
        }
        c->last = LAST_INS_IDX(0);
    } else {
        if (addr && !is_addr(*prev)) {
            err(c, "syntax error");
        }
        push(addr ? OP_IDXA : OP_IDXV);
        push((uint8_t) n);
        c->last = LAST_INS_IDX(1);
    }
}

void c_str_index(riff_code *c, int prev_idx, riff_str *k, int addr) {
    uint8_t *prev = &c->code[prev_idx];
    patch_val_to_addr(prev);
    if (!is_addr(*prev)) {
        err(c, "invalid member access");
    }
    int idx = find_constant(c, &(riff_token) {TK_STR, .s = k});
    if (idx < 0) {
        m_growarray(c->k, c->nk, c->kcap, riff_val);
        c->k[c->nk++] = (riff_val) {TYPE_STR, .s = k};
        if (c->nk > (UINT8_MAX + 1)) {
            err(c, "Exceeded max number of unique literals");
        }
        idx = c->nk - 1;
    }
    push(addr ? OP_SIDXA : OP_SIDXV);
    push((uint8_t) idx);
    c->last = LAST_INS_IDX(1);
}

void c_fldv_index(riff_code *c, int mode) {
    push(mode ? OP_FLDA : OP_FLDV);
    c->last = LAST_INS_IDX(0);
}

void c_call(riff_code *c, int n) {
    push(OP_CALL);
    push((uint8_t) n);
    c->last = LAST_INS_IDX(1);
}

// TODO this function will be used to evaluate infix expression "nodes" and fold
// constants if possible.
void c_infix(riff_code *c, int op) {
    switch (op) {
    case '+':       push(OP_ADD);    break;
    case '-':       push(OP_SUB);    break;
    case '*':       push(OP_MUL);    break;
    case '/':       push(OP_DIV);    break;
    case '#':       push(OP_CAT);    break;
    case '%':       push(OP_MOD);    break;
    case '>':       push(OP_GT);     break;
    case '<':       push(OP_LT);     break;
    case '=':       push(OP_SET);    break;
    case '&':       push(OP_AND);    break;
    case '|':       push(OP_OR);     break;
    case '^':       push(OP_XOR);    break;
    case '~':       push(OP_MATCH);  break;
    case TK_NMATCH: push(OP_NMATCH); break;
    case TK_SHL:    push(OP_SHL);    break;
    case TK_SHR:    push(OP_SHR);    break;
    case TK_POW:    push(OP_POW);    break;
    case TK_GE:     push(OP_GE);     break;
    case TK_LE:     push(OP_LE);     break;
    case TK_EQ:     push(OP_EQ);     break;
    case TK_NE:     push(OP_NE);     break;
    case TK_ADDX:   push(OP_ADDX);   break;
    case TK_ANDX:   push(OP_ANDX);   break;
    case TK_DIVX:   push(OP_DIVX);   break;
    case TK_MODX:   push(OP_MODX);   break;
    case TK_MULX:   push(OP_MULX);   break;
    case TK_ORX:    push(OP_ORX);    break;
    case TK_SUBX:   push(OP_SUBX);   break;
    case TK_XORX:   push(OP_XORX);   break;
    case TK_CATX:   push(OP_CATX);   break;
    case TK_POWX:   push(OP_POWX);   break;
    case TK_SHLX:   push(OP_SHLX);   break;
    case TK_SHRX:   push(OP_SHRX);   break;
    default: break;
    }
    c->last = LAST_INS_IDX(0);
}

void c_prefix(riff_code *c, int op) {
    switch (op) {
    case '!':    push(OP_LNOT);   break;
    case '#':    push(OP_LEN);    break;
    case '+':    push(OP_NUM);    break;
    case '-':    push(OP_NEG);    break;
    case '~':    push(OP_NOT);    break;
    case TK_INC: push(OP_PREINC); break;
    case TK_DEC: push(OP_PREDEC); break;
    default: break;
    }
    c->last = LAST_INS_IDX(0);
}

void c_postfix(riff_code *c, int op) {
    switch (op) {
    case TK_INC: push(OP_POSTINC); break;
    case TK_DEC: push(OP_POSTDEC); break;
    default: break;
    }
    c->last = LAST_INS_IDX(0);
}

void c_concat(riff_code *c, int n) {
    if (n == 2) {
        push(OP_CAT);
        c->last = LAST_INS_IDX(0);
    } else if (n > 2) {
        push(OP_CATI);
        push((uint8_t) n);
        c->last = LAST_INS_IDX(1);
    }
}

void c_pop(riff_code *c, int n) {
    if (n == 1) {
        push(OP_POP);
        c->last = LAST_INS_IDX(0);
    } else if (n > 1) {
        push(OP_POPI);
        push((uint8_t) n);
        c->last = LAST_INS_IDX(1);
    }
}

void c_pop_expr_stmt(riff_code *c, int n) {
    if (n == 1 && c->code[c->n-1] == OP_SET) {
        c->code[c->n-1] = OP_SETP;
        c->last = LAST_INS_IDX(0);
    } else {
        c_pop(c, n);
    }
}

// t = 0 => void return
// t = 1 => return one value
void c_return(riff_code *c, int t) {
    // TODO patch TCALL?
    if (!t) {
        push(OP_RET);
    } else {

        // Patch OP_CALLs immediately preceding OP_RET1 to be tailcall optimized
        if (c->code[c->last] == OP_CALL) {
            c->code[c->last] = OP_TCALL;
        }
        push(OP_RET1);
    }
    c->last = LAST_INS_IDX(0);
}
