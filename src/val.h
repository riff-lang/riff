#ifndef VAL_H
#define VAL_H

#include <stdint.h>
#include <stdlib.h>

#include "str.h"
#include "types.h"

typedef struct {
    uint8_t type;
    union {
        flt_t  f;
        int_t  i;
        str_t *s;
        // Add array
        // Add function
    } u;
} val_t;

val_t *v_newint(int_t);
val_t *v_newflt(flt_t);
val_t *v_newstr(str_t *);

#endif
