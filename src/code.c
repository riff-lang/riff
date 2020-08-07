#include <stdio.h>

#include "code.h"
#include "mem.h"

#define push(x) c_push(c, x)

static void err(code_t *c, const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void c_init(code_t *c) {
    c->n     = 0;
    c->cap   = 0;
    c->code  = NULL;
    c->k.n   = 0;
    c->k.cap = 0;
    c->k.v   = NULL;
}

void c_push(code_t *c, uint8_t b) {
    eval_resize(c->code, c->n, c->cap);
    c->code[c->n++] = b;
}

void c_free(code_t *c) {
    free(c);
    c_init(c);
}

// Push a jump instruction and return the location of the byte to be
// patched
int c_prep_jump(code_t *c, int type) {
    switch (type) {
    case JMP:  push(OP_JMP8);  break;
    case JZ:   push(OP_JZ8);   break; 
    case JNZ:  push(OP_JNZ8);  break;
    case XJZ:  push(OP_XJZ8);  break;
    case XJNZ: push(OP_XJNZ8); break;
    default: break;
    }
    push(0x00); // Reserve byte
    return c->n - 1;
}

// Simple backward jumps. Encode a 2-byte offset if necessary.
void c_jump(code_t *c, int type, int l) {
    int d = l - c->n;
    uint8_t jmp;
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
        err(c, "Backward jump too large");
    }
}

// Overwrite the byte at location `l` with the distance between
// the code object's "current" instruction and `l`. Useful for patching
// forward jumps.
void c_patch(code_t *c, int l) {
    int jmp = c->n - l + 1;
    if (jmp > INT8_MAX)
        err(c, "Forward jump too large to patch");
    c->code[l] = (int8_t) jmp;
}

