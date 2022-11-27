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
    LOOP_RANGE_KV,
    LOOP_RANGE_V,
    LOOP_STR_KV,
    LOOP_STR_V,
    LOOP_TAB_KV,
    LOOP_TAB_V,
    LOOP_NULL
};

typedef struct vm_iter vm_iter;

// Loop iterator
struct vm_iter {
    int        t;    // Loop type
    riff_uint  n;    // Control var
    riff_val  *v;    // Stack slot for `v` in `[k,]v`
    riff_val  *k;    // Stack slot for `k` in `[k,]v`
    union {
        struct {
            riff_val  *kp;   // Keys pointer
            riff_tab  *tab;
            riff_val  *keys; // Keys to look up tables with (fixed at loop start)
        };
        struct {
            riff_int   itvl;
            riff_int   st;   // Start (for ranges)
        };
        const char *str;
    };
    vm_iter   *p;    // Previous loop iterator
};

int vm_exec(riff_state *);
int vm_exec_reenter(riff_state *, vm_stack *);

#endif
