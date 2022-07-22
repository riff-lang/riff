#ifndef TABLE_H
#define TABLE_H

#include "types.h"

// NOTE: The "array" part of the table is an array of riff_val pointers, instead
// of a flattened array of riff_val objects. This facilitates the mobility of
// values between the hash table and the "array" part. E.g. The address of a
// riff_val object can be pushed on the VM stack while in the hash table, but
// get evicted and thrown into the array part before the VM actually
// dereferences it.
struct riff_tab {
    riff_val   **v;
    riff_htab   *h;
    riff_val    *nullv;
    uint32_t    lsize;
    uint32_t    psize;
    uint32_t    cap;
    int         nullx: 1;
    int         hint: 1;
};

typedef struct ht_node ht_node;

struct riff_htab {
    ht_node  **nodes;
    uint32_t   lsize;
    uint32_t   psize;
    uint32_t   mask;
    uint32_t   cap;
    int        hint: 1;
};

struct ht_node {
    union {
        riff_str *str;
        riff_val *val;
    } k;
    riff_val *v;
    ht_node  *next;
};

void      riff_tab_init(riff_tab *);
riff_int  riff_tab_logical_size(riff_tab *);
riff_val *riff_tab_collect_keys(riff_tab *);
riff_val *riff_tab_lookup(riff_tab *, riff_val *, int);
riff_val *riff_tab_insert_int(riff_tab *, riff_int, riff_val *);
riff_val *riff_tab_insert(riff_tab *, riff_val *, riff_val *, int);

void      riff_htab_init(riff_htab *);
riff_val *riff_htab_lookup_val(riff_htab *, riff_val *);
riff_val *riff_htab_lookup_str(riff_htab *, riff_str *);
riff_val *riff_htab_insert_val(riff_htab *, riff_val *, riff_val *);
riff_val *riff_htab_insert_str(riff_htab *, riff_str *, riff_val *);
riff_val *riff_htab_insert_cstr(riff_htab *, const char *, riff_val *);

#endif
