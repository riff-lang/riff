#ifndef CODE_H
#define CODE_H

#include <stdint.h>
#include <stdlib.h>

#include "lex.h"
#include "val.h"

enum opcodes {
    OP_JMP8,    // Jump (1-byte address)
    OP_JMP16,   // Jump (2-byte address)
    OP_JZ8,     // Jump if zero (1-byte address)
    OP_JZ16,    // Jump if zero (2-byte address)
    OP_ADD,     // Add
    OP_SUB,     // Substract
    OP_MUL,     // Multiply
    OP_DIV,     // Divide
    OP_MOD,     // Modulus
    OP_POW,     // Exponentiation
    OP_AND,     // Bitwise AND
    OP_OR,      // Bitwise OR
    OP_XOR,     // Bitwise XOR
    OP_SHL,     // Bitwise shift left
    OP_SHR,     // Bitwise shift right
    OP_NUM,     // Numeric coercion
    OP_NEG,     // Negate
    OP_NOT,     // Bitwise NOT
    OP_EQ,      // Equality
    OP_NE,      // Not equal
    OP_GT,      // Greater-than
    OP_GE,      // Greater-than or equal-to
    OP_LT,      // Less-than
    OP_LE,      // Less-than or equal-to
    OP_LAND,    // Logical AND
    OP_LOR,     // Logical OR
    OP_LNOT,    // Logical NOT
    OP_CALL,    // Function call
    OP_CAT,     // Concatenate
    OP_PREINC,  // Pre-increment
    OP_PREDEC,  // Pre-decrement
    OP_POSTINC, // Post-increment
    OP_POSTDEC, // Post-decrement
    OP_LEN,     // Length
    OP_POP,     // Pop from stack
    OP_PUSH0,   // Push literal `0` on stack
    OP_PUSH1,   // Push literal `1` on stack
    OP_PUSH2,   // Push literal `2` on stack
    OP_PUSHI,   // Push immediate value on stack
    OP_PUSHK,   // Push constant as literal value on stack
    OP_PUSHS,   // Push symbol on stack
    OP_RET,     // Return
    OP_RET0,
    OP_SET,     // Set
    OP_PRINT
};

typedef struct {
    int      n;
    int      cap;
    uint8_t *code;
    struct {
        int     n;
        int     cap;
        val_t **v;
    } k; // Constants table
} code_t;

void c_init(code_t *);
void c_push(code_t *, uint8_t);
void c_free(code_t *);
void c_constant(code_t *, token_t *);
void c_symbol(code_t *, token_t *);
void c_prefix(code_t *, int);
void c_infix(code_t *, int);
void c_postfix(code_t *, int);
void c_patch(code_t *, int);
int  c_prep_jump(code_t *, int);

#endif
