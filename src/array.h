#ifndef ARRAY_H
#define ARRAY_H

#include "hash.h"
#include "types.h"

struct arr_t {
    // int n;          // Total (a.n + h->n)
    // struct {
    //     int     n;
    //     int     cap;
    //     val_t **v;
    // } a;
    hash_t *h;
};

void   a_init(arr_t *);
val_t *a_lookup(arr_t *, val_t *);
val_t *a_insert_int(arr_t *, int_t, val_t *);
val_t *a_insert(arr_t *, val_t *, val_t *);
void   a_delete(arr_t *, val_t *);

#endif
