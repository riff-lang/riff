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
    OP_POPI,    // Pop IP+1 values from stack (SP -= (IP+1))
    OP_NULL,    // Push null value on stack
    OP_IMM8,    // Push (IP+1) as literal value on stack
    OP_IMM0,    // Push literal `0` on stack
    OP_IMM1,    // Push literal `1` on stack
    OP_IMM2,    // Push literal `2` on stack
    OP_PUSHK,   // Push K[IP+1] on stack as value
    OP_PUSHK0,  // Push K[0] on stack as value
    OP_PUSHK1,  // Push K[1] on stack as value
    OP_PUSHK2,  // Push K[2] on stack as value
    OP_GBLA,    // Push pointer to global var K[IP+1] on stack
    OP_GBLA0,   // Push pointer to global var K[0] on stack
    OP_GBLA1,   // Push pointer to global var K[1] on stack
    OP_GBLA2,   // Push pointer to global var K[2] on stack
    OP_GBLV,    // Copy value of global var K[IP+1] onto stack
    OP_GBLV0,   // Copy value of global var K[0] onto stack
    OP_GBLV1,   // Copy value of global var K[1] onto stack
    OP_GBLV2,   // Copy value of global var K[2] onto stack
    OP_LCL,     // Reserve stack[IP+1] as a local var
    OP_LCL0,    // Reserve stack[0] as a local var
    OP_LCL1,    // Reserve stack[1] as a local var
    OP_LCL2,    // Reserve stack[2] as a local var
    OP_LCLA,    // Push pointer to stack[IP+1] on stack
    OP_LCLA0,   // Push pointer to stack[0] on stack
    OP_LCLA1,   // Push pointer to stack[1] on stack
    OP_LCLA2,   // Push pointer to stack[2] on stack
    OP_LCLV,    // Copy value of stack[IP+1] onto stack
    OP_LCLV0,   // Copy value of stack[0] onto stack
    OP_LCLV1,   // Copy value of stack[1] onto stack
    OP_LCLV2,   // Copy value of stack[2] onto stack
    OP_CALL,    // Function call
    OP_RET,     // Return (stack unmodified)
    OP_RET1,    // Return 1 value from stack top
    OP_ARRAY0,  // Create empty array
    OP_ARRAY,   // Create array of the top (IP+1) stack elements
    OP_ARRAYK,  // Create array of the top K[IP+1] stack elements
    OP_IDXA,    // Index of a set, leaving pointer on stack
    OP_IDXV,    // Index of a set, leaving value on stack
    OP_ARGA,
    OP_ARGV,
    OP_SET,     // Assignment
    OP_PRINT1,  // Print value at SP[-1]; --SP
    OP_PRINT,   // Print (IP+1) values from stack; SP -= (IP+1)
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
    int      n;     // Number of bytes in bytecode array
    int      cap;   // Bytecode array capacity
    uint8_t *code;  // Bytecode array
    int      nk;    // Number of constants in pool
    int      kcap;  // Constants pool capacity
    rf_val  *k;     // Constants pool
} rf_code;

void c_init(rf_code *);
void c_push(rf_code *, uint8_t);
void c_free(rf_code *);
void c_fn_constant(rf_code *, rf_fn *);
void c_constant(rf_code *, rf_token *);
void c_global(rf_code *, rf_token *, int);
void c_local(rf_code *, int, int);
void c_array(rf_code *, int);
void c_prefix(rf_code *, int);
void c_infix(rf_code *, int);
void c_postfix(rf_code *, int);
void c_jump(rf_code *, int, int);
void c_patch(rf_code *, int);
int  c_prep_jump(rf_code *, int);
void c_print(rf_code *, int);

#endif
