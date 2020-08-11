#ifndef ARRAY_H
#define ARRAY_H

#include "hash.h"
#include "types.h"

struct rf_arr {
    // int n;          // Total (a.n + h->n)
    // struct {
    //     int     n;
    //     int     cap;
    //     rf_val **v;
    // } a;
    hash_t *h;
};

void    a_init(rf_arr *);
rf_val *a_lookup(rf_arr *, rf_val *, int);
rf_val *a_insert_int(rf_arr *, rf_int, rf_val *);
rf_val *a_insert(rf_arr *, rf_val *, rf_val *);
void    a_delete(rf_arr *, rf_val *);

#endif
