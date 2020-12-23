#include <stdio.h>

#include "array.h"
#include "mem.h"

#define set(f)   a->f = 1
#define unset(f) a->f = 0

#define MIN_LOAD_FACTOR 0.5

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
// the array. This accomodates "deletion" via `null` assignment.
rf_int a_length(rf_arr *a) {
    if (!a->lx)
        return a->n + h_length(a->h);
    rf_int l = 0;
    for (int i = 0; i < a->cap; ++i) {
        if (a->v[i] && !is_null(a->v[i]))
            ++l;
    }
    // Include special "null" index
    l += (a->nullx && !is_null(a->nullv));
    a->n = l;
    unset(lx);
    return l + h_length(a->h);
}

static int exists(rf_arr *a, rf_int k) {
    return k < a->cap && a->v[k] != NULL;
}

rf_val *a_collect_keys(rf_arr *a) {
    rf_int len = a_length(a);
    rf_val *keys = malloc(len * sizeof(rf_val));
    int n = 0;
    for (rf_int i = 0; i < a->cap && n <= len; ++i) {
        if (exists(a, i) && !is_null(a->v[i])) {
            keys[n++] = (rf_val) {TYPE_INT, .u.i = i};
        }
    }
    hash_t *h = a->h;
    for (int i = 0; i < h->cap && n <= len; ++i) {
        if (h->e[i] && !is_null(h->e[i]->val)) {
            keys[n++] = (rf_val) {TYPE_STR, .u.s = h->e[i]->key};
        }
    }
    if (a->nullx)
        keys[n++] = (rf_val) {TYPE_NULL, .u.i = 0};
    return keys;
}

static double potential_lf(int n, int cap, rf_int k) {
    double n1 = n + 1;
    double n2 = cap > k ? cap : k + 1;
    return n1 / n2;
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
    if (exists(a, k)) {
        return a->v[k];
    }
    if (!a->cap) {
        return a_insert_int(a, k, v_newnull(), set, 0);
    }
    if (k < a->cap ||
        (potential_lf(a->an, a->cap, k) >= MIN_LOAD_FACTOR)) {
        return a_insert_int(a, k, v_newnull(), set, 0);
    } else {
        rf_str *ik = s_int2str(k);
        rf_val *v  = h_lookup(a->h, ik, set);
        m_freestr(ik);
        return v;
    }
}

// If the entire string is is a valid integer, return the number +
// offset.
static rf_int str2intidx(rf_str *s, int offset) {
    char *end;
    rf_flt f = strtod(s->str, &end);
    rf_int i = (rf_int) f;
    if (f == i && *end == '\0')
        return i + offset;
    return -1;
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
        if ((k->u.i + offset) < 0) {
            rf_str *ik = s_int2str(k->u.i + offset);
            rf_val *v  = h_lookup(a->h, ik, set);
            m_freestr(ik);
            return v;
        }
        else
            return a_lookup_int(a, k->u.i + offset, set);
    case TYPE_FLT:
        if ((k->u.f == (rf_int) k->u.f) &&
            (((rf_int) k->u.f + offset) >= 0))
            return a_lookup_int(a, (rf_int) k->u.f + offset, set);
        else {
            rf_str *fk = s_flt2str(k->u.f + offset);
            rf_val *v  = h_lookup(a->h, fk, set);
            m_freestr(fk);
            return v;
        }
    case TYPE_STR: {
        rf_int si = str2intidx(k->u.s, offset);
        return si >= 0 ? a_lookup_int(a, si, set)
                       : h_lookup(a->h, k->u.s, set);

    }
    // TODO monitor
    case TYPE_ARR: {
        rf_str *ak = s_int2str((rf_int) k->u.a);
        rf_val *v  = h_lookup(a->h, s_int2str((rf_int) k->u.a), set);
        m_freestr(ak);
        return v;
    }
    case TYPE_RFN: // TODO
    default: break;
    }
    return NULL;
}

static int new_size(int n, int cap, rf_int k) {
    int sz = cap < k ? k + 1 : cap + 1;
    while (potential_lf(n, sz, k) >= MIN_LOAD_FACTOR) {
        ++sz;
    }
    return sz + 1;
}

// VM currently initializes sequential arrays backwards, inserting the
// last element first by calling a_insert_int() with `force` set to 1.
// This allows memory to be allocated once with the exact size needed
// instead of potentially resizing multiple times throughout
// initialization. Otherwise, a_insert_int() can defer to the hash
// table part of the rf_arr if the rf_int `k` would cause the array
// part to be too sparsely populated.
rf_val *a_insert_int(rf_arr *a, rf_int k, rf_val *v, int set, int force) {
    if (set) set(lx);
    double lf = potential_lf(a->an, a->cap, k);
    if (force || (lf >= MIN_LOAD_FACTOR)) {
        if (a->cap <= k) {
            set(lx);
            int oc = a->cap;
            int nc = force ? k + 1 : new_size(a->an, a->cap, k);
            // TODO the hackiest hack that ever hacked - maybe keep
            // track of integer keys in the hash part instead
            nc += (!force * h_length(a->h)) + 1;
            a->v = realloc(a->v, sizeof(rf_val *) * nc);

            // Collect valid integer keys from the hash part and move
            // them to the newly allocated array part
            for (int i = oc; i < nc; ++i) {
                if (exists(a,i)) continue;
                rf_str *ik = s_int2str(i);
                a->v[i] = h_delete(a->h, ik);
                if (a->v[i]) a->an++;
                m_freestr(ik);
            }
            a->cap = nc;
        }
        if (!exists(a, k)) {
            rf_val *nv = malloc(sizeof(rf_val));
            *nv     = *v;
            a->v[k] = nv;
            a->an++;
            if (set && !is_null(v))
                a->n++;
        } else {
            *a->v[k] = *v;
        }
        return a->v[k];
    }
    else {
        rf_str *ik = s_int2str(k);
        rf_val *rv = h_lookup(a->h, ik, set); // TODO?
        m_freestr(ik);
        return rv;
    }
}

rf_val *a_insert(rf_arr *a, rf_val *k, rf_val *v, int set) {
    if (set) set(lx);
    switch (k->type) {
    case TYPE_NULL:
        *a->nullv = *v;
        if (!is_null(v)) a->n++;
        return a->nullv;
    case TYPE_INT: return a_insert_int(a, k->u.i, v, set, 0);
    case TYPE_FLT:
        if (k->u.f == (rf_int) k->u.f)
            return a_insert_int(a, (rf_int) k->u.f, v, set, 0);
        else {
            rf_str *fk = s_flt2str(k->u.f);
            rf_val *rv = h_insert(a->h, fk, v, set);
            m_freestr(fk);
            return rv;
        }
    case TYPE_STR: return h_insert(a->h, k->u.s, v, set);

    // TODO monitor
    case TYPE_ARR: {
        rf_str *ak = s_int2str((rf_int) k->u.a);
        rf_val *rv = h_insert(a->h, ak, v, set);
        m_freestr(ak);
        return rv;
    }
    case TYPE_RFN: // TODO
    default: break;
    }
    return NULL;
}

#undef set
#undef unset
#undef MIN_LOAD_FACTOR
