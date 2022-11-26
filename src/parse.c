#include "parse.h"

#include "code.h"
#include "lex.h"
#include "mem.h"
#include "string.h"
#include "value.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

#define push(b) c_push(y->c, b)

#define advance()       riff_lex_advance(y->x, 0)
#define peek()          riff_lex_peek(y->x, 0)
#define advance_mode(m) riff_lex_advance(y->x, (m))
#define peek_mode(m)    riff_lex_peek(y->x, (m))

#define set(f)      y->f    = 1
#define unset(f)    y->f    = 0
#define restore(f)  y->f    = f
#define unset_all() y->lhs  = 0; \
                    y->ox   = 0; \
                    y->retx = 0

#define save_and_unset(f) int f = y->f; y->f = 0

#define TK(i)       (y->x->tk[i])
#define TK_CMP(i,t) (TK(i).kind == (t))

enum expr_flags {
    EXPR_UNARY  = 1 << 0,
    EXPR_FIELD  = 1 << 1,
    EXPR_REF    = 1 << 2,
    EXPR_FOR_Z  = 1 << 3
};

// Local vars
typedef struct {
    riff_str *id;
    int       d;
    int       reserved: 1;
} local;

// Patch lists used for break/continue statements in loops
typedef struct {
    int  n;
    int  cap;
    int *l;
} patch_list;

typedef struct {
    riff_lexer *x;          // Parser controls lexical analysis
    riff_code  *c;          // Current code object
    riff_state *state;      // Global state
    int         nlcl;       // Number of locals in scope
    int         lcap;
    local      *lcl;        // Array of local vars
    uint8_t     ld;         // Lexical depth/scope
    uint8_t     fd;         // Top-level scope of the current function
    uint8_t     id;         // Iterator depth (`for` loops only)
    uint8_t     loop;       // Depth of current loop
    int         lhs: 1;     // Set when leftmost expr has been evaluated
    int         ox: 1;      // Typical (i.e. not ++/--) operation flag
    int         lx: 1;      // Local flag (newly-declared)
    int         retx: 1;    // Return flag
    patch_list *brk;        // Patch list for break stmts (current loop)
    patch_list *cont;       // Patch list for continue stmts (current loop)
} riff_parser;

// TODO Hardcoded logic for valid "follow" tokens should be cleaned up
// TODO Syntax error handling; parser should do as much heavy lifting as
// possible

static int  expr(riff_parser *, uint32_t, int);
static void stmt_list(riff_parser *);
static void stmt(riff_parser *);
static void add_local(riff_parser *, riff_str *, int);
static int  compile_fn(riff_parser *);
static void y_init(riff_parser *);

static void err(riff_parser *y, const char *msg) {
    fprintf(stderr, "riff: [compile] line %d: %s\n", y->x->ln, msg);
    exit(1);
}

static char closing_delim(char c) {
    switch (c) {
    case '[': return ']';
    case '(': return ')';
    case '{': return '}';
    default:  return c;
    }
}

static int is_asgmt(int tk) {
    return tk == '=' || (tk >= RIFF_TK_ADDX && tk <= RIFF_TK_XORX);
}

static int is_literal(int tk) {
    return tk >= RIFF_TK_NULL && tk <= RIFF_TK_REGEX;
}

static int is_incdec(int tk) {
    return tk == RIFF_TK_INC || tk == RIFF_TK_DEC;
}

static int literal_follow_ok(int tk) {
    return !(is_incdec(tk) || tk == '(');
}

static void check(riff_parser *y, int tk, const char *msg) {
    if (!TK_CMP(0, tk))
        err(y, msg);
}

static void consume(riff_parser *y, int tk, const char *msg) {
    check(y, tk, msg);
    advance();
}

static void consume_mode(riff_parser *y, int mode, int tk, const char *msg) {
    check(y, tk, msg);
    advance_mode(mode);
}

