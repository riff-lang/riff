#ifndef CODE_H
#define CODE_H

#include <stdint.h>
#include <stdlib.h>

#include "lex.h"
#include "types.h"

enum opcodes {
    OP_JMP8,    // Jump (1-byte offset)
    OP_JMP16,   // Jump (2-byte offset)
    OP_JNZ8,    // Jump if non-zero (1-byte offset)
    OP_JNZ16,   // Jump if non-zero (2-byte offset)
    OP_JZ8,     // Jump if zero (1-byte offset)
    OP_JZ16,    // Jump if zero (2-byte offset)
    OP_XJNZ8,   // "Exclusive" jump if non-zero (1-byte offset)
    OP_XJNZ16,  // "Exclusive" jump if non-zero (2-byte offset)
    OP_XJZ8,    // "Exclusive" jump if zero (1-byte offset)
    OP_XJZ16,   // "Exclusive" jump if zero (2-byte offset)
    OP_TEST,    // Logical test
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
    OP_LNOT,    // Logical NOT
    OP_CAT,     // Concatenate
    OP_CALL,    // Function call
    OP_PREINC,  // Pre-increment
    OP_PREDEC,  // Pre-decrement
    OP_POSTINC, // Post-increment
    OP_POSTDEC, // Post-decrement
    OP_LEN,     // Length
    OP_ADDX,    // Add assign
    OP_SUBX,    // Subtract assign
    OP_MULX,    // Multiply assign
    OP_DIVX,    // Divide assign
    OP_MODX,    // Modulus assign
    OP_CATX,    // Concatenate assign
    OP_POWX,    // Exponentiation assign
    OP_ANDX,    // Bitwise AND assign
    OP_ORX,     // Bitwise OR assign
    OP_SHLX,    // Bitwise shift left assign
    OP_SHRX,    // Bitwise shift right assign
    OP_XORX,    // Bitwise XOR assign
    OP_POP,     // Pop (--SP)
    OP_PUSHI,   // Push (IP+1) as literal value on stack
    OP_PUSH0,   // Push literal `0` on stack
    OP_PUSH1,   // Push literal `1` on stack
    OP_PUSH2,   // Push literal `2` on stack
    OP_PUSHK,   // Push K[IP+1] on stack as value
    OP_PUSHK0,  // Push K[0] on stack as value
    OP_PUSHK1,  // Push K[1] on stack as value
    OP_PUSHK2,  // Push K[2] on stack as value
    OP_PUSHA,   // Push pointer to symbol K[IP+1] on stack
    OP_PUSHA0,  // Push pointer to symbol K[0] on stack
    OP_PUSHA1,  // Push pointer to symbol K[1] on stack
    OP_PUSHA2,  // Push pointer to symbol K[2] on stack
    OP_PUSHV,   // Copy value of symbol K[IP+1] onto stack
    OP_PUSHV0,  // Copy value of symbol K[0] onto stack
    OP_PUSHV1,  // Copy value of symbol K[1] onto stack
    OP_PUSHV2,  // Copy value of symbol K[2] onto stack
    OP_RET,     // Return (stack unmodified)
    OP_RET1,    // Return 1 value from stack top
    OP_IDXA,    // Index of a set, leaving pointer on stack
    OP_IDXV,    // Index of a set, leaving value on stack
    OP_SET,     // Assignment
    OP_PRINT,   // Print value at SP[-1], --SP
    OP_EXIT     // exit(0) (maybe temporary)
};

enum jumps {
    JMP,        // Unconditional jump
    JZ,         // Pop stack, jump if zero
    JNZ,        // Pop stack, jump if non-zero
    XJZ,        // Pop stack OR jump if zero
    XJNZ        // Pop stack OR jump if non-zero
};

typedef struct {
    int      n;
    int      cap;
    uint8_t *code;
    struct {
        int     n;
        int     cap;
        val_t **v;
    } k;        // Constants table
} code_t;

void c_init(code_t *);
void c_push(code_t *, uint8_t);
void c_free(code_t *);
void c_constant(code_t *, token_t *);
void c_symbol(code_t *, token_t *, int);
void c_prefix(code_t *, int);
void c_infix(code_t *, int);
void c_postfix(code_t *, int);
void c_jump(code_t *, int, int);
void c_patch(code_t *, int);
int  c_prep_jump(code_t *, int);

#endif
