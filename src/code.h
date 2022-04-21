#ifndef CODE_H
#define CODE_H

#include "lex.h"
#include "types.h"

#include <stdint.h>
#include <stdlib.h>

enum opcodes {
#define OPCODE(x,y,z) OP_##x
#include "opcodes.h"
};

enum jumps {
    JMP,    // Unconditional jump
    JZ,     // Pop stack, jump if zero
    JNZ,    // Pop stack, jump if non-zero
    XJZ,    // Pop stack OR jump if zero
    XJNZ    // Pop stack OR jump if non-zero
};

typedef struct {
    uint8_t *code;  // Bytecode array
    rf_val  *k;     // Constants pool

    // TODO array sizes/capacities can be discarded after
    // compilation. If a global constants pool is used at some point
    // in the future, the rf_code array in the rf_env struct can be
    // flattened to an array of bytecode chunks.
    int      n;     // Number of bytes in bytecode array
    int      cap;   // Bytecode array capacity
    int      nk;    // Number of constants in pool
    int      kcap;  // Constants pool capacity
} rf_code;

void c_init(rf_code *);
void c_push(rf_code *, uint8_t);
void c_free(rf_code *);
void c_fn_constant(rf_code *, rf_fn *);
void c_constant(rf_code *, rf_token *);
void c_global(rf_code *, rf_token *, int);
void c_local(rf_code *, int, int);
void c_table(rf_code *, int);
void c_index(rf_code *, int, int);
void c_prefix(rf_code *, int);
void c_infix(rf_code *, int);
void c_postfix(rf_code *, int);
void c_jump(rf_code *, int, int);
void c_loop(rf_code *, int);
void c_range(rf_code *, int, int, int);
void c_patch(rf_code *, int);
int  c_prep_jump(rf_code *, int);
int  c_prep_loop(rf_code *, int);
void c_pop(rf_code *, int);
void c_return(rf_code *, int);

#endif
