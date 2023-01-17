#ifndef CODE_H
#define CODE_H

#include "lex.h"
#include "value.h"

#include <stdint.h>
#include <stdlib.h>

#define OPCODE_DEF(_)                           \
    _(JMP,     1)                               \
    _(JMP16,   2)                               \
    _(JNZ,     1)                               \
    _(JNZ16,   2)                               \
    _(JZ,      1)                               \
    _(JZ16,    2)                               \
    _(XJNZ,    1)                               \
    _(XJNZ16,  2)                               \
    _(XJZ,     1)                               \
    _(XJZ16,   2)                               \
    _(LOOP,    1)  _(LOOP16,  2)                \
    _(POPL,    0)                               \
    _(ITERV,   2)                               \
    _(ITERKV,  2)                               \
    _(LEN,     0)                               \
    _(LNOT,    0)                               \
    _(NEG,     0)                               \
    _(NOT,     0)                               \
    _(NUM,     0)                               \
    _(ADD,     0)                               \
    _(SUB,     0)                               \
    _(MUL,     0)                               \
    _(DIV,     0)                               \
    _(MOD,     0)                               \
    _(POW,     0)                               \
    _(AND,     0)                               \
    _(OR,      0)                               \
    _(XOR,     0)                               \
    _(SHL,     0)                               \
    _(SHR,     0)                               \
    _(EQ,      0)                               \
    _(NE,      0)                               \
    _(GT,      0)                               \
    _(GE,      0)                               \
    _(LT,      0)                               \
    _(LE,      0)                               \
    _(MATCH,   0)                               \
    _(NMATCH,  0)                               \
    _(CAT,     0)                               \
    _(CATI,    1)                               \
    _(VIDXV,   0)                               \
    _(PREINC,  0)                               \
    _(PREDEC,  0)                               \
    _(POSTINC, 0)                               \
    _(POSTDEC, 0)                               \
    _(ADDX,    0)                               \
    _(SUBX,    0)                               \
    _(MULX,    0)                               \
    _(DIVX,    0)                               \
    _(MODX,    0)                               \
    _(CATX,    0)                               \
    _(POWX,    0)                               \
    _(ANDX,    0)                               \
    _(ORX,     0)                               \
    _(SHLX,    0)                               \
    _(SHRX,    0)                               \
    _(XORX,    0)                               \
    _(POP,     0)                               \
    _(POPI,    1)                               \
    _(NULL,    0)                               \
    _(IMM,     1)                               \
    _(IMM16,   2)                               \
    _(ZERO,    0)  _(ONE,     0)                \
    _(CONST,   1)                               \
    _(CONST0,  0)  _(CONST1,  0)  _(CONST2,  0) \
    _(GBLA,    1)                               \
    _(GBLA0,   0)  _(GBLA1,   0)  _(GBLA2,   0) \
    _(GBLV,    1)                               \
    _(GBLV0,   0)  _(GBLV1,   0)  _(GBLV2,   0) \
    _(LCLA,    1)                               \
    _(LCLA0,   0)  _(LCLA1,   0)  _(LCLA2,   0) \
    _(DUPA,    0)                               \
    _(LCLV,    1)                               \
    _(LCLV0,   0)  _(LCLV1,   0)  _(LCLV2,   0) \
    _(TCALL,   1)  _(CALL,    1)                \
    _(RET,     0)                               \
    _(RET1,    0)                               \
    _(TAB0,    0)                               \
    _(TAB,     1)                               \
    _(TABK,    1)                               \
    _(IDXA,    1)                               \
    _(IDXV,    1)                               \
    _(IDXA1,   0)                               \
    _(IDXV1,   0)                               \
    _(SIDXA,   1)                               \
    _(SIDXV,   1)                               \
    _(FLDA,    0)                               \
    _(FLDV,    0)                               \
    _(RNG,     0)                               \
    _(RNGF,    0)                               \
    _(RNGT,    0)                               \
    _(RNGI,    0)                               \
    _(SRNG,    0)                               \
    _(SRNGF,   0)                               \
    _(SRNGT,   0)                               \
    _(SRNGI,   0)                               \
    _(SET,     0)                               \
    _(SETP,    0)                               \

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
