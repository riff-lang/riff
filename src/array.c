#include <stdio.h>

#include "array.h"
#include "mem.h"
#include "types.h"

static void err(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void a_init(rf_arr *a) {
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

static rf_val *a_lookup_str(rf_arr *a, rf_str *k, int set) {
    return h_lookup(a->h, k, set);
}

// If int k is within the capacity of the "array" part, perform the
// lookup. Otherwise, defer to a_lookup_str().
// static rf_val *a_lookup_int(rf_arr *a, rf_int k) {
// }

rf_val *a_lookup(rf_arr *a, rf_val *k, int set) {
    switch (k->type) {
    case TYPE_NULL: return a_lookup_str(a, s_int2str(0), set); // TODO
    case TYPE_INT:  return a_lookup_str(a, s_int2str(k->u.i), set);
    case TYPE_FLT:  return a_lookup_str(a, s_flt2str(k->u.f), set); // TODO
    case TYPE_STR:  return a_lookup_str(a, k->u.s, set);
    case TYPE_ARR:
    case TYPE_FN: // TODO Error?
    default: break;
    }
    return NULL;
}

static rf_val *a_insert_str(rf_arr *a, rf_str *k, rf_val *v) {
    return h_insert(a->h, k, v, 1);
}

rf_val *a_insert_int(rf_arr *a, rf_int k, rf_val *v) {
    return h_insert(a->h, s_int2str(k), v, 1);
}

rf_val *a_insert(rf_arr *a, rf_val *k, rf_val *v) {
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
// static void a_delete_str(rf_arr *a, rf_str *k) {
// }

// TODO
// static void a_delete_int(rf_arr *a, rf_int k) {
// }

// TODO
void a_delete(rf_arr *a, rf_val *k) {
}
