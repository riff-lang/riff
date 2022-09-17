#ifndef CODE_H
#define CODE_H

#include "lex.h"
#include "value.h"

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
    uint8_t  *code;  // Bytecode array
    riff_val *k;     // Constants pool
    int       n;     // Number of bytes in bytecode array
    int       cap;   // Bytecode array capacity
    int       nk;    // Number of constants in pool
    int       kcap;  // Constants pool capacity
} riff_code;

void c_init(riff_code *);
void c_push(riff_code *, uint8_t);
void c_fn_constant(riff_code *, riff_fn *, uint8_t **);
void c_constant(riff_code *, riff_token *, uint8_t **);
void c_global(riff_code *, riff_token *, int, uint8_t **);
void c_local(riff_code *, int, int, int, uint8_t **);
void c_table(riff_code *, int, uint8_t **);
void c_index(riff_code *, uint8_t *, int, int, uint8_t **);
void c_str_index(riff_code *, uint8_t *, riff_str *, int, uint8_t **);
void c_prefix(riff_code *, int, uint8_t **);
void c_infix(riff_code *, int, uint8_t **);
void c_postfix(riff_code *, int, uint8_t **);
void c_concat(riff_code *, int, uint8_t **);
void c_jump(riff_code *, int, int, uint8_t **);
void c_loop(riff_code *, int, uint8_t **);
void c_range(riff_code *, int, int, int, uint8_t **);
void c_patch(riff_code *, int);
int  c_prep_jump(riff_code *, int);
int  c_prep_loop(riff_code *, int);
void c_end_loop(riff_code *, uint8_t **);
void c_pop(riff_code *, int, uint8_t **);
void c_pop_expr_stmt(riff_code *, int, uint8_t **);
void c_return(riff_code *, uint8_t *, int, uint8_t **);

#endif
