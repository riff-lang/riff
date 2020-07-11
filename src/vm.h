#ifndef VM_H
#define VM_H

#include "code.h"
#include "hash.h"
#include "types.h"

val_t *z_add(val_t *, val_t *);
val_t *z_sub(val_t *, val_t *);
int    z_exec(code_t *);

#endif
