#ifndef VM_H
#define VM_H

#include "code.h"
#include "env.h"
#include "hash.h"
#include "types.h"

// TODO dynamic stack allocation
#define STACK_SIZE 1024

// VM operations exposed for (future) codegen optimizations
void z_add(rf_val *, rf_val *);
void z_sub(rf_val *, rf_val *);
void z_mul(rf_val *, rf_val *);
void z_div(rf_val *, rf_val *);
void z_mod(rf_val *, rf_val *);
void z_pow(rf_val *, rf_val *);
void z_and(rf_val *, rf_val *);
void z_or(rf_val *, rf_val *);
void z_xor(rf_val *, rf_val *);
void z_shl(rf_val *, rf_val *);
void z_shr(rf_val *, rf_val *);
void z_num(rf_val *);
void z_neg(rf_val *);
void z_not(rf_val *);
void z_eq(rf_val *, rf_val *);
void z_ne(rf_val *, rf_val *);
void z_gt(rf_val *, rf_val *);
void z_ge(rf_val *, rf_val *);
void z_lt(rf_val *, rf_val *);
void z_le(rf_val *, rf_val *);
void z_lnot(rf_val *);
void z_len(rf_val *);
void z_test(rf_val *);
void z_cat(rf_val *, rf_val *);
void z_idx(rf_val *, rf_val *);

int  z_exec(rf_env *);

#endif
