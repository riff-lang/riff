#ifndef HASH_H
#define HASH_H

#include "types.h"

typedef struct {
    str_t *key;
    val_t *val;
} entry_t;

typedef struct {
    int       n;    // Number of elements (excluding null values)
    int       an;   // Number of elements (including null values)
    int       cap;
    entry_t **e;
} hash_t;

void   h_init(hash_t *);
val_t *h_lookup(hash_t *, str_t *);
val_t *h_insert(hash_t *, str_t *, val_t *);
void   h_delete(hash_t *, str_t *);

#endif
