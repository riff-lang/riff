#include <stdio.h>

#include "str.h"
#include "table.h"

static const char *keys[] = {
    "sds0",
    "sff1",
    "lll2",
    "hgf3",
    "kfs4",
    "0df5",
    "ghn6",
    "qpx7",
    "ipc8",
    "dpa9"
    // "key0",
    // "key1",
    // "key2",
    // "key3",
    // "key4",
    // "key5",
    // "key6",
    // "key7",
    // "key8",
    // "key9"
};

static void t_print(table_t *t, int c) {
    for (int i = 0; i < c; i++) {
        if (!t->e[i])
            printf("[%d] (empty)\n", i);
        else
            printf("[%d] Key: %s, Value: %lld\n", 
                    i,
                    t->e[i]->key->str,
                    t->e[i]->val->u.i);
    }
}

int main(void) {
    table_t t;
    t_init(&t);
    for (int j = 0; j < 10; j++) {
        t_insert(&t, s_newstr(keys[j], 4), v_newint(j));
        printf("Iteration %d:\n", j);
        t_print(&t, t.cap);
        printf("\n");
    }
    printf("Final table:\n");
    for (int j = 0; j < 10; j++) {
        printf("Key: %s, Value: %lld\n",
                keys[j],
                t_lookup(&t, s_newstr(keys[j], 4))->u.i);
    }
    return 0;
}
