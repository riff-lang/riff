#ifndef PARSE_H
#define PARSE_H

#include "code.h"
#include "env.h"
#include "lex.h"
#include "types.h"

// Local vars
typedef struct {
    rf_str *id;
    int     d;
} local;

// Patch lists used for break/continue statements in loops
typedef struct {
    int  n;
    int  cap;
    int *l;
} p_list;

typedef struct {
    rf_lexer *x;    // Parser controls lexical analysis
    rf_code  *c;    // Current code object
    rf_env   *e;    // Global environment struct

    int    nlcl;    // Number of locals in scope
    int    lcap;
    local *lcl;     // Array of local vars

    uint8_t fld;    // Depth of '$' expr
    uint8_t pd;     // Parentheses depth
    uint8_t ld;     // Lexical depth/scope
    uint8_t fd;     // Top-level scope of the current function
    uint8_t id;     // Iterator depth (`for` loops only)
    uint8_t sd;     // Subscript depth (exprs inside [])
    uint8_t loop;   // Depth of current loop

    // Flags used when evaluating whether expression statements should
    // be printed
    int lhs: 1;     // Set when leftmost expr has been evaluated
    int ax:  1;     // Assignment flag
    int ox:  1;     // Typical (i.e. not ++/--) operation flag
    int px:  1;     // Prefix or postfix increment/decrement flag
    int ux:  1;     // Unary op?

    int fx:   1;    // For loop flag (parsing `z` in `for x,y in z`)
    int lx:   1;    // Local flag (newly-declared)
    int rx:   1;    // Reference flag - OP_xxA vs OP_xxV instructions
    int retx: 1;    // Return flag

    p_list *brk;    // Patch list for break stmts (current loop)
    p_list *cont;   // Patch list for continue stmts (current loop)
} rf_parser;

int y_compile(rf_env *);

#endif
