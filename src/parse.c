#include <stdio.h>

#include "mem.h"
#include "parse.h"

#define adv       x_adv(y->x)
#define peek      x_peek(y->x)
#define push(b)   c_push(y->c, b)
#define set(f)    y->f    = 1;
#define unset(f)  y->f    = 0;
#define unset_all y->lhs  = 0; \
                  y->argx = 0; \
                  y->ax   = 0; \
                  y->ox   = 0; \
                  y->px   = 0; \
                  y->rx   = 0;

// TODO Hardcoded logic for valid "follow" tokens should be cleaned
// up
// TODO Syntax error handling; parser should do as much heavy lifting
// as possible

static void err(rf_parser *y, const char *msg) {
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

static int const_follow_ok(int tk) {
    return !(is_incdec(tk) || tk == '(');
}

static void check(rf_parser *y, int tk, const char *msg) {
    if (y->x->tk.kind != tk)
        err(y, msg);
}

static void consume(rf_parser *y, int tk, const char *msg) {
    check(y, tk, msg);
    adv;
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
    case '&':                     return 9;
    case '^':                     return 8;
    case '|':                     return 7;
    case '>': case '<':      
    case TK_GE: case TK_LE:       return 6;
    case TK_EQ: case TK_NE:       return 5;
    case TK_AND:                  return 4;
    case TK_OR:                   return 3;
    case '?':                     return 2;
    default:
        return is_asgmt(tk);
    }
}

// Standard unary ops with standard unary binding power
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

static int expr(rf_parser *y, int rbp);

static void literal(rf_parser *y) {
    c_constant(y->c, &y->x->tk);
    adv;
    // Assert no assignment appears following a constant literal
    if (!y->argx && is_asgmt(y->x->tk.kind))
        err(y, "Attempt to assign to constant value");
}

static int resolve_local(rf_parser *y, rf_str *s) {
    if (!y->nlcl)
        return -1;

    // If LHS hasn't been evaluated or we're not inside a local
    // statement, search through all active locals. Otherwise, we're
    // evaluating the RHS of a newly declared local and should exclude
    // it from its own initialization.
    //
    // This allows for statements like `local x = x` where the LHS `x`
    // refers to a newly-declared local and the RHS refers to any `x`
    // previously in scope.
    int i = (!y->lhs || !y->lx) ? y->nlcl - 1 : y->nlcl - 2;

    for (; i >= 0; --i) {
        if (y->lcl[i].id->hash == s->hash &&
            y->lcl[i].d <= y->ld)
            return i;
    }
    return -1;
}

static void identifier(rf_parser *y) {
    int scope = resolve_local(y, y->x->tk.lexeme.s);

    // If symbol succeeds a prefix ++/-- operation, signal codegen
    // to push the address of the symbol
    if (y->rx) {
        if (scope >= 0)
            c_local(y->c, scope, 1);
        else
            c_global(y->c, &y->x->tk, 1);
        adv;
        return;
    }

    // Else, peek at the next token. If the following token is
    // assignment, ++/--, or '[', signal codegen to push the address
    // of the symbol. Otherwise, push the value itself.
    peek;
    if (is_incdec(y->x->la.kind) ||
        is_asgmt(y->x->la.kind)  ||
        y->x->la.kind == '[') {
        if (scope >= 0)
            c_local(y->c, scope, 1);
        else
            c_global(y->c, &y->x->tk, 1);
    } else {
        if (scope >= 0)
            c_local(y->c, scope, 0);
        else
            c_global(y->c, &y->x->tk, 0);
    }
    adv;
}

static int conditional(rf_parser *y) {
    int e, l1, l2;

    // x ?: y
    if (y->x->tk.kind == ':') {
        adv;
        l1 = c_prep_jump(y->c, XJNZ);
        e = expr(y, 0);
        c_patch(y->c, l1);
    }

    // x ? y : z
    else {
        l1 = c_prep_jump(y->c, JZ);
        expr(y, 0);
        l2 = c_prep_jump(y->c, JMP);
        c_patch(y->c, l1);
        consume(y, ':', "Expected ':'");
        e = expr(y, 0);
        c_patch(y->c, l2);
    }
    return e;
}

// Short-circuiting logical operations (&&, ||)
static void logical(rf_parser *y, int tk) {
    push(OP_TEST);
    int l1 = c_prep_jump(y->c, tk == TK_OR ? XJNZ : XJZ);
    expr(y, lbp(tk));
    push(OP_TEST);
    c_patch(y->c, l1);
}

