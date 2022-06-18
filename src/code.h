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
// } rf_code;

// typedef struct {
    int      n;     // Number of bytes in bytecode array
    int      cap;   // Bytecode array capacity
    int      nk;    // Number of constants in pool
    int      kcap;  // Constants pool capacity
// } rf_code_md;
} rf_code;

void     c_init(rf_code *);
void     c_push(rf_code *, uint8_t);
uint8_t *c_fn_constant(rf_code *, rf_fn *);
uint8_t *c_constant(rf_code *, rf_token *);
uint8_t *c_global(rf_code *, rf_token *, int);
uint8_t *c_local(rf_code *, int, int, int);
uint8_t *c_table(rf_code *, int);
uint8_t *c_index(rf_code *, uint8_t *, int, int);
uint8_t *c_str_index(rf_code *, uint8_t *, rf_str *, int);
uint8_t *c_prefix(rf_code *, int);
uint8_t *c_infix(rf_code *, int);
uint8_t *c_postfix(rf_code *, int);
uint8_t *c_jump(rf_code *, int, int);
uint8_t *c_loop(rf_code *, int);
uint8_t *c_range(rf_code *, int, int, int);
void     c_patch(rf_code *, int);
int      c_prep_jump(rf_code *, int);
int      c_prep_loop(rf_code *, int);
uint8_t *c_pop(rf_code *, int);
uint8_t *c_pop_expr_stmt(rf_code *, int);
uint8_t *c_return(rf_code *, uint8_t *, int);

#endif
