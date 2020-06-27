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
    if (c->cap <= c->n) {
        c->cap  = increase_cap(c->cap);
        c->code = realloc(c->code, c->cap);
    }
    c->code[c->n++] = b;
}

void c_free(code_t *c) {
    free(c);
    c_init(c);
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
        if (i == 0) {
            c_push(c, OP_PUSH0);
            return;
        } else if (i == 1) {
            c_push(c, OP_PUSH1);
            return;
        } else if (i >= 2 && i <= 255) {
            c_push(c, OP_PUSHI);
            c_push(c, (uint8_t) i);
            return;
        } else {
            v = v_newint(i);
        }
        break;
    }
    case TK_STR:
        v = v_newstr(tk->lexeme.s);
        break;
    default: break;
    }
    if (c->k.cap <= c->k.n) {
        c->k.cap = increase_cap(c->k.cap);
        c->k.v   = realloc(c->k.v, sizeof(val_t *) * c->k.cap);
    }

    c->k.v[c->k.n++] = v;
    if (c->k.n > 255)
        err(c, "Number of constants exceeds maximum");
    c_push(c, OP_PUSHK);
    c_push(c, (uint8_t) c->k.n - 1);
}

void c_symbol(code_t *c, token_t *tk) {
}