// Retrieve index of a set (string/array, but works for numbers as well)
static void set_index(rf_parser *y) {
    y->idx++;
    int rx = y->rx; // Save flag
    unset(rx);      // Unset
    expr(y, 0);
    consume(y, ']', "Expected ']'");
    if (rx || is_asgmt(y->x->tk.kind) || is_incdec(y->x->tk.kind))
        push(OP_IDXA);
    else
        push(OP_IDXV);
    y->rx = rx;     // Restore flag
    y->idx--;
}

static int expr_list(rf_parser *y, int c) {
    int n = 0;
    while (y->x->tk.kind != c) {
        int rx = y->rx; // Save flag
        unset(rx);      // Unset
        expr(y, 0);
        y->rx = rx;     // Restore flag
        ++n;
        if (y->x->tk.kind == ',')
            adv;
        else
            break;
    }
    return n;
}

static void call(rf_parser *y) {
    int n = expr_list(y, ')');
    consume(y, ')', "Expected ')'");
    push(OP_CALL);
    push((uint8_t) n);
}

// TODO Support arbitrary indexing a la C99 designators
static void array(rf_parser *y) {
    int n = expr_list(y, '}');
    consume(y, '}', "Expected '}'");
    c_array(y->c, n);
}

static int nud(rf_parser *y) {
    int tk = y->x->tk.kind;
    if (uop(tk)) {
        set(ox);
        adv;
        expr(y, 12);
        c_prefix(y->c, tk);
        return 0;
    }
    switch (tk) {
    case '$':
        adv;
        set(argx);
        expr(y, 16);
        if (y->rx                   ||
            is_asgmt(y->x->tk.kind) ||
            is_incdec(y->x->tk.kind)) {
            set(ax);
            push(OP_ARGA);
        } else
            push(OP_ARGV);
        unset(argx);
        if (!y->lhs) set(lhs);
        break;
    case '(':
        adv;
        expr(y, 0);
        consume(y, ')', "Expected ')'");
        if (!y->argx && is_asgmt(y->x->tk.kind))
            err(y, "Invalid operator following expr");
        return ')';
    case '{':
        adv;
        array(y);
        break;
    case TK_INC: case TK_DEC:
        if (adv)
            err(y, "Expected symbol following prefix ++/--");
        if (!(y->x->tk.kind == TK_ID ||
              y->x->tk.kind == '$'))
            err(y, "Unexpected symbol following prefix ++/--");
        set(rx);
        expr(y, 14);
        c_prefix(y->c, tk);
        if (!y->idx && !y->lhs) {
            set(px);
            set(lhs);
        }
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
        else
            err(y, "Unexpected symbol");
        break;
    }
    return tk;
}

static int led(rf_parser *y, int p, int tk) {
    switch (tk) {
    case TK_INC: case TK_DEC:
        if (is_const(p) || y->rx)
            return p;
        if (!y->idx && !y->lhs) {
            set(px);
            set(lhs);
        }
        c_postfix(y->c, tk);
        adv;
        p = y->x->tk.kind;
        unset(rx);
        break;
    case '?':
        unset(rx);
        if (!y->ox) set(ox);
        adv;
        p = conditional(y);
        break;
    case TK_AND: case TK_OR:
        unset(rx);
        if (!y->ox) set(ox);
        adv;
        logical(y, tk);
        p = y->x->tk.kind;
        break;
    case '[':
        adv;
        set_index(y);
        break;
    case '(':
        if (!y->ox) set(ox);
        adv;
        call(y);
        break;
    default:
        if (lbop(tk) || rbop(tk)) {
            if (!y->idx && !y->ax && !y->ox) {
                set(lhs);
                if (is_asgmt(tk)) {
                    set(ax);
                } else {
                    set(ox);
                }
            } else if (!(y->argx || (is_asgmt(tk) && y->ox))) {
                err(y, "Syntax error");
            }
            unset(rx);
            adv;
            p = expr(y, lbop(tk) ? lbp(tk) : lbp(tk) - 1);
            c_infix(y->c, tk);
        }
        break;
    }
    return p;
}

static int expr(rf_parser *y, int rbp) {
    int p  = nud(y);
    int tk = y->x->tk.kind;

    while (rbp < lbp(tk)) {

        // Return early if previous nud was a constant and succeeding
        // token would make for an invalid expression. This allows the
        // succeeding token to be parsed as a new expression instead
        // of throwing an error
        if ((is_const(p) || is_incdec(p) || p == ')') && !const_follow_ok(tk))
            return p;

        // Return if the previous nud had a prefix ++/-- attached and
        // the following token is also ++/--. This would be an
        // invalid operation; this allows the ++/-- to be parsed as
        // a prefix op for the following expression.
        if (y->rx && is_incdec(tk))
            return p;

        p  = led(y, p, tk);
        tk = y->x->tk.kind;
    }
    return p;
}

