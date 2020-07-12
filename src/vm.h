#ifndef VM_H
#define VM_H

#include "code.h"
#include "hash.h"
#include "types.h"

val_t *z_add(val_t *, val_t *);
val_t *z_sub(val_t *, val_t *);
val_t *z_mul(val_t *, val_t *);
val_t *z_div(val_t *, val_t *);
val_t *z_mod(val_t *, val_t *);
val_t *z_pow(val_t *, val_t *);
val_t *z_and(val_t *, val_t *);
val_t *z_or(val_t *, val_t *);
val_t *z_xor(val_t *, val_t *);
val_t *z_shl(val_t *, val_t *);
val_t *z_shr(val_t *, val_t *);
val_t *z_num(val_t *);
val_t *z_neg(val_t *);
val_t *z_not(val_t *);
val_t *z_eq(val_t *, val_t *);
val_t *z_ne(val_t *, val_t *);
val_t *z_gt(val_t *, val_t *);
val_t *z_ge(val_t *, val_t *);
val_t *z_lt(val_t *, val_t *);
val_t *z_le(val_t *, val_t *);
val_t *z_lnot(val_t *);
val_t *z_len(val_t *);
val_t *z_test(val_t *);
int    z_exec(code_t *);

#endif
