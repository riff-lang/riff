#include <stdio.h>

#include "str.h"
#include "table.h"

int main(void) {
    char *str = "hello";
    str_t *s = s_newstr(str, 5);
    table_t t;
    val_t *v = v_newint(24);
    t_init(&t);
    t_insert(&t, s, v);
    printf("%lld\n", t_lookup(&t, s)->u.i);
    return 0;
}