// Standalone expressions
static void expr_stmt(rf_parser *y) {
    unset_all;
    int n = expr_list(y, 0);

    // Implicit printing conditions
    // 1. Leftmost expr is not being assigned to
    // 2. Leftmost expr is not being incremented/decremented UNLESS
    //    accompanied by an otherwise typical operation, e.g.
    //    arithmetic
    //
    // Examples:
    //   a = 1       (Do not print)
    //   a << 2      (Print)
    //   a++         (Do not print)
    //   a = b--     (Do not print)
    //   ++a == 1    (Print)
    //   x++ ? y : z (Print)
    //   ++a[b]      (Do not print)
    //   a[++b]      (Print)
    if (n > 1 || (!y->ax && (!y->px || y->ox)))
        c_print(y->c, n);
    else
        push(OP_POP);
}

static void stmt_list(rf_parser *);
static void stmt(rf_parser *);

static void break_stmt(rf_parser *y) {
    if (!y->ld)
        err(y, "break statement outside of loop");
    // Reserve a forward jump
    p_list *p = y->brk;
    eval_resize(p->l, p->n, p->cap);
    p->l[p->n++] = c_prep_jump(y->c, JMP);
}

static void cont_stmt(rf_parser *y) {
    if (!y->ld)
        err(y, "continue statement outside of loop");
    // Reserve a forward jump
    p_list *p = y->cont;
    eval_resize(p->l, p->n, p->cap);
    p->l[p->n++] = c_prep_jump(y->c, JMP);
}

static void p_init(p_list *p) {
    p->n   = 0;
    p->cap = 0;
    p->l   = NULL;
}

// After exiting scope, "pop" local variables no longer in scope by
// decrementing y->nlcl
static void pop_locals(rf_parser *y) {
    if (!y->nlcl) return;

    int count = 0;
    // Remove !y->ld in terminating condition if top-level locals
    // aren't meant to be popped
    for (int i = y->nlcl - 1; i >= 0 && (!y->ld || y->lcl[i].d > y->ld); --i) {
        y->nlcl--;
        free(y->lcl[i].id);
        ++count;
    }
    if (!count)
        return;
    else if (count == 1)
        push(OP_POP);
    else {
        push(OP_POPI);
        push((uint8_t) count);
    }
}

static void enter_loop(rf_parser *y, p_list *b, p_list *c) {
    y->ld++;
    p_init(b);
    p_init(c);
    y->brk  = b;
    y->cont = c;
}

static void exit_loop(rf_parser *y, p_list *ob, p_list *oc, p_list *nb, p_list *nc) {
    pop_locals(y);
    y->ld--;
    y->brk  = ob;
    y->cont = oc;
    if (nb->n) free(nb->l);
    if (nc->n) free(nc->l);
}
static void do_stmt(rf_parser *y) {
    p_list *r_brk  = y->brk;
    p_list *r_cont = y->cont;
    p_list b, c;
    enter_loop(y, &b, &c);
    int l1 = y->c->n;
    if (y->x->tk.kind == '{') {
        adv;
        stmt_list(y);
        consume(y, '}', "Expected '}'");
    } else {
        stmt(y);
    }
    // Patch continue stmts
    for (int i = 0; i < c.n; i++) {
        c_patch(y->c, c.l[i]);
    }
    consume(y, TK_WHILE, "Expected 'while' condition after 'do' block");
    expr(y, 0);
    c_jump(y->c, JNZ, l1);
    // Patch break stmts
    for (int i = 0; i < b.n; i++) {
        c_patch(y->c, b.l[i]);
    }
    exit_loop(y, r_brk, r_cont, &b, &c);
}

static void exit_stmt(rf_parser *y) {
    push(OP_EXIT);
}

// TODO
static void fn_def(rf_parser *y) {
}

// TODO
static void for_stmt(rf_parser *y) {
}

static void if_stmt(rf_parser *y) {
    expr(y, 0);
    int l1, l2;
    y->ld++;
    l1 = c_prep_jump(y->c, JZ);
    if (y->x->tk.kind == '{') {
        adv;
        stmt_list(y);
        consume(y, '}', "Expected '}'");
    } else {
        stmt(y);
    }
    if (y->x->tk.kind == TK_ELSE) {
        adv;
        l2 = c_prep_jump(y->c, JMP);
        c_patch(y->c, l1);
        if (y->x->tk.kind == '{') {
            adv;
            stmt_list(y);
            consume(y, '}', "Expected '}'");
        } else {
            stmt(y);
        }
        c_patch(y->c, l2);
    } else {
        c_patch(y->c, l1);
    }
    y->ld--;
    pop_locals(y);
}

