#ifndef TABLE_H
#define TABLE_H

#include "str.h"
#include "val.h"

typedef struct {
    str_t *key;
    val_t *val;
} entry_t;

typedef struct {
    int       n;
    int       cap;
    entry_t **e;
} table_t;

void   t_init(table_t *);
val_t *t_lookup(table_t *, str_t *);
void   t_insert(table_t *, str_t *, val_t *);
void   t_delete(table_t *, str_t *);

#endif
