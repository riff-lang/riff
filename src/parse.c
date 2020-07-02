#include <stdio.h>

#include "parse.h"
#include "val.h"

#define adv(y)  x_adv(y->x)
#define push(b) c_push(y->c, b)

static int is_assignment(int tk) {
    return tk == '=' || (tk >= TK_ADD_ASSIGN && tk <= TK_XOR_ASSIGN);
}
static void err(parser_t *y, const char *msg) {
    fprintf(stderr, "line %d: %s\n", y->x->ln, msg);
    exit(1);
}

static void check_char(parser_t *y, int tk) {
    if (y->x->tk.kind != tk) {
        char msg[20];
        sprintf(msg, "Missing '%c'", tk);
        err(y, msg);
    }
}

// LBP for non-prefix operators
static int lbp(int tk) {
    switch (tk) {
    case TK_INC: case TK_DEC:     return 15;
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
    case '?':                     return 2;
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
    return tk == '!' || tk == '#' || tk == '+' ||
           tk == '-' || tk == '~';
}

static int lbop(int tk) {
    return tk == '%' || tk == '&' || tk == '(' || tk == '*' ||
           tk == '+' || tk == '-' || tk == '/' || tk == '<' ||
           tk == '>' || tk == '^' || tk == '|' || tk == '[' ||
           tk == TK_AND || tk == TK_EQ  || tk == TK_NE ||
           tk == TK_GE  || tk == TK_LE  || tk == TK_OR ||
           tk == TK_SHL || tk == TK_SHR || tk == TK_CAT;
}

static int rbop(int tk) {
    return tk == '=' || tk == TK_POW ||
           (tk >= TK_ADD_ASSIGN && tk <= TK_XOR_ASSIGN);
}

static int expr(parser_t *y, int rbp);

static void literal(parser_t *y) {
    c_constant(y->c, &y->x->tk);
}

static void identifier(parser_t *y) {
    c_symbol(y->c, &y->x->tk);
}

static void conditional(parser_t *y) {
    int l1, l2;
    if (y->x->tk.kind == ':') {
        adv(y);
        l1 = c_prep_jump(y->c, 1);
        push(OP_POP); // TODO JZ pops from stack but not JNZ??
        expr(y, 0);
        c_patch(y->c, l1);
    } else {
        l1 = c_prep_jump(y->c, 0);
        expr(y, 0);
        l2 = c_prep_jump(y->c, 2);
        c_patch(y->c, l1);
        check_char(y, ':');
        adv(y);
        expr(y, 0);
        c_patch(y->c, l2);
    }
}

static void nud(parser_t *y) {
    int tk = y->x->tk.kind;
    if (uop(tk)) {
        adv(y);
        expr(y, 12);
        c_prefix(y->c, tk);
        return;
    }
    switch (tk) {
    case '(':
        adv(y);
        expr(y, 0);
        check_char(y, ')');
        return;
    case '{': // New array/table?
    case TK_INC: case TK_DEC:
        adv(y);
        expr(y, 14);
        c_prefix(y->c, tk);
        return;
    case TK_FLT: case TK_INT: case TK_STR:
        literal(y);
        break;
    case TK_ID:
        identifier(y);
        break;
    default: break;
    }
}

static void led(parser_t *y, int tk) {
    if (tk == '?') {
        conditional(y);
    } else if (lbop(tk) || rbop(tk)) {
        if (is_assignment(tk)) y->a = 1;
        expr(y, lbop(tk) ? lbp(tk) : lbp(tk) - 1);
        c_infix(y->c, tk);
    }
}

// TODO Hacky logic for prefix and postfix ops; requires cleanup
static int expr(parser_t *y, int rbp) {
    nud(y);
    int op = y->x->tk.kind;

    // Hack for unary
    if (!lbp(op)) {
        adv(y);
        op = y->x->tk.kind;
    }
    if (op == TK_INC || op == TK_DEC) {
        c_postfix(y->c, op);
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

static void stmt_list(parser_t *);
static void stmt(parser_t *);

static void fn_def(parser_t *y) {
}

static void for_stmt(parser_t *y) {
}

static void if_stmt(parser_t *y) {
    expr(y, 0);
    int l1 = c_prep_jump(y->c, 0);
    if (y->x->tk.kind == '{') {
        stmt_list(y);
        check_char(y, '}');
    } else
        stmt(y);
    c_patch(y->c, l1);
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
    while (y->x->tk.kind != TK_EOI)
        stmt(y);
    push(OP_RET0);
}

static void y_init(parser_t *y, const char *src) {
    y->a = 0;
    x_init(y->x, src);
}

int y_compile(const char *src, code_t *c) {
    parser_t y;
    lexer_t x;
    y.x = &x;
    y.c = c;
    y_init(&y, src);
    stmt_list(&y);
    return 0;
}

#undef adv
#undef push