// LBP for non-prefix operators
static int lbp(int tk) {
    switch (tk) {
    case '.': case '(': case'[':
    case RIFF_TK_INC: case RIFF_TK_DEC: return 16;
    case RIFF_TK_POW:                   return 15;
    case '*': case '/': case '%':       return 13;
    case '+': case '-':                 return 12;
    case '#':                           return 11;
    case RIFF_TK_SHL: case RIFF_TK_SHR: return 11;
    case '&':                           return 10;
    case '^':                           return 9;
    case '|':                           return 8;
    case '>': case '<':      
    case RIFF_TK_GE: case RIFF_TK_LE:   return 7;
    case '~': case RIFF_TK_NMATCH:      return 6;
    case RIFF_TK_EQ: case RIFF_TK_NE:   return 6;
    case RIFF_TK_AND:                   return 5;
    case RIFF_TK_OR:                    return 4;
    case RIFF_TK_2DOTS:                 return 3;
    case '?':                           return 2;
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
    return tk == '#' || tk == '%' || tk == '&' || tk == '(' ||
           tk == '*' || tk == '+' || tk == '-' || tk == '/' ||
           tk == '<' || tk == '>' || tk == '^' || tk == '|' ||
           tk == '.' || tk == '[' || tk == '~' ||
           tk == RIFF_TK_AND || tk == RIFF_TK_EQ  || tk == RIFF_TK_NE ||
           tk == RIFF_TK_GE  || tk == RIFF_TK_LE  || tk == RIFF_TK_OR ||
           tk == RIFF_TK_SHL || tk == RIFF_TK_SHR || tk == RIFF_TK_NMATCH;
}

static int rbop(int tk) {
    return is_asgmt(tk) || tk == RIFF_TK_POW;
}

static int resolve_local(riff_parser *y, uint32_t flags, riff_str *s) {
    if (!y->nlcl) {
        return -1;
    }

    // If LHS hasn't been evaluated or we're not inside a local statement,
    // search through all active locals. Otherwise, we're evaluating the RHS of
    // a newly declared local and should exclude it from its own initialization.
    //
    // This allows for statements like `local x = x` where the LHS `x` refers to
    // a newly-declared local and the RHS refers to any `x` previously in scope.
    int i = (!y->lhs || !y->lx) ? y->nlcl - 1 : y->nlcl - 2;

    // If parsing the expression `z` in a 'for' loop declaration e.g.
    //
    //   for x,y in z {...}
    //
    // Decrement `i` again to ignore both `x` and `y` for reasons similar to the
    // above comments. Note that EXPR_FOR_Z is not set for 'for' loops in the
    // form of `for x in y`; the lx flag is sufficient
    i -= !!(flags & EXPR_FOR_Z);

    for (; i >= 0; --i) {
        if (riff_str_eq(y->lcl[i].id, s) && y->lcl[i].d <= y->ld) {
            return i;
        }
    }
    return -1;
}

static void identifier(riff_parser *y, uint32_t flags) {
    int scope  = resolve_local(y, flags, TK(0).s);
    local *var = scope >= 0 ? &y->lcl[scope] : NULL;

    // If symbol succeeds a prefix ++/-- operation, signal codegen to push the
    // address of the symbol
    if (flags & EXPR_REF) {
        if (var) {
            c_local(y->c, scope, 1, !var->reserved);
            var->reserved = 1;
        } else {
            c_global(y->c, &TK(0), 1);
        }
        advance_mode(LEX_LED);
        return;
    }

    // Else, peek at the next token. If the following token is assignment,
    // ++/--, or '[', signal codegen to push the address of the symbol.
    // Otherwise, push the value itself.
    peek_mode(LEX_LED);
    if ((!(flags & EXPR_FIELD) || flags & EXPR_UNARY) &&
            (is_incdec(TK(1).kind) ||
             is_asgmt(TK(1).kind)  ||
             TK_CMP(1, '.') || TK_CMP(1, '['))) {
        if (var) {
            c_local(y->c, scope, 1, !var->reserved);
            var->reserved = 1;
        } else {
            c_global(y->c, &TK(0), 1);
        }
    } else {
        if (var) {
            c_local(y->c, scope, 0, 0);
        } else {
            c_global(y->c, &TK(0), 0);
        }
    }
    advance_mode(LEX_NUD);
}

static void literal(riff_parser *y, uint32_t flags) {
    c_constant(y->c, &TK(0));
    advance_mode(LEX_LED);
    // Assert no assignment appears following a constant literal
    if (!(flags & EXPR_FIELD) && is_asgmt(TK(0).kind)) {
        err(y, "invalid assignment to constant value");
    }
}

static void interpolated_str(riff_parser *y, char d) {
    uint8_t n = 0;
    int inter_mode = d == '\'' ? RIFF_TK_STR_INTER_SQ : RIFF_TK_STR_INTER_DQ;
    int lex_mode = d == '\'' ? LEX_STR_SQ : LEX_STR_DQ;
    while (TK_CMP(0, inter_mode)) {
        if (riff_strlen(TK(0).s) > 0) {
            ++n;
            TK(0).kind = RIFF_TK_STR;
            c_constant(y->c, &TK(0));
        }
        advance();

        // Lexer ensures validity of the following token
        if (TK_CMP(0, RIFF_TK_IDENT)) {
            int scope = resolve_local(y, 0, TK(0).s);
            if (scope >= 0) {
                c_local(y->c, scope, 0, 0);
            } else {
                c_global(y->c, &TK(0), 0);
            }
            ++n;
            advance_mode(lex_mode);
        } else {
            char c = closing_delim(TK(0).kind);
            peek();
            if (!TK_CMP(1, c)) {
                advance();
                expr(y, 0, 0);
                ++n;
                consume_mode(y, lex_mode, c, "expected closing delimeter following interpolated expression");
            } else {
                advance_mode(lex_mode);
            }
        }
    }

    // RIFF_TK_STR denotes the end of a string literal
    if (riff_likely(TK_CMP(0, RIFF_TK_STR))) {
        if (riff_strlen(TK(0).s) > 0) {
            ++n;
            c_constant(y->c, &TK(0));
        }
        advance_mode(LEX_LED);
    } else {
        err(y, "invalid interpolation");
    }
    c_concat(y->c, n);
}

static int paren_expr(riff_parser *y) {
    save_and_unset(lhs);
    save_and_unset(ox);
    int e = expr(y, 0, 0);
    restore(lhs);
    restore(ox);
    return e;
}

// conditional_expr = expr '?' [expr] ':' expr
static int conditional(riff_parser *y, uint32_t flags) {
    int e, l1, l2;

    // x ?: y
    if (TK_CMP(0, ':')) {
        advance();
        l1 = c_prep_jump(y->c, XJNZ);
        e = paren_expr(y);
        c_patch(y->c, l1);
    }

    // x ? y : z
    else {
        l1 = c_prep_jump(y->c, JZ);
        paren_expr(y);
        l2 = c_prep_jump(y->c, JMP);
        c_patch(y->c, l1);
        consume(y, ':', "expected ':' in ternary expression");
        e = paren_expr(y);
        c_patch(y->c, l2);
    }
    return e;
}

// logical_expr = expr logical_op expr
// logical_op   = '&&' | '||'
static void logical(riff_parser *y, uint32_t flags, int tk) {
    int l1 = c_prep_jump(y->c, tk == RIFF_TK_OR ? XJNZ : XJZ);
    expr(y, flags, lbp(tk));
    c_patch(y->c, l1);
}

// expr_list = expr {',' expr}
static int expr_list(riff_parser *y, int c) {
    save_and_unset(lhs);
    save_and_unset(ox);
    int n = 0;
    while (!TK_CMP(0, c)) {
        unset_all();
        expr(y, 0, 0);
        ++n;
        if (TK_CMP(0, ',')) {
            advance();
        } else {
            break;
        }
    }
    restore(lhs);
    restore(ox);
    return n;
}

static int paren_expr_list(riff_parser *y, int c) {
    int n = 0;
    while (!TK_CMP(0, c)) {
        paren_expr(y);
        ++n;
        if (TK_CMP(0, ',')) {
            advance();
        } else {
            break;
        }
    }
    return n;
}

// member_access_expr = id '.' id
static void member_access(riff_parser *y, uint32_t flags) {
    int last_ins_idx = y->c->last;
    if (!TK_CMP(0, RIFF_TK_IDENT)) {
        err(y, "expected identifier in member access expression");
    }
    riff_str *s = TK(0).s;
    advance_mode(LEX_LED);
    c_str_index(y->c, last_ins_idx, s, flags & EXPR_REF || is_asgmt(TK(0).kind) || is_incdec(TK(0).kind) || TK_CMP(0, '.') || TK_CMP(0, '['));
}

// subscript_expr = id '[' expr ']'
static void subscript(riff_parser *y, uint32_t flags) {
    int last_ins_idx = y->c->last;
    int n = paren_expr_list(y, ']');
    consume_mode(y, LEX_LED, ']', "expected ']' following subscript expression");
    c_index(y->c, last_ins_idx, n, flags & EXPR_REF || is_asgmt(TK(0).kind) || is_incdec(TK(0).kind) || TK_CMP(0, '.') || TK_CMP(0, '['));
}

// call_expr = expr '(' [expr_list] ')'
static void call(riff_parser *y) {
    int n = paren_expr_list(y, ')');
    consume_mode(y, LEX_LED, ')', "expected ')'");
    c_call(y->c, n);
}

// table_expr = '{' expr_list '}'
//            | '[' expr_list ']'
static void table(riff_parser *y, int delim) {
    int n = expr_list(y, delim);
    consume_mode(y, LEX_LED, delim, "expected closing brace/bracket");
    c_table(y->c, n);
}

// Function literals
// anon_fn_expr = 'fn' ['(' [id {',' id}] '{' stmt_list '}'
static void anon_fn(riff_parser *y) {
    riff_fn *f = malloc(sizeof(riff_fn));
    riff_str *name = riff_str_new("", 0);
    f_init(f, name);
    m_growarray(y->state->fn, y->state->nf, y->state->fcap, riff_fn *);
    y->state->fn[y->state->nf++] = f;
    riff_parser fy;
    fy.state = y->state;
    fy.x = y->x;
    fy.c = f->code;
    y_init(&fy);
    add_local(&fy, name, 1);   // Dummy reference to itself
    f->arity = compile_fn(&fy);
    c_fn_constant(y->c, f);
}

// range_expr = [expr] '..' [expr] [':' expr] 
//
// type = 1 if called from led()
// type = 0 if called from nud()
static int range(riff_parser *y, uint32_t flags, int type) {
    int e, from, to, step;
    e    = 0;
    from = type;
    to   = 0;
    step = 0;

    // No upper bound provided?
    if (TK_CMP(0, ':')) {
        step = 1;
        advance();
        e = expr(y, flags, 0);
    } else if (TK_CMP(0, ')') || TK_CMP(0, '}') || TK_CMP(0, ']')) {
        c_range(y->c, from, to, step);
        return e;
    }

    // Consume next expr as upper bound in range
    else {
        to = 1;
        expr(y, flags, 0);
        if (TK_CMP(0, ':')) {
            step = 1;
            advance();
            e = expr(y, flags, 0);
        }
    }

    c_range(y->c, from, to, step);
    return e;
}

static int nud(riff_parser *y, uint32_t flags) {
    int tk = TK(0).kind;
    int e = 0;
    if (uop(tk)) {
        set(ox);
        advance();
        e = expr(y, flags | EXPR_UNARY, 13);
        c_prefix(y->c, tk);
        return e;
    }
    switch (tk) {

    // TODO
    case '$': {
        save_and_unset(ox);
        advance();
        expr(y, EXPR_FIELD, 17);
        c_fldv_index(y->c, flags & EXPR_REF || is_asgmt(TK(0).kind) || is_incdec(TK(0).kind));
        set(lhs);
        restore(ox);
        break;
    }
    case '(': {
        advance();
        paren_expr(y);
        consume_mode(y, LEX_LED, ')', "expected ')'");
        if (!(flags & EXPR_FIELD)) {
            if (is_asgmt(TK(0).kind)) {
                err(y, "invalid operator following expr");
            }
            // Hack to prevent parsing ++/-- as postfix following parenthesized
            // expression
            else if (is_incdec(TK(0).kind)) {
                return RIFF_TK_INC;
            }
        }
        return ')';
    }
    case '[':
    case '{':
        advance();
        table(y, closing_delim(tk));
        break;
    case RIFF_TK_2DOTS:
        set(ox);
        advance();
        e = range(y, flags & ~EXPR_REF, 0);
        break;
    case RIFF_TK_INC:
    case RIFF_TK_DEC:
        if (advance()) {
            err(y, "expected symbol following prefix ++/--");
        }
        if (!(TK_CMP(0, RIFF_TK_IDENT) || TK_CMP(0, '$'))) {
            err(y, "unexpected symbol following prefix ++/--");
        }
        expr(y, flags | EXPR_REF, 14);
        c_prefix(y->c, tk);
        set(lhs);
        break;
    case RIFF_TK_NULL:
    case RIFF_TK_INT:
    case RIFF_TK_FLOAT:
    case RIFF_TK_STR:
    case RIFF_TK_REGEX:
        literal(y, flags);
        break;
    case RIFF_TK_STR_INTER_SQ:
        interpolated_str(y, '\'');
        break;
    case RIFF_TK_STR_INTER_DQ:
        interpolated_str(y, '"');
        break;
    case RIFF_TK_IDENT:
        identifier(y, flags);
        break;
    case RIFF_TK_FN:
        advance();
        anon_fn(y);
        break;
    default:
        // TODO Handle invalid nuds
        if (lbop(tk) || rbop(tk)) {
            err(y, "invalid start of expression");
        } else {
            err(y, "unexpected symbol");
        }
        break;
    }
    return tk;
}

static int led(riff_parser *y, uint32_t flags, int p, int tk) {
    switch (tk) {
    case RIFF_TK_2DOTS:
        set(ox);
        advance();
        return range(y, flags & ~EXPR_REF, 1);
    case RIFF_TK_INC:
    case RIFF_TK_DEC:
        if (is_literal(p) || flags & EXPR_REF) {
            return p;
        }
        set(lhs);
        c_postfix(y->c, tk);
        advance_mode(LEX_LED);
        return TK(0).kind;
    case '?':
        set(ox);
        advance();
        return conditional(y, flags & ~EXPR_REF);
    case RIFF_TK_AND:
    case RIFF_TK_OR:
        set(ox);
        advance();
        logical(y, flags & ~EXPR_REF, tk);
        return TK(0).kind;
    case '.':
        advance();
        member_access(y, flags);
        return TK(0).kind;
    case '[':
        advance();
        subscript(y, flags);
        return ']';
    case '(':
        advance();
        call(y);
        return ')';
    default:
        if (lbop(tk) || rbop(tk)) {
            if (y->lhs && !(flags & EXPR_FIELD) && y->ox && is_asgmt(tk)) {
                err(y, "syntax error");
            }
            if (!y->ox) {
                set(lhs);
            }
            if (!is_asgmt(tk)) {
                set(ox);
            }
            advance();
            p = expr(y, flags & ~EXPR_REF, lbop(tk) ? lbp(tk) : lbp(tk) - 1);
            c_infix(y->c, tk);
        }
        break;
    }
    return p;
}

static int expr(riff_parser *y, uint32_t flags, int rbp) {
    int p  = nud(y, flags);
    int tk = TK(0).kind;

    while (rbp < lbp(tk)) {

        // Return early if previous nud was a literal and succeeding token would
        // make for an invalid expression. This allows the succeeding token to
        // be parsed as a new expression instead of throwing an error
        if ((is_literal(p) || is_incdec(p)) && !literal_follow_ok(tk))
            return p;

        // Return if the previous nud had a prefix ++/-- attached and the
        // following token is also ++/--. This would be an invalid operation;
        // this allows the ++/-- to be parsed as a prefix op for the following
        // expression.
        if (flags & EXPR_REF && is_incdec(tk))
            return p;

        // Prevent post-increments from being associated with function calls
        if (p == ')' && is_incdec(tk))
            return p;

        p  = led(y, flags, p, tk);
        tk = TK(0).kind;
    }
    return p;
}

// expr_stmt = expr_list
static void expr_stmt(riff_parser *y) {
    int n = expr_list(y, 0);
    c_pop_expr_stmt(y->c, n);
}

// After exiting scope, "pop" local variables no longer in scope by
// decrementing y->nlcl
static uint8_t pop_locals(riff_parser *y, int depth, int f) {
    if (!y->nlcl) return 0;

    uint8_t count = 0;
    for (int i = y->nlcl - 1; i >= 0 && (y->lcl[i].d > depth); --i) {
        ++count;
    }
    if (count) {
        c_pop(y->c, count);
    }
    return count;
}

// break_stmt = 'break'
static void break_stmt(riff_parser *y) {
    if (!y->loop) {
        err(y, "break statement outside of loop");
    }
    // Reserve a forward jump
    patch_list *p = y->brk;
    pop_locals(y, y->loop, 0);
    m_growarray(p->l, p->n, p->cap, int);
    p->l[p->n++] = c_prep_jump(y->c, JMP);
}

// continue_stmt = 'continue'
static void continue_stmt(riff_parser *y) {
    if (!y->loop) {
        err(y, "continue statement outside of loop");
    }
    // Reserve a forward jump
    patch_list *p = y->cont;
    pop_locals(y, y->loop, 0);
    m_growarray(p->l, p->n, p->cap, int);
    p->l[p->n++] = c_prep_jump(y->c, JMP);
}

static void p_init(patch_list *p) {
    p->n   = 0;
    p->cap = 0;
    p->l   = NULL;
}

static void enter_loop(riff_parser *y, patch_list *b, patch_list *c) {
    p_init(b);
    p_init(c);
    y->brk  = b;
    y->cont = c;
}

static void exit_loop(riff_parser *y, patch_list *ob, patch_list *oc, patch_list *nb, patch_list *nc) {
    y->brk  = ob;
    y->cont = oc;
    if (nb->n) free(nb->l);
    if (nc->n) free(nc->l);
}

// do_stmt = 'do' stmt 'while' expr
//         | 'do' '{' stmt_list '}' 'while' expr
static void do_stmt(riff_parser *y) {
    patch_list *r_brk  = y->brk;
    patch_list *r_cont = y->cont;
    patch_list b, c;
    uint8_t old_loop = y->loop;
    y->loop = ++y->ld;
    enter_loop(y, &b, &c);
    int l1 = y->c->n;
    if (TK_CMP(0, '{')) {
        advance();
        stmt_list(y);
        consume(y, '}', "expected '}'");
    } else {
        stmt(y);
    }
    consume(y, RIFF_TK_WHILE, "expected 'while' condition after 'do' block");
    --y->ld;
    y->loop = old_loop;
    y->nlcl -= pop_locals(y, y->ld, 1);
    // Patch continue stmts
    for (int i = 0; i < c.n; i++) {
        c_patch(y->c, c.l[i]);
    }
    expr(y, 0, 0);
    c_jump(y->c, JNZ, l1);
    // Patch break stmts
    for (int i = 0; i < b.n; i++) {
        c_patch(y->c, b.l[i]);
    }
    exit_loop(y, r_brk, r_cont, &b, &c);
}

static void add_local(riff_parser *y, riff_str *id, int reserved) {
    m_growarray(y->lcl, y->nlcl, y->lcap, local);
    y->lcl[y->nlcl++] = (local) {id, y->ld, reserved};
}

// Returns the arity of the parsed function
static int compile_fn(riff_parser *y) {
    int arity = 0;
    uint8_t old_fd = y->fd;
    y->fd = ++y->ld;
    if (TK_CMP(0, '(')) {
        advance();

        // Read parameter identifiers, reserving them as locals to the
        // function
        while (1) {
            if (TK_CMP(0, RIFF_TK_IDENT)) {
                add_local(y, TK(0).s, 1);
                ++arity;
                advance();
            }
            if (TK_CMP(0, ',')) {
                advance();
            } else {
                break;
            }
        }
        consume(y, ')', "expected ')'");
    }
    consume(y, '{', "expected '{'");
    stmt_list(y);

    // Caller cleans the stack; no need to pop locals from scope

    // If the last stmt was not a return statement, push OP_RET
    if (!y->retx) {
        push(OP_RET);
    }
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
static void local_fn(riff_parser *y) {
    if (!TK_CMP(0, RIFF_TK_IDENT)) {
        err(y, "expected identifier for local function definition");
    }
    riff_str *id = TK(0).s;
    int idx = resolve_local(y, 0, id);

    // Create string for disassembly
    riff_str *fn_name = riff_strcat("local ", TK(0).s->str, 6, riff_strlen(TK(0).s));

    // If the identifier doesn't already exist as a local at the current scope,
    // add a new local.
    if (idx < 0 || y->lcl[idx].d != y->ld) {
        add_local(y, id, 1);
        c_local(y->c, y->nlcl-1, 1, 1);
    }
    riff_fn *f = malloc(sizeof(riff_fn));
    f_init(f, fn_name);
    riff_parser fy;
    m_growarray(y->state->fn, y->state->nf, y->state->fcap, riff_fn *);
    y->state->fn[y->state->nf++] = f;
    fy.state = y->state;
    fy.x = y->x;
    fy.c = f->code;
    y_init(&fy);

    // Reserve first local slot for itself. VM will adjust FP accordingly.
    add_local(&fy, id, 1);

    advance();
    f->arity = compile_fn(&fy);

    // Add function to the outer scope
    c_fn_constant(y->c, f);
    c_infix(y->c, '=');
}

// fn_stmt = 'fn' id ['(' [id {',' id} ')'] '{' stmt_list '}'
static void fn_stmt(riff_parser *y) {
    riff_fn *f = malloc(sizeof(riff_fn));
    riff_str *id;
    if (riff_unlikely(!TK_CMP(0, RIFF_TK_IDENT))) {
        err(y, "expected identifier for function definition");
    } else {
        id = TK(0).s;
        advance();
    }
    f_init(f, id);
    m_growarray(y->state->fn, y->state->nf, y->state->fcap, riff_fn *);
    y->state->fn[y->state->nf++] = f;

    // Functions parsed with their own parser, same lexer
    riff_parser fy;
    fy.state = y->state;
    fy.x = y->x;
    fy.c = f->code;
    y_init(&fy);

    // Reserve first local slot for itself. This is to match behavior of named
    // local functions which are compiled so they can reference themselves. This
    // is only to keep the VM calling convention consistent and (should) make
    // direct recursion a little faster.
    add_local(&fy, id, 1);

    f->arity = compile_fn(&fy);
}

// for_stmt = 'for' id [',' id] 'in' expr stmt
//          | 'for' id [',' id] 'in' expr '{' stmt_list '}'
static void for_stmt(riff_parser *y) {
    int paren = 0;
    if (TK_CMP(0, '(')) {
        paren = 1;
        advance();
    }
    patch_list *r_brk  = y->brk;
    patch_list *r_cont = y->cont;
    patch_list b, c;

    y->id++;

    // Increment lexical depth once before adding locals [k,] v so they're only
    // visible to the loop.
    ++y->ld;
    if (!TK_CMP(0, RIFF_TK_IDENT)) {
        err(y, "expected identifier");
    }
    add_local(y, TK(0).s, 1);
    advance();
    int kv = 0;
    uint32_t flags = 0;
    if (TK_CMP(0, ',')) {
        flags |= EXPR_FOR_Z;
        kv = 1;
        advance();
        if (!TK_CMP(0, RIFF_TK_IDENT)) {
            err(y, "expected identifier following ','");
        }
        add_local(y, TK(0).s, 1);
        advance();
    }
    set(lx);
    consume(y, RIFF_TK_IN, "expected 'in'");
    expr(y, flags, 0);
    unset(lx);
    int l1 = c_prep_loop(y->c, kv);
    if (paren) {
        consume(y, ')', "expected ')'");
    }
    uint8_t old_loop = y->loop;

    // Increment the lexical depth again so break/continue statements don't pop
    // k,v locals.
    y->loop = ++y->ld;
    enter_loop(y, &b, &c);
    if (TK_CMP(0, '{')) {
        advance();
        stmt_list(y);
        consume(y, '}', "expected '}'");
    } else {
        stmt(y);
    }

    // Patch continue stmts
    for (int i = 0; i < c.n; i++) {
        c_patch(y->c, c.l[i]);
    }

    // Pop locals created inside the loop body
    y->ld -= 1;
    y->nlcl -= pop_locals(y, y->ld, 1);

    c_patch(y->c, l1);
    c_loop(y->c, l1 + 2);

    // Patch break stmts
    for (int i = 0; i < b.n; i++) {
        c_patch(y->c, b.l[i]);
    }

    // OP_POPL cleans up iterator state in the VM. Needs to be its own
    // instruction since break statements need to jump past the OP_LOOP
    // instructions to prevent further iteration
    c_end_loop(y->c);

    y->ld -= 1;
    y->id -= 1;
    y->loop = old_loop;

    // Pop locals with lexical depth as the argument instead of y->loop. The
    // workaround of incrementing lexical depth twice prevents break/continue
    // statements from otherwise popping the 'for' loop's own locals. All other
    // loop constructs use y->loop as the argument.
    y->nlcl -= pop_locals(y, y->ld, 1);
    exit_loop(y, r_brk, r_cont, &b, &c);
}

// if_stmt = 'if' expr stmt {'elif' expr ...} ['else' ...]
//         | 'if' expr '{' stmt_list '}' {'elif' expr ...} ['else' ...]
static void if_stmt(riff_parser *y) {
    expr(y, 0, 0);
    int l1, l2;
    ++y->ld;
    l1 = c_prep_jump(y->c, JZ);
    if (TK_CMP(0, '{')) {
        advance();
        stmt_list(y);
        consume(y, '}', "expected '}'");
    } else {
        stmt(y);
    }
    --y->ld;
    y->nlcl -= pop_locals(y, y->ld, 1);
    if (TK_CMP(0, RIFF_TK_ELIF)) {
y_elif:
        advance();
        l2 = c_prep_jump(y->c, JMP);
        c_patch(y->c, l1);
        if_stmt(y);
        c_patch(y->c, l2);
    } else if (TK_CMP(0, RIFF_TK_ELSE)) {
        advance();
        // Handle as `elif` for `else if`. This avoids the non-breaking issue
        // where lexical depth is double incremented.
        if (TK_CMP(0, RIFF_TK_IF)) {
            goto y_elif;
        }
        ++y->ld;
        l2 = c_prep_jump(y->c, JMP);
        c_patch(y->c, l1);
        if (TK_CMP(0, '{')) {
            advance();
            stmt_list(y);
            consume(y, '}', "expected '}'");
        } else {
            stmt(y);
        }
        c_patch(y->c, l2);
        --y->ld;
        y->nlcl -= pop_locals(y, y->ld, 1);
    } else {
        c_patch(y->c, l1);
    }
}

// local_stmt = 'local' expr {',' expr}
//            | 'local' fn_stmt
static void local_stmt(riff_parser *y) {
    if (TK_CMP(0, RIFF_TK_FN)) {
        advance();
        local_fn(y);
        return;
    }
    while (1) {
        unset_all();
        riff_str *id;
        if (!TK_CMP(0, RIFF_TK_IDENT)) {
            peek();
            if (!TK_CMP(1, RIFF_TK_IDENT)) {
                err(y, "local declaration requires valid identifier");
            }
            id = TK(1).s;
        } else {
            id = TK(0).s;
        }

        int idx = resolve_local(y, 0, id);

        // If the identifier doesn't already exist as a local at the
        // current scope, add a new local
        if (idx < 0 || y->lcl[idx].d != y->ld) {
            set(lx);    // Only set for newly-declared locals
            add_local(y, id, 0);
        }
        expr(y, EXPR_REF, 0);
        c_pop_expr_stmt(y->c, 1);
        unset(lx);
        if (TK_CMP(0, ',')) {
            advance();
        } else {
            break;
        }
    }
}

// loop_stmt = 'loop' stmt
//           | 'loop '{' stmt_list '}'
static void loop_stmt(riff_parser *y) {
    patch_list *r_brk  = y->brk;
    patch_list *r_cont = y->cont;
    patch_list b, c;
    uint8_t old_loop = y->loop;
    y->loop = ++y->ld;
    enter_loop(y, &b, &c);
    int l1 = y->c->n;
    if (TK_CMP(0, '{')) {
        advance();
        stmt_list(y);
        consume(y, '}', "expected '}'");
    } else {
        stmt(y);
    }
    --y->ld;
    y->loop = old_loop;
    y->nlcl -= pop_locals(y, y->ld, 1);
    // Patch continue stmts
    for (int i = 0; i < c.n; i++) {
        c_patch(y->c, c.l[i]);
    }
    c_jump(y->c, JMP, l1);
    // Patch break stmts
    for (int i = 0; i < b.n; i++) {
        c_patch(y->c, b.l[i]);
    }
    exit_loop(y, r_brk, r_cont, &b, &c);
}

// ret_stmt = 'return' [expr]
static void ret_stmt(riff_parser *y) {
    // Destroy any unterminated iterators before returning control. Destroying
    // iterators before parsing any return expression allows for codegen to
    // still check for tailcalls.
    for (int i = y->id; i > 0; --i) {
        c_end_loop(y->c);
    }
    if (y->ld == y->fd) {
        set(retx);
    }
    if (TK_CMP(0, RIFF_TK_EOI) || TK_CMP(0, ';') || TK_CMP(0, '}')) {
        c_return(y->c, 0);
        return;
    }

    const char *p = y->x->p; // Save pointer
    expr(y, 0, 0);

    // p != y->x->p when a valid expression is parsed following the `return`
    // keyword.
    c_return(y->c, p != y->x->p);
}

// while_stmt = 'while' expr stmt
//            | 'while' expr '{' stmt_list '}'
static void while_stmt(riff_parser *y) {
    patch_list *r_brk  = y->brk;
    patch_list *r_cont = y->cont;
    patch_list b, c;
    int l1, l2;
    l1 = y->c->n;
    expr(y, 0, 0);
    l2 = c_prep_jump(y->c, JZ);
    uint8_t old_loop = y->loop;
    y->loop = ++y->ld;
    enter_loop(y, &b, &c);
    if (TK_CMP(0, '{')) {
        advance();
        stmt_list(y);
        consume(y, '}', "expected '}'");
    } else {
        stmt(y);
    }
    --y->ld;
    y->loop = old_loop;
    y->nlcl -= pop_locals(y, y->ld, 1);
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

// stmt = ';'
//      | break_stmt
//      | continue_stmt
//      | do_stmt
//      | expr_stmt
//      | fn_stmt
//      | for_stmt
//      | if_stmt
//      | local_stmt
//      | ret_stmt
//      | while_stmt
static void stmt(riff_parser *y) {
    unset_all();
    switch (TK(0).kind) {
    case ';':       advance();                   break;
    case RIFF_TK_BREAK:  advance(); break_stmt(y);    break;
    case RIFF_TK_CONT:   advance(); continue_stmt(y); break;
    case RIFF_TK_DO:     advance(); do_stmt(y);       break;
    case RIFF_TK_FN:
        peek();
        if (TK_CMP(1, RIFF_TK_IDENT)) {
            advance();
            fn_stmt(y);
        } else {
            expr_stmt(y);
        }
        break;
    case RIFF_TK_FOR:    advance(); for_stmt(y);      break;
    case RIFF_TK_IF:     advance(); if_stmt(y);       break;
    case RIFF_TK_LOCAL:  advance(); local_stmt(y);    break;
    case RIFF_TK_LOOP:   advance(); loop_stmt(y);     break;
    case RIFF_TK_RETURN: advance(); ret_stmt(y);      break;
    case RIFF_TK_WHILE:  advance(); while_stmt(y);    break;
    default:                   expr_stmt(y);     break;
    }

    // Skip token if semicolon used as a statement terminator
    if (TK_CMP(0, ';')) {
        advance();
    }
}

// stmt_list = {stmt}
static void stmt_list(riff_parser *y) {
    while (!(TK_CMP(0, RIFF_TK_EOI) || TK_CMP(0, '}'))) {
        stmt(y);
    }
}

static void y_init(riff_parser *y) {
    unset_all();
    unset(lx);
    y->nlcl = 0;
    y->lcap = 0;
    y->lcl  = NULL;
    y->ld   = 0;
    y->fd   = 0;
    y->id   = 0;
    y->loop = 0;
    y->brk  = NULL;
    y->cont = NULL;
}

int riff_compile(riff_state *s) {
    riff_parser y;
    riff_lexer  x;
    riff_buf    b;
    riff_buf_init(&b);

    y.state = s;
    y.c = s->main.code;
    y.x = &x;
    x.buf = &b;
    riff_lex_init(&x, s->src);
    // Overwrite OP_RET byte if appending to an existing bytecode array.
    if (y.c->n && y.c->code[y.c->n-1] == OP_RET) {
        y.c->n -= 1;
    }
    y_init(&y);
    stmt_list(&y);
    pop_locals(&y, y.ld, 1);
    c_push(y.c, OP_RET);
    riff_buf_free(&b);
    return 0;
}
