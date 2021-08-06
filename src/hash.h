#ifndef HASH_H
#define HASH_H

#include "types.h"

typedef struct {
    rf_str *key;
    rf_val *val;
} ht_node;

typedef struct {
    ht_node  **nodes;
    uint32_t   n;
    uint32_t   an;
    uint32_t   cap;
    int        lx: 1;
} rf_htbl;

void      h_init(rf_htbl *);
uint32_t  h_length(rf_htbl *);
int       h_exists_int(rf_htbl *, rf_int);
rf_val   *h_lookup(rf_htbl *, rf_str *, int);
rf_val   *h_insert(rf_htbl *, rf_str *, rf_val *, int);
rf_val   *h_delete(rf_htbl *, rf_str *);

#endif
