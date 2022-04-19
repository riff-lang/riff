#ifndef VM_H
#define VM_H

#include "code.h"
#include "env.h"
#include "table.h"
#include "types.h"

// VM stack element
typedef union {
    uintptr_t  t; // Implicit type tag
    rf_val    *a;
    rf_val     v;
} rf_stack;

enum loops {
    LOOP_RNG,
    LOOP_STR,
    LOOP_TAB,
    LOOP_FN
};

typedef struct rf_iter rf_iter;

// Loop iterator
struct rf_iter {
    union {
        rf_int      itvl;
        const char *str;
        uint8_t    *code;
        rf_tab     *tab;
    } set;
    rf_iter *p;    // Previous loop iterator
    int      t;    // Loop type
    rf_uint  n;    // Control var
    rf_int   on;   // Saved control var for freeing keys allocation
    rf_int   st;   // Start (for ranges)
    rf_val  *k;    // Stack slot for `k` in `[k,]v`
    rf_val  *v;    // Stack slot for `v` in `[k,]v`
    rf_val  *keys; // Keys to look up tables with (fixed at loop start)
};

int z_exec(rf_env *);
int z_exec_reenter(rf_env *, rf_stack *);

#endif
