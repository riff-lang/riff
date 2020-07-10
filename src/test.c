#include <stdio.h>

#include "str.h"
#include "hash.h"

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

static void h_print(hash_t *h, int c) {
    for (int i = 0; i < c; i++) {
        !h->e[i] ?
            printf("[%d] (empty)\n", i)
        :
            printf("[%d] Key: %s, Value: %lld\n", 
                    i,
                    h->e[i]->key->str,
                    h->e[i]->val->u.i);
    }
}

int main(void) {
    hash_t h;
    h_init(&h);
    for (int j = 0; j < 10; j++) {
        h_insert(&h, s_newstr(keys[j], 4), v_newint(j));
        printf("Iteration %d:\n", j);
        h_print(&h, h.cap);
        printf("\n");
    }
    printf("Final hash:\n");
    for (int j = 0; j < 10; j++) {
        printf("Key: %s, Value: %lld\n",
                keys[j],
                h_lookup(&h, s_newstr(keys[j], 4))->u.i);
    }
    return 0;
}
