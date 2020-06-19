#ifndef CODE_H
#define CODE_H

#include <stdint.h>
#include <stdlib.h>

#include "types.h"

enum opcodes {
    OP_JMP,     // Jump
    OP_JZ,      // Jump if zero
    OP_JNZ,     // Jump if not zero
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
    OP_GT,      // Greater-than
    OP_GE,      // Greater-than or equal-to
    OP_LT,      // Less-than
    OP_LE,      // Less-than or equal-to
    OP_LAND,    // Logical AND
    OP_LOR,     // Logical OR
    OP_LNOT,    // Logical NOT
    OP_CALL,    // Function call
    OP_CAT,     // Concatenate
    OP_INC,     // Increment
    OP_DEC,     // Decrement
    OP_LEN,     // Length
    OP_POP,     // Pop from stack
    OP_PUSHA,   // Push array on stack
    OP_PUSHI,   // Push immediate value on stack
    OP_PUSHK,   // Push constant value on stack
    OP_PUSHV,   // Push var on stack
    OP_RET,     // Return
    OP_RET0,
    OP_SET,     // Set
    OP_PRINT
};

typedef struct {
    const char   *name;
    int      size;
    int      cap;
    uint8_t *code;
    struct {
        int      size;
        int      cap;
        value_t *k;
    } k; // Constants
} chunk_t;

void c_init(chunk_t *, const char *);
void c_push(chunk_t *, uint8_t);
void c_free(chunk_t *);
uint8_t c_addk(chunk_t *, value_t *);

#endif
