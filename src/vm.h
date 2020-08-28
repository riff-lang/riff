#ifndef VM_H
#define VM_H

#include "code.h"
#include "env.h"
#include "hash.h"
#include "types.h"

// TODO dynamic stack allocation
#define STACK_SIZE 1024

typedef union {
    uint64_t  t; // Implicit type tag
    rf_val   *a;
    rf_val    v;
} rf_stack;

int  z_exec(rf_env *);

#endif
