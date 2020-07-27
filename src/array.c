#include <stdio.h>

#include "array.h"
#include "mem.h"
#include "types.h"

static void err(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void a_init(arr_t *a) {
    // a->n     = 0;
    // a->a.n   = 0;
    // a->a.cap = 0;
    // a->a->v  = NULL;
    a->h = malloc(sizeof(hash_t));
    h_init(a->h);
}

// Source: The Implementation of Lua 5.0, section 4
// The computed size of the array part is the largest n such that at
// least half the slots between 1 and n are in use (to avoid wasting
// space with sparse arrays) and there is at least one used slot
// between n/2 + 1 and n (to avoid a size n when n/2 would do)

static val_t *a_lookup_str(arr_t *a, str_t *k) {
    return h_lookup(a->h, k);
}

// If int k is within the capacity of the "array" part, perform the
// lookup. Otherwise, defer to a_lookup_str().
// static val_t *a_lookup_int(arr_t *a, int_t k) {
// }

val_t *a_lookup(arr_t *a, val_t *k) {
    switch (k->type) {
    case TYPE_VOID: return a_lookup_str(a, s_int2str(0)); // TODO
    case TYPE_INT:  return a_lookup_str(a, s_int2str(k->u.i));
    case TYPE_FLT:  return a_lookup_str(a, s_flt2str(k->u.f)); // TODO
    case TYPE_STR:  return a_lookup_str(a, k->u.s);
    case TYPE_ARR:
    case TYPE_FN: // TODO Error?
    default: break;
    }
    return NULL;
}

static val_t *a_insert_str(arr_t *a, str_t *k, val_t *v) {
    return h_insert(a->h, k, v);
}

val_t *a_insert_int(arr_t *a, int_t k, val_t *v) {
    return h_insert(a->h, s_int2str(k), v);
}

val_t *a_insert(arr_t *a, val_t *k, val_t *v) {
    switch (k->type) {
    case TYPE_INT: return a_insert_str(a, s_int2str(k->u.i), v);
    case TYPE_FLT: return a_insert_str(a, s_flt2str(k->u.f), v); // TODO
    case TYPE_STR: return a_insert_str(a, k->u.s, v);
    case TYPE_ARR:
    case TYPE_FN: // TODO Error?
    default: break;
    }
    return NULL;
}

// TODO
// static void a_delete_str(arr_t *a, str_t *k) {
// }

// TODO
// static void a_delete_int(arr_t *a, int_t k) {
// }

// TODO
void a_delete(arr_t *a, val_t *k) {
}