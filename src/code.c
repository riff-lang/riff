#include "code.h"

#include "mem.h"

#include <stdio.h>

#define push(x) c_push(c, x)

static void err(rf_code *c, const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void c_init(rf_code *c) {
    c->code = NULL;
    c->k    = NULL;
    c->n    = 0;
    c->cap  = 0;
    c->nk   = 0;
    c->kcap = 0;
}

void c_push(rf_code *c, uint8_t b) {
    m_growarray(c->code, c->n, c->cap, uint8_t);
    c->code[c->n++] = b;
}

void c_free(rf_code *c) {
    free(c);
    c_init(c);
}

// Push a jump instruction and return the location of the byte to be
// patched
int c_prep_jump(rf_code *c, int type) {
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

int c_prep_loop(rf_code *c, int type) {
    if (type)
        push(OP_ITERKV);
    else
        push(OP_ITERV);
    // Reserve two bytes for 16-bit jump
    push(0x00);
    push(0x00);
    return c->n - 2;
}

// Simple backward jumps. Encode a 2-byte offset if necessary.
void c_jump(rf_code *c, int type, int l) {
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
    } else if (d <= INT16_MAX && d >= INT16_MIN) {
        switch (type) {
        case JMP:  push(OP_JMP16);  break;
        case JZ:   push(OP_JZ16);   break; 
        case JNZ:  push(OP_JNZ16);  break;
        case XJZ:  push(OP_XJZ16);  break;
        case XJNZ: push(OP_XJNZ16); break;
        default: break;
        }
        push((int8_t) ((d >> 8) & 0xff));
        push((int8_t) (d & 0xff));
    } else {
        err(c, "backward jump larger than INT16_MAX");
    }
}

void c_loop(rf_code *c, int l) {
    int d = c->n - l;
    if (d <= UINT8_MAX) {
        push(OP_LOOP8);
        push((uint8_t) d);
    } else if (d <= UINT16_MAX) {
        push(OP_LOOP16);
        push((uint8_t) ((d >> 8) & 0xff));
        push((uint8_t) d);
    } else {
        err(c, "backward loop too large");
    }
}

void c_range(rf_code *c, int from, int to, int step) {
    if (step) {
        if (from) {
            if (to)
                push(OP_SSEQ);
            else
                push(OP_SSEQF);
        } else {
            if (to)
                push(OP_SSEQT);
            else
                push(OP_SSEQE);
        }
    } else {
        if (from) {
            if (to)
                push(OP_SEQ);
            else
                push(OP_SEQF);
        } else {
            if (to)
                push(OP_SEQT);
            else
                push(OP_SEQE);
        }
    }
}

// Overwrite the bytes at location l and l+1 with the distance between
// the code object's "current" instruction and `l`. Useful for patching
// forward jumps.
void c_patch(rf_code *c, int l) {
    int jmp = c->n - l + 1;
    if (jmp > INT16_MAX)
        err(c, "forward jump too large to patch");
    c->code[l]     = ((jmp >> 8) & 0xff);
    c->code[l + 1] = (jmp & 0xff);
}

static void c_pushk(rf_code *c, int i) {
    switch (i) {
    case 0: push(OP_PUSHK0); break;
    case 1: push(OP_PUSHK1); break;
    case 2: push(OP_PUSHK2); break;
    default:
        push(OP_PUSHK);
        push((uint8_t) i);
        break;
    }
}

void c_fn_constant(rf_code *c, rf_fn *fn) {
    m_growarray(c->k, c->nk, c->kcap, rf_val);
    c->k[c->nk++] = (rf_val) {TYPE_RFN, .u.fn = fn};
    if (c->nk > (UINT8_MAX + 1))
        err(c, "Exceeded max number of unique literals");
    c_pushk(c, c->nk - 1);
}

// Add a rf_val literal to a code object's constant table, if
// necessary
void c_constant(rf_code *c, rf_token *tk) {

    if (tk->kind == TK_NULL) {
        push(OP_NULL);
        return;
    } else if (tk->kind == TK_RE) {
        m_growarray(c->k, c->nk, c->kcap, rf_val);
        c->k[c->nk++] = (rf_val) {TYPE_RE, .u.r = tk->lexeme.r};
        c_pushk(c, c->nk - 1);
        return;
    }

    // Search for existing duplicate literal
    for (int i = 0; i < c->nk; i++) {
        switch (tk->kind) {
        case TK_FLT:
            if (c->k[i].type != TYPE_FLT)
                break;
            else if (tk->lexeme.f == c->k[i].u.f) {
                c_pushk(c, i);
                return;
            }
            break;
        case TK_INT:
            if (c->k[i].type != TYPE_INT)
                break;
            else if (tk->lexeme.i == c->k[i].u.i) {
                c_pushk(c, i);
                return;
            }
            break;
        case TK_STR:
            if (c->k[i].type != TYPE_STR)
                break;
            else if (tk->lexeme.s->hash == c->k[i].u.s->hash) {
                c_pushk(c, i);
                return;
            }
            break;
        default: break;
        }
    }

    // Provided literal does not already exist in constant table
    switch (tk->kind) {
    case TK_FLT:
        m_growarray(c->k, c->nk, c->kcap, rf_val);
        c->k[c->nk++] = (rf_val) {TYPE_FLT, .u.f = tk->lexeme.f};
        break;
    case TK_INT: {
        rf_int i = tk->lexeme.i;
        switch (i) {
        case 0: push(OP_IMM0); return;
        case 1: push(OP_IMM1); return;
        case 2: push(OP_IMM2); return;
        default:
            if (i >= 3 && i <= UINT8_MAX) {
                push(OP_IMM8);
                push((uint8_t) i);
                return;
            } else if (i >= 256 && i <= UINT16_MAX) {
                push(OP_IMM16);
                push((uint8_t) ((i >> 8) & 0xff));
                push((uint8_t) i);
                return;
            } else {
                m_growarray(c->k, c->nk, c->kcap, rf_val);
                c->k[c->nk++] = (rf_val) {TYPE_INT, .u.i = i};
            }
            break;
        }
        break;
    }
    case TK_STR: {
        m_growarray(c->k, c->nk, c->kcap, rf_val);
        rf_str *s = s_newstr(tk->lexeme.s->str, tk->lexeme.s->l, 1);
        c->k[c->nk++] = (rf_val) {TYPE_STR, .u.s = s};
        break;
    }
    default: break;
    }

    if (c->nk > (UINT8_MAX + 1))
        err(c, "Exceeded max number of unique literals");
    c_pushk(c, c->nk - 1);
}

static void push_global_addr(rf_code *c, int i) {
    switch (i) {
    case 0: push(OP_GBLA0); break;
    case 1: push(OP_GBLA1); break;
    case 2: push(OP_GBLA2); break;
    default:
        push(OP_GBLA);
        push((uint8_t) i);
        break;
    }
}

static void push_global_val(rf_code *c, int i) {
    switch (i) {
    case 0: push(OP_GBLV0); break;
    case 1: push(OP_GBLV1); break;
    case 2: push(OP_GBLV2); break;
    default:
        push(OP_GBLV);
        push((uint8_t) i);
        break;
    }
}

// mode = 1 => VM will push the pointer to the rf_val in the global
//             hash table onto the stack
// mode = 0 => VM will make a copy of the global's rf_val and push it
//             onto the stack
void c_global(rf_code *c, rf_token *tk, int mode) {

    // Search for existing symbol
    for (int i = 0; i < c->nk; ++i) {
        if (c->k[i].type == TYPE_STR &&
            tk->lexeme.s->hash == c->k[i].u.s->hash) {
            if (mode) push_global_addr(c, i);
            else      push_global_val(c, i);
            return;
        }
    }
    rf_str *s = s_newstr(tk->lexeme.s->str, tk->lexeme.s->l, 1);
    m_growarray(c->k, c->nk, c->kcap, rf_val);
    c->k[c->nk++] = (rf_val) {TYPE_STR, .u.s = s};
    if (c->nk > (UINT8_MAX + 1))
        err(c, "Exceeded max number of unique literals");
    if (mode) push_global_addr(c, c->nk - 1);
    else      push_global_val(c, c->nk - 1);
}

static void push_local_addr(rf_code *c, int i) {
    switch (i) {
    case 0: push(OP_LCLA0); break;
    case 1: push(OP_LCLA1); break;
    case 2: push(OP_LCLA2); break;
    default:
        push(OP_LCLA);
        push((uint8_t) i);
        break;
    }
}

static void push_local_val(rf_code *c, int i) {
    switch (i) {
    case 0: push(OP_LCLV0); break;
    case 1: push(OP_LCLV1); break;
    case 2: push(OP_LCLV2); break;
    default:
        push(OP_LCLV);
        push((uint8_t) i);
        break;
    }
}

// mode = 1 => VM will push the pointer to the rf_val at stack[i]
// mode = 0 => VM will make a copy of the rf_val at stack[i] and
//             push onto the stack
void c_local(rf_code *c, int i, int mode) {
    if (mode)
        push_local_addr(c, i);
    else
        push_local_val(c, i);
}

void c_table(rf_code *c, int n) {
    if (!n)
        push(OP_TBL0);
    else if (n <= 0xff) {
        push(OP_TBL);
        push((uint8_t) n);
    } else {
        push(OP_TBLK);

        // Search for exisitng `n` in the constants pool
        for (int i = 0; i < c->nk; ++i) {
            if (c->k[i].type != TYPE_INT)
                continue;
            if (c->k[i].u.i == n) {
                push((uint8_t) i);
                return;
            }
        }

        // Otherwise, add `n` to constants pool
        m_growarray(c->k, c->nk, c->kcap, rf_val);
        c->k[c->nk++] = (rf_val) {TYPE_INT, .u.i = (rf_int) n};
        push((uint8_t) c->nk - 1);
    }
}

void c_index(rf_code *c, int n, int type) {
    if (n == 1) {
        if (type)
            push(OP_IDXA1);
        else
            push(OP_IDXV1);
    } else {
        if (type) {
            push(OP_IDXA);
            push((uint8_t) n);
        } else {
            push(OP_IDXV);
            push((uint8_t) n);
        }
    }
}

// TODO this function will be used to evaluate infix expression
// "nodes" and fold constants if possible.
void c_infix(rf_code *c, int op) {
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
}

void c_prefix(rf_code *c, int op) {
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
}

void c_postfix(rf_code *c, int op) {
    switch (op) {
    case TK_INC: push(OP_POSTINC); break;
    case TK_DEC: push(OP_POSTDEC); break;
    default: break;
    }
}

void c_pop(rf_code *c, int n) {
    if (n == 1)
        push(OP_POP);
    else {
        push(OP_POPI);
        push((uint8_t) n);
    }
}

// t = 0 => void return
// t = 1 => return one value
void c_return(rf_code *c, int t) {
    if (!t)
        push(OP_RET);
    else {

        // Patch OP_CALLs immediately preceding OP_RET1 to be tailcall
        // optimized
        if (c->n > 1 && c->code[c->n-2] == OP_CALL) {
            c->code[c->n-2] = OP_TCALL;
        }
        push(OP_RET1);
    }
}
