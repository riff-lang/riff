#include <stdio.h>

#include "parse.h"

#define adv(y)  x_adv(y->x)
#define push(b) c_push(y->c, b)

// General TODO: hardcoded logic for valid "follow" tokens should be
// cleaned up

static void err(parser_t *y, const char *msg) {
    fprintf(stderr, "line %d: %s\n", y->x->ln, msg);
    exit(1);
}

static int is_asgmt(int tk) {
    return tk == '=' || (tk >= TK_ADDX && tk <= TK_XORX);
}

static int is_const(int tk) {
    return tk == TK_INT || tk == TK_FLT || tk == TK_STR;
}

static int is_incdec(int tk) {
    return tk == TK_INC || tk == TK_DEC;
}

static void check(parser_t *y, int tk, const char *msg) {
    if (y->x->tk.kind != tk)
        err(y, msg);
}

static void consume(parser_t *y, int tk, const char *msg) {
    check(y, tk, msg);
    adv(y);
}

// LBP for non-prefix operators
static int lbp(int tk) {
    switch (tk) {
    case '(': case'[':
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
    default:
        return is_asgmt(tk);
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
    return is_asgmt(tk) || tk == TK_POW;
}

static int expr(parser_t *y, int rbp);

static void literal(parser_t *y) {
    c_constant(y->c, &y->x->tk);
    adv(y);
    // Assert no assignment appears following a constant literal
    if (is_asgmt(y->x->tk.kind))
        err(y, "Attempt to assign to constant value");
}

static void identifier(parser_t *y) {
    c_symbol(y->c, &y->x->tk);
    adv(y);
}

static void conditional(parser_t *y) {
    int l1, l2;

    // x ?: y
    if (y->x->tk.kind == ':') {
        adv(y);
        l1 = c_prep_jump(y->c, XJNZ);
        expr(y, 0);
        c_patch(y->c, l1);
    }

    // x ? y : z
    else {
        l1 = c_prep_jump(y->c, JZ);
        expr(y, 0);
        l2 = c_prep_jump(y->c, JMP);
        c_patch(y->c, l1);
        consume(y, ':', "Expected ':'");
        expr(y, 0);
        c_patch(y->c, l2);
    }
}

// Short-circuiting logical operations (&&, ||)
static void logical(parser_t *y, int tk) {
    push(OP_TEST);
    int l1 = c_prep_jump(y->c, tk == TK_OR ? XJNZ : XJZ);
    expr(y, lbp(tk));
    push(OP_TEST);
    c_patch(y->c, l1);
}

static int nud(parser_t *y) {
    int tk = y->x->tk.kind;
    if (uop(tk)) {
        adv(y);
        expr(y, 12);
        c_prefix(y->c, tk);
        return 0;
    }
    switch (tk) {
    case '(':
        adv(y);
        expr(y, 0);
        consume(y, ')', "Expected ')'");
        if (is_asgmt(y->x->tk.kind))
            err(y, "Invalid operator following expr");
        return ')';
    case '{': // TODO New array/table
        break;
    case TK_INC: case TK_DEC:
        adv(y);
        if (is_const(y->x->tk.kind))
            err(y, "Unexpected constant following prefix increment or decrement");
        expr(y, 14);
        c_prefix(y->c, tk);
        y->pf = 0;
        break;
    case TK_FLT: case TK_INT: case TK_STR:
        literal(y);
        break;
    case TK_ID:
        identifier(y);
        break;
    default:
        // TODO Handle invalid nuds
        if (lbop(tk) || rbop(tk))
            err(y, "Invalid start of expression");
        break;
    }
    return tk;
}

static int led(parser_t *y, int tk) {
    int p = 0;
    switch (tk) {
    case TK_INC: case TK_DEC:
        y->pf = 0;
        c_postfix(y->c, tk);
        adv(y);
        p = y->x->tk.kind;
        break;
    case '?':
        adv(y);
        p = y->x->tk.kind;
        conditional(y);
        break;
    case TK_AND: case TK_OR:
        adv(y);
        p = y->x->tk.kind;
        logical(y, tk);
        break;
    case '[':
        adv(y);
        expr(y, 0);
        consume(y, ']', "Expected ']'");
        push(OP_GET);
        break;
    case '(':
        break;
    default:
        if (lbop(tk) || rbop(tk)) {
            if (is_asgmt(tk)) y->pf = 0;
            adv(y);
            p = y->x->tk.kind;
            expr(y, lbop(tk) ? lbp(tk) : lbp(tk) - 1);
            c_infix(y->c, tk);
        }
        break;
    }
    return p;
}

static int expr(parser_t *y, int rbp) {
    int p  = nud(y);
    int tk = y->x->tk.kind;
    while (rbp < lbp(tk)) {
        // Return early if previous nud was a constant;
        // evaluate inc/dec as prefix op for next expr
        if (is_const(p) && is_incdec(tk))
            return 1;
        p  = led(y, tk);
        tk = y->x->tk.kind;
    }
    return 0;
}

// Standalone expressions
// Evaluates whether a standalone expression had any side effects.
// If a standalone expression is given without any assignment, its
// result will be printed.
static void expr_stmt(parser_t *y) {
    expr(y, 0);
    if (y->pf)
        push(OP_PRINT);
    y->pf = 1;
}

static void stmt_list(parser_t *);
static void stmt(parser_t *);

static void do_stmt(parser_t *y) {
    int l1 = y->c->n;
    if (y->x->tk.kind == '{') {
        adv(y);
        stmt_list(y);
        consume(y, '}', "Expected '}'");
    } else {
        stmt(y);
    }
    consume(y, TK_WHILE, "Expected 'while' condition after 'do' block");
    expr(y, 0);
    c_jump(y->c, JNZ, l1);
}

static void exit_stmt(parser_t *y) {
    push(OP_EXIT);
}

static void fn_def(parser_t *y) {
}

static void for_stmt(parser_t *y) {
}

static void if_stmt(parser_t *y) {
    expr(y, 0);
    int l1, l2;
    l1 = c_prep_jump(y->c, JZ);
    if (y->x->tk.kind == '{') {
        adv(y);
        stmt_list(y);
        consume(y, '}', "Expected '}'");
    } else {
        stmt(y);
    }
    if (y->x->tk.kind == TK_ELSE) {
        adv(y);
        l2 = c_prep_jump(y->c, JMP);
        c_patch(y->c, l1);
        if (y->x->tk.kind == '{') {
            adv(y);
            stmt_list(y);
            consume(y, '}', "Expected '}'");
        } else {
            stmt(y);
        }
        c_patch(y->c, l2);
    } else {
        c_patch(y->c, l1);
    }
}

static void print_stmt(parser_t *y) {
    const char *p = y->x->p; // Save pointer
    expr(y, 0);
    if (p == y->x->p)        // No expression parsed
        err(y, "Expected expression following `print`");
    push(OP_PRINT);
}

static void ret_stmt(parser_t *y) {
    const char *p = y->x->p; // Save pointer
    expr(y, 0);
    if (p == y->x->p)        // No expression parsed
        push(OP_RET);
    else
        push(OP_RET1);
}

static void while_stmt(parser_t *y) {
    int l1, l2;
    l1 = y->c->n;
    expr(y, 0);
    l2 = c_prep_jump(y->c, JZ);
    if (y->x->tk.kind == '{') {
        adv(y);
        stmt_list(y);
        consume(y, '}', "Expected '}'");
    } else {
        stmt(y);
    }
    c_jump(y->c, JMP, l1);
    c_patch(y->c, l2);
}

static void stmt(parser_t *y) {
    switch (y->x->tk.kind) {
    case ';':       adv(y);                break;
    case TK_DO:     adv(y); do_stmt(y);    break;
    case TK_EXIT:   adv(y); exit_stmt(y);  break;
    case TK_FN:     adv(y); fn_def(y);     break;
    case TK_FOR:    adv(y); for_stmt(y);   break;
    case TK_IF:     adv(y); if_stmt(y);    break;
    case TK_LOCAL: break;
    case TK_PRINT:  adv(y); print_stmt(y); break;
    case TK_RETURN: adv(y); ret_stmt(y);   break;
    case TK_WHILE:  adv(y); while_stmt(y); break;
    default: expr_stmt(y);                 break;
    }
}

static void stmt_list(parser_t *y) {
    while (!(y->x->tk.kind == TK_EOI ||
             y->x->tk.kind == '}'))
        stmt(y);
}

static void y_init(parser_t *y, const char *src) {
    y->pf = 1;
    x_init(y->x, src);
}

int y_compile(const char *src, code_t *c) {
    parser_t y;
    lexer_t x;
    y.x = &x;
    y.c = c;
    y_init(&y, src);
    stmt_list(&y);
    c_push(c, OP_RET);
    return 0;
}

#undef adv
#undef push
