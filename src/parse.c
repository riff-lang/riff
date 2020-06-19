#include <stdio.h>

#include "parse.h"
#include "types.h"

#define next(y) x_next(y->x)
#define push(b) c_push(y->c, b)

static void check(lexer_t *x, int tk) {
}

static int  expr(parser_t *y, int rbp);
static void nud(parser_t *y);
static void led(parser_t *y, int tk);
static int  lbp(int tk);

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

// Binary operators which directly map to a single VM instruction
static int bop(int tk) {
    switch (tk) {
    case '%':    return OP_MOD;
    case '&':    return OP_AND;
    case '(':    return OP_CALL; // ???
    case '*':    return OP_MUL;
    case '+':    return OP_ADD;
    case '-':    return OP_SUB;
    case '/':    return OP_DIV;
    case '<':    return OP_LT;
    case '=':    return OP_SET;
    case '>':    return OP_GT;
    case '^':    return OP_XOR;
    case '|':    return OP_OR;
    case TK_AND: return OP_LAND;
    case TK_CAT: return OP_CAT;
    case TK_EQ:  return OP_EQ;
    case TK_GE:  return OP_GE;
    case TK_LE:  return OP_LE;
    case TK_OR:  return OP_LOR;
    // case TK_POW: return OP_POW;
    case TK_SHL: return OP_SHL;
    case TK_SHR: return OP_SHR;
    default:     return 0;
    }
}

#define UBP 12

static void nud(parser_t *y) {
    int tk = y->x->tk.type;
    int u = uop(tk);
    if (u) {
        next(y);
        expr(y, UBP);
        push((uint8_t) uop);
        return;
    }
    switch (tk) {
    case '(':
        expr(y, 0);
        // Check for closing ')'?
        break;
    case TK_DEC: // Pre-decrement
    case TK_INC: // Pre-increment
    case TK_FLT: {
        value_t v;
        v.type = TYPE_FLT;
        v.u.f = y->x->tk.lexeme.f;
        push(OP_PUSHK);
        push(c_addk(y->c, &v));
    }
        break;
    case TK_ID:
        // Push var
    case TK_INT: {
        int_t i = y->x->tk.lexeme.i;

        // If 0 <= i <= 255, it can be used as an immediate operand,
        // without adding it to a chunk's constants table.
        if (i >= 0 && i <= 255) {
            // printf("nud (%d)\n", tk);
            push(OP_PUSHI);
            push((uint8_t) i);
        } else {
            value_t v;
            v.type = TYPE_INT;
            v.u.i = i;
            push(OP_PUSHK);
            push(c_addk(y->c, &v));
        }
    }
        break;
    case TK_STR:
        // Push constant
    default: break;
    }
}

#undef UBP

static void led(parser_t *y, int tk) {

    // Simple binary operation where the token maps to a single VM
    // instruction
    int b = bop(tk);
    if (b) {
        expr(y, lbp(tk));
        push(b);
        return;
    }
    switch (tk) {
    case TK_POW: expr(y, lbp(tk) - 1); push(OP_POW); break;
    case TK_NE:
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

// LBP for non-unary operators
static int lbp(int tk) {
    switch (tk) {
    case TK_POW:                  return 14;
    case '*': case '/': case '%': return 12;
    case '+': case '-':           return 11;
    case TK_CAT:                  return 10; // TODO
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

// def expression(rbp=0):
//     global token
//     t = token
//     token = next()
//     left = t.nud()
//     while rbp < token.lbp:
//         t = token
//         token = next()
//         left = t.led(left)
//     return left

static int expr(parser_t *y, int rbp) {
    nud(y);
    next(y);
    int op = y->x->tk.type;
    while (rbp < lbp(op)) {
        op = y->x->tk.type;
        next(y);
        led(y, op);
    }
    return 0;
    // Call nud() with initial token
    // Advance token stream
    // While rbp < lbp(token)
    //     Save token
    //     Advance token stream
    //     Call led(token) with saved token
    //
    // Return next token to be parsed?
}

// Evaluates whether a standalone expression had any side effects. If
// a standalone expression is given without any assignment, its result
// will be printed.
static void expr_stmt(parser_t *y) {
    expr(y, 0);
    if (y->a)
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
}

static void while_stmt(parser_t *y) {
}

static void stmt(parser_t *y) {
    int tt = y->x->tk.type;
    switch(tt) {
    // case ';': x_next(y->x); break;
    // case TK_FN: x_next(y->x); fn_def(y);
    case TK_FOR:
    case TK_IF:
    case TK_LOCAL:
    case TK_RETURN:
    case TK_WHILE:
    default: expr_stmt(y); break;
    }
    push(OP_RET);
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
    // c_init(y.c);
    return 0;
}

#undef next
