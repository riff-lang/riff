#ifndef TABLE_H
#define TABLE_H

#include "hash.h"
#include "types.h"

struct rf_tbl {
    rf_val   **v;
    rf_htbl   *h;
    rf_val    *nullv;    // Special slot for the "null" index in an array
    uint32_t   n;        // Number of elements (excluding null values)
    uint32_t   an;       // Number of elements (including null values)
    uint32_t   cap;
    int        nullx: 1; // null flag ("null" index set?)
    int        lx:    1; // Re-calculate length?
};

void    t_init(rf_tbl *);
rf_int  t_length(rf_tbl *);
rf_val *t_collect_keys(rf_tbl *);
rf_val *t_lookup(rf_tbl *, rf_val *, int);
rf_val *t_insert_int(rf_tbl *, rf_int, rf_val *, int, int);
rf_val *t_insert(rf_tbl *, rf_val *, rf_val *, int);

#endif
