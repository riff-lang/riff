#ifndef VM_H
#define VM_H

#include "code.h"
#include "env.h"
#include "hash.h"
#include "types.h"

// TODO dynamic stack allocation
#define STACK_SIZE 1024

// VM stack element
typedef union {
    uint64_t  t; // Implicit type tag
    rf_val   *a;
    rf_val    v;
} rf_stack;

enum loops {
    LOOP_SEQ,
    LOOP_STR,
    LOOP_ARR,
    LOOP_FN
};

typedef struct rf_iter rf_iter;

// Loop iterator
struct rf_iter {
    rf_iter  *p;    // Previous loop iterator
    int       t;    // Loop type
    rf_int    n;    // Control var
    rf_int    on;   // Saved control var for freeing keys allocation
    rf_val   *k;    // Stack slot for `k` in `[k,]v`
    rf_val   *v;    // Stack slot for `v` in `[k,]v`
    rf_val   *keys; // Keys to look up arrays with (fixed at loop start)
    union {
        rf_int      seq;
        const char *str;
        uint8_t    *code;
        rf_arr     *arr;
    } set;
};

int  z_exec(rf_env *);

#endif
