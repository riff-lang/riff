#include <stdio.h>

#include "array.h"
#include "mem.h"

static void err(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void a_init(rf_arr *a) {
    a->n   = 0;
    a->an  = 0;
    a->cap = 0;
    a->v   = NULL;
    a->h   = malloc(sizeof(hash_t));
    h_init(a->h);
}

static int exists(rf_val **v, rf_int k) {
    return v[k] != NULL;
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
// lookup. Otherwise, defer to h_lookup().
static rf_val *a_lookup_int(rf_arr *a, rf_int k, int set) {
    if (k <= a->cap) {
        if (exists(a->v, k))
            return a->v[k];
        else
            return a_insert_int(a, k, v_newnull(), set, 0);
    } else {
        return h_lookup(a->h, s_int2str(k), set);
    }
}

// TODO flt lookup is horribly slow with string conversion
rf_val *a_lookup(rf_arr *a, rf_val *k, int set) {
    switch (k->type) {
    case TYPE_NULL: return a_lookup_str(a, s_int2str(0), set); // TODO
    case TYPE_INT:  return a_lookup_int(a, k->u.i, set);
    case TYPE_FLT:
        if (k->u.f == (rf_int) k->u.f)
            return a_lookup_int(a, (rf_int) k->u.f, set);
        else
            return a_lookup_str(a, s_flt2str(k->u.f), set); // TODO
    case TYPE_STR:  return a_lookup_str(a, k->u.s, set);
    case TYPE_ARR:
    case TYPE_FN: // TODO Error?
    default: break;
    }
    return NULL;
}

static rf_val *a_insert_str(rf_arr *a, rf_str *k, rf_val *v, int set) {
    return h_insert(a->h, k, v, set);
}

// TODO
rf_val *a_insert_int(rf_arr *a, rf_int k, rf_val *v, int set, int force) {
    if (force) {
        if (a->cap <= a->an) {
            a->cap = k <= 0 ? 8 : k + 1;
            a->v = realloc(a->v, sizeof(rf_val *) * a->cap);
        }
        rf_val *nv = malloc(sizeof(rf_val));
        nv->type = v->type;
        nv->u    = v->u;
        a->v[k]  = nv;
        a->an++;
        if (set || !is_null(v))
            a->n++;
        return nv;
    }
    else
        return h_insert(a->h, s_int2str(k), v, set);
}

rf_val *a_insert(rf_arr *a, rf_val *k, rf_val *v, int set) {
    switch (k->type) {
    case TYPE_INT: return a_insert_int(a, k->u.i, v, set, 0);
    case TYPE_FLT:
        if (k->u.f == (rf_int) k->u.f)
            return a_insert_int(a, (rf_int) k->u.f, v, set, 0);
        else
            return a_insert_str(a, s_flt2str(k->u.f), v, set); // TODO
    case TYPE_STR: return a_insert_str(a, k->u.s, v, set);
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
