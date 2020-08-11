#ifndef HASH_H
#define HASH_H

#include "types.h"

typedef struct {
    rf_str *key;
    rf_val *val;
} entry_t;

typedef struct {
    int       n;    // Number of elements (excluding null values)
    int       an;   // Number of elements (including null values)
    int       cap;
    entry_t **e;
} hash_t;

void    h_init(hash_t *);
rf_val *h_lookup(hash_t *, rf_str *, int);
rf_val *h_insert(hash_t *, rf_str *, rf_val *, int);
void    h_delete(hash_t *, rf_str *);

#endif