static void local_stmt(rf_parser *y) {
    while (1) {
        unset_all;
        rf_str *id;
        if (y->x->tk.kind != TK_ID) {
            peek;
            if (y->x->la.kind != TK_ID)
                err(y, "local declaration requires valid identifier");
            id = y->x->la.lexeme.s;
        } else {
            id = y->x->tk.lexeme.s;
        }

        int idx = resolve_local(y, id);

        // If the identifier doesn't already exist as a local at the
        // current scope, add a new local
        if (idx < 0 || y->lcl[idx].d != y->ld) {
            set(lx);    // Only set for newly-declared locals
            if (y->lcap <= y->nlcl) {
                y->lcap = y->lcap < 8 ? 8 : y->lcap * 2;
                y->lcl = realloc(y->lcl, sizeof(local) * y->lcap);
            }
            y->lcl[y->nlcl].id = s_newstr(id->str, id->l, 1);
            y->lcl[y->nlcl].d  = y->ld;
            switch (y->nlcl) {
            case 0: push(OP_LCL0); break;
            case 1: push(OP_LCL1); break;
            case 2: push(OP_LCL2); break;
            default:
                push(OP_LCL);
                push((uint8_t) y->nlcl);
                break;
            }
            y->nlcl++;
        }
        expr(y, 0);
        push(OP_POP);

        unset(lx);
        if (y->x->tk.kind == ',')
            adv;
        else
            break;
    }
}

static void print_stmt(rf_parser *y) {
    int paren = 0;
    if (y->x->tk.kind == '(') { // Parenthesized expr list?
        adv;
        paren = ')';
    }
    const char *p = y->x->p;    // Save pointer
    int n = expr_list(y, paren);
    if (p == y->x->p)           // No expression parsed
        err(y, "Expected expression following `print`");
    c_print(y->c, n);
    if (paren)
        consume(y, ')', "Expected ')'");
}

static void ret_stmt(rf_parser *y) {
    const char *p = y->x->p; // Save pointer
    expr(y, 0);
    if (p == y->x->p)        // No expression parsed
        push(OP_RET);
    else
        push(OP_RET1);
}

static void while_stmt(rf_parser *y) {
    p_list *r_brk  = y->brk;
    p_list *r_cont = y->cont;
    p_list b, c;
    int l1, l2;
    l1 = y->c->n;
    expr(y, 0);
    l2 = c_prep_jump(y->c, JZ);
    enter_loop(y, &b, &c);
    if (y->x->tk.kind == '{') {
        adv;
        stmt_list(y);
        consume(y, '}', "Expected '}'");
    } else {
        stmt(y);
    }
    // Patch continue stmts
    for (int i = 0; i < c.n; i++) {
        c_patch(y->c, c.l[i]);
    }
    c_jump(y->c, JMP, l1);
    c_patch(y->c, l2);
    // Patch break stmts
    for (int i = 0; i < b.n; i++) {
        c_patch(y->c, b.l[i]);
    }
    exit_loop(y, r_brk, r_cont, &b, &c);
}

static void stmt(rf_parser *y) {
    switch (y->x->tk.kind) {
    case ';':       adv;                break;
    case TK_BREAK:  adv; break_stmt(y); break;
    case TK_CONT:   adv; cont_stmt(y);  break;
    case TK_DO:     adv; do_stmt(y);    break;
    case TK_EXIT:   adv; exit_stmt(y);  break;
    case TK_FN:     adv; fn_def(y);     break; // TODO
    case TK_FOR:    adv; for_stmt(y);   break; // TODO
    case TK_IF:     adv; if_stmt(y);    break;
    case TK_LOCAL:  adv; local_stmt(y); break;
    case TK_PRINT:  adv; print_stmt(y); break;
    case TK_RETURN: adv; ret_stmt(y);   break;
    case TK_WHILE:  adv; while_stmt(y); break;
    default:             expr_stmt(y);  break;
    }
}

static void stmt_list(rf_parser *y) {
    while (!(y->x->tk.kind == TK_EOI ||
             y->x->tk.kind == '}'))
        stmt(y);
}

static void y_init(rf_parser *y, const char *src) {
    unset_all;
    unset(lx);
    x_init(y->x, src);

    y->nlcl = 0;
    y->lcap = 0;
    y->lcl  = NULL;

    y->idx  = 0;
    y->ld   = 0;
    y->brk  = NULL;
    y->cont = NULL;
}

int y_compile(const char *src, rf_code *c) {
    rf_parser y;
    rf_lexer x;
    y.x = &x;
    y.c = c;
    y_init(&y, src);
    stmt_list(&y);
    pop_locals(&y);
    c_push(c, OP_RET);
    x_free(&x);
    return 0;
}

#undef adv
#undef peek
#undef push
#undef set
#undef unset
#undef unset_all
