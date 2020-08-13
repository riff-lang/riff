#include <stdio.h>

#include "array.h"
#include "mem.h"

#define set(f)   a->f = 1
#define unset(f) a->f = 0

static void err(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void a_init(rf_arr *a) {
    unset(nullx);
    unset(lx);
    a->n     = 0;
    a->an    = 0;
    a->cap   = 0;
    a->nullv = v_newnull();
    a->v     = NULL;
    a->h     = malloc(sizeof(hash_t));
    h_init(a->h);
}

// lx flag is set whenever the VM performs a lookup with the intent on
// setting the value (OP_xxA). When lx is set, recalculate length of
// the array. This accomodates "deletion" via assigning `null`.
rf_int a_length(rf_arr *a) {
    if (!a->lx)
        return a->n + h_length(a->h);
    rf_int l = 0;
    for (int i = 0; i < a->cap; ++i) {
        if (a->v[i] && !is_null(a->v[i]))
            ++l;
    }
    // Don't forget special "null" index
    l += (a->nullx && !is_null(a->nullv));
    a->n = l;
    unset(lx);
    return l + h_length(a->h);
}

static int exists(rf_val **v, rf_int k) {
    return v[k] != NULL;
}

// Source: The Implementation of Lua 5.0, section 4
// The computed size of the array part is the largest n such that at
// least half the slots between 1 and n are in use (to avoid wasting
// space with sparse arrays) and there is at least one used slot
// between n/2 + 1 and n (to avoid a size n when n/2 would do)

// If int k is within the capacity of the "array" part, perform the
// lookup. Otherwise, defer to h_lookup().
static rf_val *a_lookup_int(rf_arr *a, rf_int k, int set) {
    if (set) set(lx);
    if (k <= a->cap) {
        if (exists(a->v, k))
            return a->v[k];
        else
            return a_insert_int(a, k, v_newnull(), set, 1);
    } else {
        return h_lookup(a->h, s_int2str(k), set);
    }
}

// TODO flt lookup is slow with string conversion
// offset parameter used for utilizing negative indices with the
// default array/argv without advancing the raw pointer (realloc
// issues) or converting the indices to strings.
rf_val *a_lookup(rf_arr *a, rf_val *k, int set, int offset) {
    if (set) set(lx);
    switch (k->type) {
    case TYPE_NULL:
        if (set && !a->nullx) {
            set(nullx);
            a->n++;
        }
        return a->nullv;
    case TYPE_INT:
        if ((k->u.i + offset) < 0)
            return h_lookup(a->h, s_int2str(k->u.i + offset), set);
        else
            return a_lookup_int(a, k->u.i + offset, set);
    case TYPE_FLT:
        if ((k->u.f == (rf_int) k->u.f) &&
            (((rf_int) k->u.f + offset) >= 0))
            return a_lookup_int(a, (rf_int) k->u.f + offset, set);
        else
            return h_lookup(a->h, s_flt2str(k->u.f + offset), set); // TODO?
    case TYPE_STR:  return h_lookup(a->h, k->u.s, set);

    // TODO monitor
    case TYPE_ARR:
        printf("%llx\n", (rf_int) k->u.a);
        return h_lookup(a->h, s_int2str((rf_int) k->u.a), set);
    case TYPE_FN: // TODO
    default: break;
    }
    return NULL;
}

// TODO
rf_val *a_insert_int(rf_arr *a, rf_int k, rf_val *v, int set, int force) {
    if (force) {
        if (a->cap <= a->an) {
            a->cap = k <= 0 ? 8 : k + 1;
            a->v = realloc(a->v, sizeof(rf_val *) * a->cap);
            for (int i = 0; i < a->cap; ++i)
                a->v[i] = NULL;
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
    case TYPE_NULL:
        a->nullv->type = v->type;
        a->nullv->u    = v->u;
        if (!is_null(v)) a->n++;
        return a->nullv;
    case TYPE_INT: return a_insert_int(a, k->u.i, v, set, 0);
    case TYPE_FLT:
        if (k->u.f == (rf_int) k->u.f)
            return a_insert_int(a, (rf_int) k->u.f, v, set, 0);
        else
            return h_insert(a->h, s_flt2str(k->u.f), v, set); // TODO
    case TYPE_STR: return h_insert(a->h, k->u.s, v, set);

    // TODO monitor
    case TYPE_ARR:
        return h_insert(a->h, s_int2str((rf_int) k->u.a), v, set);
    case TYPE_FN: // TODO
    default: break;
    }
    return NULL;
}

#undef set
#undef unset
