#ifndef CODE_H
#define CODE_H

#include "lex.h"
#include "opcodes.h"
#include "value.h"

#include <stdint.h>
#include <stdlib.h>

#define OPCODE_ENUM(s,a)  OP_##s,
enum riff_opcode {
    OPCODE_DEF(OPCODE_ENUM)
};

enum riff_code_jump {
    JMP,    // Unconditional jump
    JZ,     // Pop stack, jump if zero
    JNZ,    // Pop stack, jump if non-zero
    XJZ,    // Pop stack OR jump if zero
    XJNZ    // Pop stack OR jump if non-zero
};

typedef struct {
    uint8_t  *code;  // Bytecode array
    riff_val *k;     // Constants pool
    int       last;  // Index of the last opcode pushed
    int       n;     // Number of bytes in bytecode array
    int       cap;   // Bytecode array capacity
    int       nk;    // Number of constants in pool
    int       kcap;  // Constants pool capacity
} riff_code;

void c_init(riff_code *);
void c_push(riff_code *, uint8_t);
void c_fn_constant(riff_code *, riff_fn *);
void c_constant(riff_code *, riff_token *);
void c_global(riff_code *, riff_token *, int);
void c_local(riff_code *, int, int, int);
void c_table(riff_code *, int);
void c_index(riff_code *, int, int, int);
void c_str_index(riff_code *, int, riff_str *, int);
void c_fldv_index(riff_code *, int);
void c_call(riff_code *c, int);
void c_prefix(riff_code *, int);
void c_infix(riff_code *, int);
void c_postfix(riff_code *, int);
void c_concat(riff_code *, int);
void c_jump(riff_code *, enum riff_code_jump, int);
void c_loop(riff_code *, int);
void c_range(riff_code *, int, int, int);
void c_patch(riff_code *, int);
int  c_prep_jump(riff_code *, enum riff_code_jump);
int  c_prep_loop(riff_code *, int);
void c_end_loop(riff_code *);
void c_pop(riff_code *, int);
void c_pop_expr_stmt(riff_code *, int);
void c_return(riff_code *, int);

#endif
