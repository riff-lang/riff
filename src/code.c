#include <stdio.h>

#include "code.h"
#include "mem.h"

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

// Emits a jump instruction and returns the location of the byte to be
// patched
int c_prep_jump(code_t *c, int type) {
    if (type)
        c_push(c, OP_JMP8);
    else
        c_push(c, OP_JZ8);
    c_push(c, 0x00); // Reserve byte
    return c->n - 1;
}

void c_patch(code_t *c, int loc) {
    c->code[loc] = (uint8_t) c->n;
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
                c_push(c, OP_PUSHK);
                c_push(c, (uint8_t) i);
                return;
            }
            break;
        case TK_INT:
            if (c->k.v[i]->type != TYPE_INT)
                break;
            else if (tk->lexeme.i == c->k.v[i]->u.i) {
                c_push(c, OP_PUSHK);
                c_push(c, (uint8_t) i);
                return;
            }
            break;
        case TK_STR:
            if (c->k.v[i]->type != TYPE_STR)
                break;
            else if (tk->lexeme.s->hash == c->k.v[i]->u.s->hash) {
                c_push(c, OP_PUSHK);
                c_push(c, (uint8_t) i);
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
        case 0: c_push(c, OP_PUSH0); return;
        case 1: c_push(c, OP_PUSH1); return;
        case 2: c_push(c, OP_PUSH2); return;
        default:
            if (i >= 3 && i <= 255) {
            c_push(c, OP_PUSHI);
            c_push(c, (uint8_t) i);
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
    c_push(c, OP_PUSHK);
    c_push(c, (uint8_t) c->k.n - 1);
}

void c_symbol(code_t *c, token_t *tk) {
    // Search for existing symbol
    for (int i = 0; i < c->k.n; i++) {
        if (c->k.v[i]->type != TYPE_STR)
            break;
        else if (tk->lexeme.s->hash == c->k.v[i]->u.s->hash) {
            c_push(c, OP_PUSHS);
            c_push(c, (uint8_t) i);
            return;
        }
    }
    val_t *v;
    v = v_newstr(tk->lexeme.s);
    eval_resize(c->k.v, c->k.n, c->k.cap);
    c->k.v[c->k.n++] = v;
    if (c->k.n > 255)
        err(c, "Exceeded max number of unique literals");
    c_push(c, OP_PUSHS);
    c_push(c, (uint8_t) c->k.n - 1);
}

void c_infix(code_t *c, int op) {
    switch (op) {
    case '+':    c_push(c, OP_ADD); break;
    case '-':    c_push(c, OP_SUB); break;
    case '*':    c_push(c, OP_MUL); break;
    case '/':    c_push(c, OP_DIV); break;
    case '%':    c_push(c, OP_MOD); break;
    case '>':    c_push(c, OP_GT);  break;
    case '<':    c_push(c, OP_LT);  break;
    case '=':    c_push(c, OP_SET); break;
    case '&':    c_push(c, OP_AND); break;
    case '|':    c_push(c, OP_OR);  break;
    case '^':    c_push(c, OP_XOR); break;
    case TK_SHL: c_push(c, OP_SHL); break;
    case TK_SHR: c_push(c, OP_SHR); break;
    case TK_POW: c_push(c, OP_POW); break;
    case TK_CAT: c_push(c, OP_CAT); break;
    case TK_GE:  c_push(c, OP_GE);  break;
    case TK_LE:  c_push(c, OP_LE);  break;
    case TK_EQ:  c_push(c, OP_EQ);  break;
    case TK_NE:  c_push(c, OP_NE);  break;
    default: break;
    }
}

void c_prefix(code_t *c, int op) {
    switch (op) {
    case '!': c_push(c, OP_LNOT); break;
    case '#': c_push(c, OP_LEN);  break;
    case '+': c_push(c, OP_NUM);  break;
    case '-': c_push(c, OP_NEG);  break;
    case '~': c_push(c, OP_NOT);  break;
    case TK_INC: c_push(c, OP_PREINC); break;
    case TK_DEC: c_push(c, OP_PREDEC); break;
    default: break;
    }
}

void c_postfix(code_t *c, int op) {
    switch (op) {
    case TK_INC: c_push(c, OP_POSTINC); break;
    case TK_DEC: c_push(c, OP_POSTDEC); break;
    default: break;
    }
}
