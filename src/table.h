#ifndef TABLE_H
#define TABLE_H

#include "types.h"

// NOTE: The "array" part of the table is an array of rf_val pointers,
// instead of a flattened array of rf_val objects. This facilitates
// the mobility of values between the hash table and the "array" part.
// E.g. The address of a rf_val object can be pushed on the VM stack
// while in the hash table, but get evicted and thrown into the array
// part before the VM actually dereferences it.
struct rf_tab {
    rf_val   **v;
    rf_htab   *h;
    rf_val    *nullv;
    uint32_t   lsize;
    uint32_t   psize;
    uint32_t   cap;
    int        nullx: 1;
    int        hint: 1;
};

typedef struct ht_node ht_node;

struct rf_htab {
    ht_node  **nodes;
    uint32_t   lsize;
    uint32_t   psize;
    uint32_t   mask;
    uint32_t   cap;
    int        hint: 1;
};

struct ht_node {
    union {
        rf_str *str;
        rf_val *val;
    } k;
    rf_val  *v;
    ht_node *next;
};

void    t_init(rf_tab *);
rf_int  t_logical_size(rf_tab *);
rf_val *t_collect_keys(rf_tab *);
rf_val *t_lookup(rf_tab *, rf_val *, int);
rf_val *t_insert_int(rf_tab *, rf_int, rf_val *);
rf_val *t_insert(rf_tab *, rf_val *, rf_val *, int);

void    ht_init(rf_htab *);
rf_val *ht_lookup_val(rf_htab *, rf_val *);
rf_val *ht_lookup_str(rf_htab *, rf_str *);
rf_val *ht_insert_val(rf_htab *, rf_val *, rf_val *);
rf_val *ht_insert_str(rf_htab *, rf_str *, rf_val *);
rf_val *ht_insert_cstr(rf_htab *, const char *, rf_val *);

#endif
