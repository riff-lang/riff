#include <stdio.h>

#include "parse.h"
#include "val.h"

#define adv(y)  x_adv(y->x)
#define push(b) c_push(y->c, b)

static void err(parser_t *y, const char *msg) {
    fprintf(stderr, "line %d: %s\n", y->x->ln, msg);
    exit(1);
}

static void check(parser_t *y, int tk) {
    if (y->x->tk.kind != tk) {
        char msg[20];
        sprintf(msg, "Missing '%c'", tk);
        err(y, msg);
    }
}

// LBP for non-unary operators
static int lbp(int tk) {
    switch (tk) {
    case TK_POW:                  return 14;
    case '*': case '/': case '%': return 12;
    case '+': case '-':           return 11;
    case TK_CAT:                  return 10;
    case TK_SHL: case TK_SHR:     return 10;
    case '>': case '<':      
    case TK_GE: case TK_LE:       return 9;
    case TK_EQ: case TK_NE:       return 8;
    case '&':                     return 7;
    case '^':                     return 6;
    case '|':                     return 5;
    case TK_AND:                  return 4;
    case TK_OR:                   return 3;
    case '?': case ':':           return 2;
    case '=':
    case TK_ADD_ASSIGN: case TK_AND_ASSIGN: case TK_DIV_ASSIGN:
    case TK_MOD_ASSIGN: case TK_MUL_ASSIGN: case TK_OR_ASSIGN:
    case TK_SUB_ASSIGN: case TK_XOR_ASSIGN: case TK_CAT_ASSIGN:
    case TK_POW_ASSIGN: case TK_SHL_ASSIGN: case TK_SHR_ASSIGN:
        return 1;
    default: return 0;
    }
}

static int uop(int tk) {
    switch (tk) {
    case '!': return OP_LNOT;
    case '#': return OP_LEN;
    case '+': return OP_NUM;
    case '-': return OP_NEG;
    case '~': return OP_NOT;
    default:  return 0;
    }
}

// Left-associative binary operators which map to a single VM
// instruction
static int lbop(int tk) {
    switch (tk) {
    case '%':    return OP_MOD;
    case '&':    return OP_AND;
    case '(':    return OP_CALL; // ???
    case '*':    return OP_MUL;
    case '+':    return OP_ADD;
    case '-':    return OP_SUB;
    case '/':    return OP_DIV;
    case '<':    return OP_LT;
    case '>':    return OP_GT;
    case '^':    return OP_XOR;
    case '|':    return OP_OR;
    case TK_AND: return OP_LAND;
    case TK_CAT: return OP_CAT;
    case TK_EQ:  return OP_EQ;
    case TK_NE:  return OP_NE;
    case TK_GE:  return OP_GE;
    case TK_LE:  return OP_LE;
    case TK_OR:  return OP_LOR;
    case TK_SHL: return OP_SHL;
    case TK_SHR: return OP_SHR;
    default:     return 0;
    }
}

// Right-associative binary operators which map to a single VM
// instruction
static int rbop(int tk) {
    switch (tk) {
    case '=':    return OP_SET;
    case TK_POW: return OP_POW;
    default:     return 0;
    }
}

static int expr(parser_t *y, int rbp);

static void literal(parser_t *y) {
    c_pushk(y->c, &y->x->tk);
}

#define UBP 12

static void nud(parser_t *y) {
    int tk = y->x->tk.kind;
    int u = uop(tk);
    if (u) {
        adv(y);
        expr(y, UBP);
        push((uint8_t) u);
        return;
    }
    switch (tk) {
    case '(':
        adv(y);
        expr(y, 0);
        check(y, ')');
        return;
    case TK_DEC: // Pre-decrement
    case TK_INC: // Pre-increment
        break;
    case TK_FLT: case TK_INT: case TK_STR:
        literal(y);
        break;
    case TK_ID:
        // Push var
        break;
    default: break;
    }
}

#undef UBP

static void led(parser_t *y, int tk) {
    int b;
    if ((b = lbop(tk))) {
        expr(y, lbp(tk));
        push(b);
        return;
    } else if ((b = rbop(tk))) {
        expr(y, lbp(tk) - 1);
        push(b);
        return;
    }
    switch (tk) {
    case '?':
    case ':':
    case '(': // Function call
    case '[': // Lookup/indexing
    case TK_ADD_ASSIGN:
    case TK_AND_ASSIGN:
    case TK_DIV_ASSIGN:
    case TK_MOD_ASSIGN:
    case TK_MUL_ASSIGN:
    case TK_OR_ASSIGN:
    case TK_SUB_ASSIGN:
    case TK_XOR_ASSIGN:
    case TK_CAT_ASSIGN:
    case TK_POW_ASSIGN:
    case TK_SHL_ASSIGN:
    case TK_SHR_ASSIGN:
    default: break;
    }
}

static int expr(parser_t *y, int rbp) {
    nud(y);
    int op = y->x->tk.kind;

    // Hack for unary
    if (!lbp(op)) {
        adv(y);
        op = y->x->tk.kind;
    }
    while (rbp < lbp(op) && !adv(y)) {
        led(y, op);
        op = y->x->tk.kind;
    }
    return 0;
}

// Standalone expressions
// Evaluates whether a standalone expression had any side effects. If
// a standalone expression is given without any assignment, its result
// will be printed.
static void expr_stmt(parser_t *y) {
    expr(y, 0);
    if (!y->a)
        push(OP_PRINT);
    y->a = 0;
}

static void fn_def(parser_t *y) {
}

static void for_stmt(parser_t *y) {
}

static void if_stmt(parser_t *y) {
}

static void ret_stmt(parser_t *y) {
    expr(y, 0);
    push(OP_RET);
}

static void while_stmt(parser_t *y) {
}

static void stmt(parser_t *y) {
    switch(y->x->tk.kind) {
    case ';':       adv(y);                break;
    case TK_FN:     adv(y); fn_def(y);     break;
    case TK_FOR:    adv(y); for_stmt(y);   break;
    case TK_IF:     adv(y); if_stmt(y);    break;
    case TK_LOCAL: break;
    case TK_RETURN: adv(y); ret_stmt(y);   break;
    case TK_WHILE:  adv(y); while_stmt(y); break;
    default: expr_stmt(y);                 break;
    }
}

static void stmt_list(parser_t *y) {
}

int y_compile(const char *src, chunk_t *c) {
    parser_t y;
    lexer_t x;
    x_init(&x, src);
    y.x = &x;
    y.c = c;
    stmt(&y);
    return 0;
}

#undef adv
