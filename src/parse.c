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
                  y->rx   = 0; \
                  y->retx = 0;

// TODO Hardcoded logic for valid "follow" tokens should be cleaned
// up
// TODO Syntax error handling; parser should do as much heavy lifting
// as possible

static int  expr(rf_parser *y, int rbp);
static void stmt_list(rf_parser *);
static void stmt(rf_parser *);
static void add_local(rf_parser *, rf_str *);
static int  compile_fn(rf_parser *y);
static void y_init(rf_parser *y);

static void err(rf_parser *y, const char *msg) {
    fprintf(stderr, "riff: [compile] line %d: %s\n", y->x->ln, msg);
    exit(1);
}

static int is_asgmt(int tk) {
    return tk == '=' || (tk >= TK_ADDX && tk <= TK_XORX);
}

static int is_const(int tk) {
    return tk == TK_NULL || tk == TK_FLT || tk == TK_INT || tk == TK_STR;
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
    case TK_INC: case TK_DEC:     return 16;
    case TK_POW:                  return 15;
    case '*': case '/': case '%': return 13;
    case '+': case '-':           return 12;
    case TK_CAT:                  return 11;
    case TK_SHL: case TK_SHR:     return 11;
    case '&':                     return 10;
    case '^':                     return 9;
    case '|':                     return 8;
    case '>': case '<':      
    case TK_GE: case TK_LE:       return 7;
    case TK_EQ: case TK_NE:       return 6;
    case TK_AND:                  return 5;
    case TK_OR:                   return 4;
    case TK_DOTS:                 return 3;
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

static void literal(rf_parser *y) {
    c_constant(y->c, &y->x->tk);
    adv;
    // Assert no assignment appears following a constant literal
    if (!y->argx && is_asgmt(y->x->tk.kind))
        err(y, "attempt to assign to constant value");
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

    // If parsing the expression `z` in a 'for' loop declaration e.g.
    //
    //   for x,y in z {...}
    //
    // Decrement `i` again to ignore both `x` and `y` for reasons
    // similar to the above comments. Note that fx is not set for
    // 'for' loops in the form of `for x in y`; the lx flag is
    // sufficient
    i -= !!y->fx;

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
    if (!y->argx &&
            (is_incdec(y->x->la.kind) ||
             is_asgmt(y->x->la.kind)  ||
             y->x->la.kind == '[')) {
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
        consume(y, ':', "expected ':' in ternary expression");
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

// Retrieve index of a set (string/array, but works for numbers as well)
static void subscript(rf_parser *y) {
    y->sd++;
    int rx = y->rx; // Save flag
    unset(rx);      // Unset
    int n = expr_list(y, ']');
    consume(y, ']', "expected ']' following subscript expression");
    c_index(y->c, n, rx || is_asgmt(y->x->tk.kind) || is_incdec(y->x->tk.kind) || y->x->tk.kind == '[');
    y->rx = rx;     // Restore flag
    y->sd--;
}

static void call(rf_parser *y) {
    int n = expr_list(y, ')');
    consume(y, ')', "expected ')'");
    push(OP_CALL);
    push((uint8_t) n);
}

// TODO Support arbitrary indexing a la C99 designators
static void array(rf_parser *y) {
    int n = expr_list(y, '}');
    consume(y, '}', "expected '}'");
    c_array(y->c, n);
}

// Anonymous functions in expressions, e.g.
//   f = fn () {...}
static void anon_fn(rf_parser *y) {
    rf_fn *f = malloc(sizeof(rf_fn));
    rf_str *name = s_newstr("<anonymous fn>", 14, 0);
    f_init(f, name);
    m_growarray(y->e->fn, y->e->nf, y->e->fcap, rf_fn *);
    y->e->fn[y->e->nf++] = f;
    rf_parser fy;
    fy.e = y->e;
    fy.x = y->x;
    fy.c = f->code;
    y_init(&fy);
    add_local(&fy, name);   // Dummy reference to itself
    f->arity = compile_fn(&fy);
    c_fn_constant(y->c, f);
}

// type = 1 if called from led()
// type = 0 if called from nud()
static int sequence(rf_parser *y, int type) {
    int e, from, to, step;
    e    = 0;
    from = type;
    to   = 0;
    step = 0;

    // No upper bound provided?
    if (y->x->tk.kind == ':') {
        step = 1;
        adv;
        e = expr(y, 0);
    } else if (y->x->tk.kind == ')' ||
               y->x->tk.kind == '}' ||
               y->x->tk.kind == ']') {
        c_sequence(y->c, from, to, step);
        return e;
    }

    // Consume next expr as upper bound in sequence
    else {
        to = 1;
        expr(y, 0);
        if (y->x->tk.kind == ':') {
            step = 1;
            adv;
            e = expr(y, 0);
        }
    }

    c_sequence(y->c, from, to, step);
    return e;
}

static int nud(rf_parser *y) {
    int tk = y->x->tk.kind;
    int e = 0;
    if (uop(tk)) {
        if (!y->sd) set(ox);
        adv;
        e = expr(y, 13);
        c_prefix(y->c, tk);
        return e;
    }
    switch (tk) {

    // TODO
    case '$': {
        int ox = y->ox;
        int rx = y->rx;
        unset(ox);
        unset(rx);
        adv;
        set(argx);
        expr(y, 17);
        if (rx || is_asgmt(y->x->tk.kind) || is_incdec(y->x->tk.kind)) {
            set(ax);
            push(OP_ARGA);
        } else
            push(OP_ARGV);
        if (!y->lhs) set(lhs);
        unset(argx);
        y->ox = ox;
        y->rx = rx;
        break;
    }
    case '(':
        adv;
        expr(y, 0);
        consume(y, ')', "expected ')'");
        if (!y->argx) { 
            if (is_asgmt(y->x->tk.kind))
                err(y, "invalid operator following expr");
            // Hack to prevent parsing ++/-- as postfix following
            // parenthesized expression
            else if (is_incdec(y->x->tk.kind))
                return TK_INC;
        }
        return ')';
    case '{':
        adv;
        array(y);
        break;
    case TK_DOTS:
        unset(rx);
        if (!y->sd) set(ox);
        adv;
        e = sequence(y, 0);
        break;
    case TK_INC: case TK_DEC:
        if (adv)
            err(y, "expected symbol following prefix ++/--");
        if (!(y->x->tk.kind == TK_ID ||
              y->x->tk.kind == '$'))
            err(y, "unexpected symbol following prefix ++/--");
        set(rx);
        expr(y, 14);
        c_prefix(y->c, tk);
        if (!y->sd && !y->lhs) {
            set(px);
            set(lhs);
        }
        unset(rx);
        break;
    case TK_NULL: case TK_FLT: case TK_INT: case TK_STR:
        literal(y);
        break;
    case TK_ID:
        identifier(y);
        break;
    case TK_FN:
        adv;
        anon_fn(y);
        break;
    default:
        // TODO Handle invalid nuds
        if (lbop(tk) || rbop(tk))
            err(y, "invalid start of expression");
        else
            err(y, "unexpected symbol");
        break;
    }
    return tk;
}

static int led(rf_parser *y, int p, int tk) {
    switch (tk) {
    case TK_DOTS:
        unset(rx);
        if (!y->sd) set(ox);
        adv;
        p = sequence(y, 1);
        break;
    case TK_INC: case TK_DEC:
        if (is_const(p) || y->rx)
            return p;
        if (!y->sd && !y->lhs) {
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
        if (!y->sd) set(ox);
        adv;
        p = conditional(y);
        break;
    case TK_AND: case TK_OR:
        unset(rx);
        if (!y->sd) set(ox);
        adv;
        logical(y, tk);
        p = y->x->tk.kind;
        break;
    case '[':
        adv;
        subscript(y);
        return ']';
    case '(':
        if (!y->sd) set(ox);
        adv;
        call(y);
        return ')';
    default:
        if (lbop(tk) || rbop(tk)) {
            if (!y->sd && !y->ax && !y->ox) {
                set(lhs);
                if (is_asgmt(tk)) {
                    set(ax);
                } else {
                    set(ox);
                }
            } else if (y->lhs && !y->argx && y->ox && is_asgmt(tk)) {
                err(y, "syntax error");
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
        if ((is_const(p) || is_incdec(p)) && !const_follow_ok(tk))
            return p;

        // Return if the previous nud had a prefix ++/-- attached and
        // the following token is also ++/--. This would be an
        // invalid operation; this allows the ++/-- to be parsed as
        // a prefix op for the following expression.
        if (y->rx && is_incdec(tk))
            return p;

        // Prevent post-increments from being associated with function
        // calls
        if (p == ')' && is_incdec(tk))
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

// After exiting scope, "pop" local variables no longer in scope by
// decrementing y->nlcl
static uint8_t pop_locals(rf_parser *y, int depth, int f) {
    if (!y->nlcl) return 0;

    uint8_t count = 0;
    for (int i = y->nlcl - 1; i >= 0 && (y->lcl[i].d > depth); --i) {
        ++count;
        if (f)
            free(y->lcl[i].id);
    }
    if (!count)
        return count;
    else if (count == 1)
        push(OP_POP);
    else {
        push(OP_POPI);
        push(count);
    }
    return count;
}

static void break_stmt(rf_parser *y) {
    if (!y->ld)
        err(y, "break statement outside of loop");
    // Reserve a forward jump
    p_list *p = y->brk;
    pop_locals(y, y->loop, 0);
    m_growarray(p->l, p->n, p->cap, int);
    p->l[p->n++] = c_prep_jump(y->c, JMP);
}

static void cont_stmt(rf_parser *y) {
    if (!y->ld)
        err(y, "continue statement outside of loop");
    // Reserve a forward jump
    p_list *p = y->cont;
    pop_locals(y, y->loop, 0);
    m_growarray(p->l, p->n, p->cap, int);
    p->l[p->n++] = c_prep_jump(y->c, JMP);
}

static void p_init(p_list *p) {
    p->n   = 0;
    p->cap = 0;
    p->l   = NULL;
}

static void enter_loop(rf_parser *y, p_list *b, p_list *c) {
    p_init(b);
    p_init(c);
    y->brk  = b;
    y->cont = c;
}

static void exit_loop(rf_parser *y, p_list *ob, p_list *oc, p_list *nb, p_list *nc) {
    y->brk  = ob;
    y->cont = oc;
    if (nb->n) free(nb->l);
    if (nc->n) free(nc->l);
}

static void do_stmt(rf_parser *y) {
    p_list *r_brk  = y->brk;
    p_list *r_cont = y->cont;
    p_list b, c;
    uint8_t old_loop = y->loop;
    y->loop = y->ld++;
    enter_loop(y, &b, &c);
    int l1 = y->c->n;
    if (y->x->tk.kind == '{') {
        adv;
        stmt_list(y);
        consume(y, '}', "expected '}'");
    } else {
        stmt(y);
    }
    consume(y, TK_WHILE, "expected 'while' condition after 'do' block");
    y->ld--;
    y->loop = old_loop;
    y->nlcl -= pop_locals(y, y->ld, 1);
    // Patch continue stmts
    for (int i = 0; i < c.n; i++) {
        c_patch(y->c, c.l[i]);
    }
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

static void add_local(rf_parser *y, rf_str *id) {
    m_growarray(y->lcl, y->nlcl, y->lcap, local);
    y->lcl[y->nlcl++] = (local) {s_newstr(id->str, id->l, 1), y->ld};
}

// Returns the arity of the parsed function
static int compile_fn(rf_parser *y) {
    int arity = 0;
    uint8_t old_fd = y->fd;
    y->fd = ++y->ld;
    if (y->x->tk.kind == '(') {
        adv;

        // Read parameter identifiers, reserving them as locals to the
        // function
        while (1) {
            if (y->x->tk.kind == TK_ID) {
                add_local(y, y->x->tk.lexeme.s);
                ++arity;
                adv;
            }
            if (y->x->tk.kind == ',')
                adv;
            else
                break;
        }
        consume(y, ')', "expected ')'");
    }
    consume(y, '{', "expected '{'");
    stmt_list(y);

    // Caller cleans the stack; no need to pop locals from scope

    // If the last stmt was not a return statement, push OP_RET
    if (!y->retx)
        push(OP_RET);
    consume(y, '}', "expected '}'");
    y->fd = old_fd;
    --y->ld;
    return arity;
}

// Handler for named local functions in the form of:
//   local fn f() {...}
//
// This is NOT equivalent to:
//   local f = fn () {...}
//
// Named local functions should be able to reference themselves
static void local_fn(rf_parser *y) {
    if (y->x->tk.kind != TK_ID)
        err(y, "expected identifier for local function definition");
    rf_str *id = y->x->tk.lexeme.s;
    int idx = resolve_local(y, id);

    // Create string for disassembly
    rf_str *str1 = s_newstr("local fn ", 9, 0);
    rf_str *str2 = s_concat(str1, y->x->tk.lexeme.s, 0);
    m_freestr(str1);

    // If the identifier doesn't already exist as a local at the
    // current scope, add a new local
    if (idx < 0 || y->lcl[idx].d != y->ld) {
        add_local(y, id);
        push(OP_NULL); // Reserve stack slot
        switch (y->nlcl - 1) {
        case 0: push(OP_LCLA0); break;
        case 1: push(OP_LCLA1); break;
        case 2: push(OP_LCLA2); break;
        default:
            push(OP_LCLA);
            push((uint8_t) y->nlcl - 1);
            break;
        }
    }
    rf_fn *f = malloc(sizeof(rf_fn));
    f_init(f, str2);
    rf_parser fy;
    m_growarray(y->e->fn, y->e->nf, y->e->fcap, rf_fn *);
    y->e->fn[y->e->nf++] = f;
    fy.e = y->e;
    fy.x = y->x;
    fy.c = f->code;
    y_init(&fy);

    // Reserve first local slot for itself. VM will adjust FP
    // accordingly.
    add_local(&fy, id);

    adv;
    f->arity = compile_fn(&fy);

    // Add function to the outer scope
    c_fn_constant(y->c, f);
    push(OP_SET);
}

// User-defined functions
static void fn_stmt(rf_parser *y) {
    rf_fn *f = malloc(sizeof(rf_fn));
    rf_str *id;
    if (y->x->tk.kind != TK_ID) {
        err(y, "expected identifier for function definition");
    } else {
        id = s_newstr(y->x->tk.lexeme.s->str, y->x->tk.lexeme.s->l, 1);
        adv;
    }
    f_init(f, id);
    m_growarray(y->e->fn, y->e->nf, y->e->fcap, rf_fn *);
    y->e->fn[y->e->nf++] = f;

    // Functions parsed with their own parser, same lexer
    rf_parser fy;
    fy.e = y->e;
    fy.x = y->x;
    fy.c = f->code;
    y_init(&fy);

    // Reserve first local slot for itself. This is to match behavior
    // of named local functions which are compiled so they can
    // reference themselves. This is only to keep the VM calling
    // convention consistent and (should) make direct recursion a
    // little faster.
    add_local(&fy, id);

    f->arity = compile_fn(&fy);
}

static void for_stmt(rf_parser *y) {
    int paren = 0;
    if (y->x->tk.kind == '(') {
        paren = 1;
        adv;
    }
    p_list *r_brk  = y->brk;
    p_list *r_cont = y->cont;
    p_list b, c;

    // Increment lexical depth once before adding locals [k,] v so
    // they're only visible to the loop
    y->ld++;
    if (y->x->tk.kind != TK_ID)
        err(y, "expected identifier");
    add_local(y, y->x->tk.lexeme.s);
    adv;
    int kv = 0;
    if (y->x->tk.kind == ',') {
        set(fx);
        kv = 1;
        adv;
        if (y->x->tk.kind != TK_ID)
            err(y, "expected identifier following ','");
        add_local(y, y->x->tk.lexeme.s);
        adv;
    }
    set(lx);
    consume(y, TK_IN, "expected 'in'");
    expr(y, 0);
    unset(fx);
    unset(lx);
    int l1 = c_prep_loop(y->c, kv);
    if (paren)
        consume(y, ')', "expected ')'");
    uint8_t old_loop = y->loop;

    // Increment the lexical depth again so break/continue statements
    // don't pop k,v locals
    y->loop = y->ld++;
    enter_loop(y, &b, &c);
    if (y->x->tk.kind == '{') {
        adv;
        stmt_list(y);
        consume(y, '}', "expected '}'");
    } else {
        stmt(y);
    }

    // Patch continue stmts
    for (int i = 0; i < c.n; i++) {
        c_patch(y->c, c.l[i]);
    }
    c_patch(y->c, l1);
    c_loop(y->c, l1 + 2);

    // Patch break stmts
    for (int i = 0; i < b.n; i++) {
        c_patch(y->c, b.l[i]);
    }

    // OP_POPL cleans up iterator state in the VM. Needs to be its own
    // instruction since break statements need to jump past the
    // OP_LOOP instructions to prevent further iteration
    push(OP_POPL);

    // Decrement lexical depth twice
    y->ld -= 2;
    y->loop = old_loop;

    // Pop locals with lexical depth as the argument instead of
    // y->loop. The workaround of incrementing lexical depth twice
    // prevents break/continue statements from otherwise popping the
    // 'for' loop's own locals. All other loop constructs use y->loop
    // as the argument.
    y->nlcl -= pop_locals(y, y->ld, 1);
    exit_loop(y, r_brk, r_cont, &b, &c);
}

static void if_stmt(rf_parser *y) {
    expr(y, 0);
    int l1, l2;
    y->ld++;
    l1 = c_prep_jump(y->c, JZ);
    if (y->x->tk.kind == '{') {
        adv;
        stmt_list(y);
        consume(y, '}', "expected '}'");
    } else {
        stmt(y);
    }
    y->ld--;
    y->nlcl -= pop_locals(y, y->ld, 1);
    if (y->x->tk.kind == TK_ELSE) {
        adv;
        y->ld++;
        l2 = c_prep_jump(y->c, JMP);
        c_patch(y->c, l1);
        if (y->x->tk.kind == '{') {
            adv;
            stmt_list(y);
            consume(y, '}', "expected '}'");
        } else {
            stmt(y);
        }
        c_patch(y->c, l2);
        y->ld--;
        y->nlcl -= pop_locals(y, y->ld, 1);
    } else {
        c_patch(y->c, l1);
    }
}

static void local_stmt(rf_parser *y) {
    if (y->x->tk.kind == TK_FN) {
        adv;
        local_fn(y);
        return;
    }
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
            add_local(y, id);
            push(OP_NULL); // Reserve stack slot
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
    unset_all;
    int paren = 0;
    if (y->x->tk.kind == '(') { // Parenthesized expr list?
        adv;
        paren = ')';
    }
    const char *p = y->x->p;    // Save pointer
    int n = expr_list(y, paren);
    if (p == y->x->p)           // No expression parsed
        err(y, "expected expression in `print` statement");
    c_print(y->c, n);
    if (paren)
        consume(y, ')', "expected ')'");
}

static void ret_stmt(rf_parser *y) {
    if (y->ld == y->fd)
        set(retx);
    if (y->x->tk.kind == ';' || y->x->tk.kind == '}') {
        push(OP_RET);
        return;
    }
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
    uint8_t old_loop = y->loop;
    y->loop = y->ld++;
    enter_loop(y, &b, &c);
    if (y->x->tk.kind == '{') {
        adv;
        stmt_list(y);
        consume(y, '}', "expected '}'");
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
    y->ld--;
    y->loop = old_loop;
    y->nlcl -= pop_locals(y, y->ld, 1);
    exit_loop(y, r_brk, r_cont, &b, &c);
}

static void stmt(rf_parser *y) {
    unset(retx);
    switch (y->x->tk.kind) {
    case ';':       adv;                break;
    case TK_BREAK:  adv; break_stmt(y); break;
    case TK_CONT:   adv; cont_stmt(y);  break;
    case TK_DO:     adv; do_stmt(y);    break;
    case TK_EXIT:   adv; exit_stmt(y);  break;
    case TK_FN:
        peek;
        if (y->x->la.kind == TK_ID) {
            adv;
            fn_stmt(y);
        } else {
            expr_stmt(y);
        }
        break;
    case TK_FOR:    adv; for_stmt(y);   break;
    case TK_IF:     adv; if_stmt(y);    break;
    case TK_LOCAL:  adv; local_stmt(y); break;
    case TK_PRINT:  adv; print_stmt(y); break;
    case TK_RETURN: adv; ret_stmt(y);   break;
    case TK_WHILE:  adv; while_stmt(y); break;
    default:             expr_stmt(y);  break;
    }

    // Skip token if semicolon used as a statement terminator
    if (y->x->tk.kind == ';') adv;
}

static void stmt_list(rf_parser *y) {
    while (!(y->x->tk.kind == TK_EOI ||
             y->x->tk.kind == '}'))
        stmt(y);
}

static void y_init(rf_parser *y) {
    unset_all;
    unset(fx);
    unset(lx);

    y->nlcl = 0;
    y->lcap = 0;
    y->lcl  = NULL;

    y->ld   = 0;
    y->fd   = 0;
    y->sd   = 0;
    y->loop = 0;
    y->brk  = NULL;
    y->cont = NULL;
}

int y_compile(rf_env *e) {
    rf_parser y;
    y.e = e;
    rf_lexer  x;
    y.x = &x;
    x_init(&x, e->src);
    y.c = e->main.code;
    y_init(&y);
    stmt_list(&y);
    pop_locals(&y, y.ld, 1);
    c_push(y.c, OP_RET);
    x_free(&x);
    return 0;
}

#undef adv
#undef peek
#undef push
#undef set
#undef unset
#undef unset_all
