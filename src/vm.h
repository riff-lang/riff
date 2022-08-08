#ifndef VM_H
#define VM_H

#include "code.h"
#include "state.h"
#include "table.h"
#include "value.h"

// VM stack element
typedef union {
    riff_val  *a;
    riff_val   v;
} vm_stack;

enum loops {
    LOOP_RNG,
    LOOP_STR,
    LOOP_TAB,
    LOOP_NULL
};

typedef struct vm_iter vm_iter;

// Loop iterator
struct vm_iter {
    union {
        riff_int    itvl;
        const char *str;
        uint8_t    *code;
        riff_tab   *tab;
    } set;
    vm_iter   *p;    // Previous loop iterator
    int        t;    // Loop type
    riff_uint  n;    // Control var
    riff_int   on;   // Saved control var for freeing keys allocation
    riff_int   st;   // Start (for ranges)
    riff_val  *k;    // Stack slot for `k` in `[k,]v`
    riff_val  *v;    // Stack slot for `v` in `[k,]v`
    riff_val  *keys; // Keys to look up tables with (fixed at loop start)
};

int vm_exec(riff_state *);
int vm_exec_reenter(riff_state *, vm_stack *);

#endif
