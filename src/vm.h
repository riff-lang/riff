#ifndef VM_H
#define VM_H

#include "code.h"
#include "hash.h"
#include "types.h"

// VM operations exposed for (future) codegen optimizations
void z_add(val_t *, val_t *, val_t *);
void z_sub(val_t *, val_t *, val_t *);
void z_mul(val_t *, val_t *, val_t *);
void z_div(val_t *, val_t *, val_t *);
void z_mod(val_t *, val_t *, val_t *);
void z_pow(val_t *, val_t *, val_t *);
void z_and(val_t *, val_t *, val_t *);
void z_or(val_t *, val_t *, val_t *);
void z_xor(val_t *, val_t *, val_t *);
void z_shl(val_t *, val_t *, val_t *);
void z_shr(val_t *, val_t *, val_t *);
void z_num(val_t *, val_t *);
void z_neg(val_t *, val_t *);
void z_not(val_t *, val_t *);
void z_eq(val_t *, val_t *, val_t *);
void z_ne(val_t *, val_t *, val_t *);
void z_gt(val_t *, val_t *, val_t *);
void z_ge(val_t *, val_t *, val_t *);
void z_lt(val_t *, val_t *, val_t *);
void z_le(val_t *, val_t *, val_t *);
void z_lnot(val_t *, val_t *);
void z_len(val_t *, val_t *);
void z_test(val_t *, val_t *);
void z_cat(val_t *, val_t *, val_t *);
void z_idx(val_t *, val_t *, val_t *);

int  z_exec(code_t *);

#endif