static void c_pushk(code_t *c, int i) {
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

// Add a val_t literal to a code object's constant table, if
// necessary
void c_constant(code_t *c, token_t *tk) {

    // Search for existing duplicate literal
    for (int i = 0; i < c->k.n; i++) {
        switch (tk->kind) {
        case TK_FLT:
            if (c->k.v[i]->type != TYPE_FLT)
                break;
            else if (tk->lexeme.f == c->k.v[i]->u.f) {
                c_pushk(c, i);
                return;
            }
            break;
        case TK_INT:
            if (c->k.v[i]->type != TYPE_INT)
                break;
            else if (tk->lexeme.i == c->k.v[i]->u.i) {
                c_pushk(c, i);
                return;
            }
            break;
        case TK_STR:
            if (c->k.v[i]->type != TYPE_STR)
                break;
            else if (tk->lexeme.s->hash == c->k.v[i]->u.s->hash) {
                c_pushk(c, i);
                return;
            }
            break;
        default: break;
        }
    }

    // Provided literal does not already exist in constant table
    val_t *v;
    switch (tk->kind) {
    case TK_FLT:
        v = v_newflt(tk->lexeme.f);
        break;
    case TK_INT: {
        int_t i = tk->lexeme.i;
        switch (i) {
        case 0: push(OP_PUSH0); return;
        case 1: push(OP_PUSH1); return;
        case 2: push(OP_PUSH2); return;
        default:
            if (i >= 3 && i <= 255) {
            push(OP_PUSHI);
            push((uint8_t) i);
            return;
        } else {
            v = v_newint(i);
        }
            break;
        }
        break;
    }
    case TK_STR:
        v = v_newstr(tk->lexeme.s);
        break;
    default: break;
    }

    eval_resize(c->k.v, c->k.n, c->k.cap);
    c->k.v[c->k.n++] = v;
    if (c->k.n > 255)
        err(c, "Exceeded max number of unique literals");
    c_pushk(c, c->k.n - 1);
}

static void push_global_addr(code_t *c, int i) {
    switch (i) {
    case 0: push(OP_PUSHGA0); break;
    case 1: push(OP_PUSHGA1); break;
    case 2: push(OP_PUSHGA2); break;
    default:
        push(OP_PUSHGA);
        push((uint8_t) i);
        break;
    }
}

static void push_global_val(code_t *c, int i) {
    switch (i) {
    case 0: push(OP_PUSHGV0); break;
    case 1: push(OP_PUSHGV1); break;
    case 2: push(OP_PUSHGV2); break;
    default:
        push(OP_PUSHGV);
        push((uint8_t) i);
        break;
    }
}

// mode = 1 => VM will push the pointer to the val_t in the global
//             hash table onto the stack
// mode = 0 => VM will make a copy of the global's val_t and push it
//             onto the stack
void c_global(code_t *c, token_t *tk, int mode) {

    // Search for existing symbol
    for (int i = 0; i < c->k.n; i++) {
        if (c->k.v[i]->type == TYPE_STR &&
            tk->lexeme.s->hash == c->k.v[i]->u.s->hash) {
            if (mode) push_global_addr(c, i);
            else      push_global_val(c, i);
            return;
        }
    }
    val_t *v;
    v = v_newstr(tk->lexeme.s);
    eval_resize(c->k.v, c->k.n, c->k.cap);
    c->k.v[c->k.n++] = v;
    if (c->k.n > 255)
        err(c, "Exceeded max number of unique literals");
    if (mode) push_global_addr(c, c->k.n - 1);
    else      push_global_val(c, c->k.n - 1);
}

static void push_local_addr(code_t *c, int i) {
    switch (i) {
    case 0: push(OP_PUSHLA0); break;
    case 1: push(OP_PUSHLA1); break;
    case 2: push(OP_PUSHLA2); break;
    default:
        push(OP_PUSHLA);
        push((uint8_t) i);
        break;
    }
}

static void push_local_val(code_t *c, int i) {
    switch (i) {
    case 0: push(OP_PUSHLV0); break;
    case 1: push(OP_PUSHLV1); break;
    case 2: push(OP_PUSHLV2); break;
    default:
        push(OP_PUSHLV);
        push((uint8_t) i);
        break;
    }
}

// mode = 1 => VM will push the pointer to the val_t at stack[i]
// mode = 0 => VM will make a copy of the val_t at stack[i] and push
//             onto the stack
void c_local(code_t *c, int i, int mode) {
    if (mode)
        push_local_addr(c, i);
    else
        push_local_val(c, i);
}

// TODO this function will be used to evaluate infix expression
// "nodes" and fold constants if possible.
void c_infix(code_t *c, int op) {
    switch (op) {
    case '+':     push(OP_ADD);  break;
    case '-':     push(OP_SUB);  break;
    case '*':     push(OP_MUL);  break;
    case '/':     push(OP_DIV);  break;
    case '%':     push(OP_MOD);  break;
    case '>':     push(OP_GT);   break;
    case '<':     push(OP_LT);   break;
    case '=':     push(OP_SET);  break;
    case '&':     push(OP_AND);  break;
    case '|':     push(OP_OR);   break;
    case '^':     push(OP_XOR);  break;
    case TK_SHL:  push(OP_SHL);  break;
    case TK_SHR:  push(OP_SHR);  break;
    case TK_POW:  push(OP_POW);  break;
    case TK_CAT:  push(OP_CAT);  break;
    case TK_GE:   push(OP_GE);   break;
    case TK_LE:   push(OP_LE);   break;
    case TK_EQ:   push(OP_EQ);   break;
    case TK_NE:   push(OP_NE);   break;
    case TK_ADDX: push(OP_ADDX); break;
    case TK_ANDX: push(OP_ANDX); break;
    case TK_DIVX: push(OP_DIVX); break;
    case TK_MODX: push(OP_MODX); break;
    case TK_MULX: push(OP_MULX); break;
    case TK_ORX:  push(OP_ORX);  break;
    case TK_SUBX: push(OP_SUBX); break;
    case TK_XORX: push(OP_XORX); break;
    case TK_CATX: push(OP_CATX); break;
    case TK_POWX: push(OP_POWX); break;
    case TK_SHLX: push(OP_SHLX); break;
    case TK_SHRX: push(OP_SHRX); break;
    default: break;
    }
}

void c_prefix(code_t *c, int op) {
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

void c_postfix(code_t *c, int op) {
    switch (op) {
    case TK_INC: push(OP_POSTINC); break;
    case TK_DEC: push(OP_POSTDEC); break;
    default: break;
    }
}

#undef push
