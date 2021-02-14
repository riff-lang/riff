#include <stdio.h>

#include "table.h"
#include "mem.h"

#define set(f)   t->f = 1
#define unset(f) t->f = 0

#define MIN_LOAD_FACTOR 0.5

static void err(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void t_init(rf_tbl *t) {
    unset(nullx);
    unset(lx);
    t->n     = 0;
    t->an    = 0;
    t->cap   = 0;
    t->nullv = v_newnull();
    t->v     = NULL;
    t->h     = malloc(sizeof(hash_t));
    h_init(t->h);
}

// lx flag is set whenever the VM performs a lookup with the intent on
// setting the value (OP_xxA). When lx is set, recalculate length of
// the array. This accomodates "deletion" via `null` assignment.
rf_int t_length(rf_tbl *t) {
    if (!t->lx)
        return t->n + h_length(t->h);
    rf_int l = 0;
    for (int i = 0; i < t->cap; ++i) {
        if (t->v[i] && !is_null(t->v[i]))
            ++l;
    }
    // Include special "null" index
    l += (t->nullx && !is_null(t->nullv));
    t->n = l;
    unset(lx);
    return l + h_length(t->h);
}

static int exists(rf_tbl *t, rf_int k) {
    return k < t->cap && t->v[k] != NULL;
}

rf_val *t_collect_keys(rf_tbl *t) {
    rf_int len = t_length(t);
    rf_val *keys = malloc(len * sizeof(rf_val));
    int n = 0;
    for (rf_int i = 0; i < t->cap && n <= len; ++i) {
        if (exists(t, i) && !is_null(t->v[i])) {
            keys[n++] = (rf_val) {TYPE_INT, .u.i = i};
        }
    }
    hash_t *h = t->h;
    for (int i = 0; i < h->cap && n <= len; ++i) {
        if (h->e[i] && !is_null(h->e[i]->val)) {
            keys[n++] = (rf_val) {TYPE_STR, .u.s = h->e[i]->key};
        }
    }
    if (t->nullx)
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
static rf_val *t_lookup_int(rf_tbl *t, rf_int k, int set) {
    if (set) set(lx);
    if (exists(t, k)) {
        return t->v[k];
    }
    if (!t->cap) {
        return t_insert_int(t, k, v_newnull(), set, 0);
    }
    if (k < t->cap ||
        (potential_lf(t->an, t->cap, k) >= MIN_LOAD_FACTOR)) {
        return t_insert_int(t, k, v_newnull(), set, 0);
    } else {
        rf_str *ik = s_int2str(k);
        rf_val *v  = h_lookup(t->h, ik, set);
        m_freestr(ik);
        return v;
    }
}

// If the entire string is is a valid integer, return the number +
// offset.
static rf_int str2intidx(rf_str *s, int offset) {
    char *end;
    rf_flt f = u_str2d(s->str, &end, 0);
    rf_int i = (rf_int) f;
    if (f == i && *end == '\0')
        return i + offset;
    return -1;
}

// TODO flt lookup is slow with string conversion
// offset parameter used for utilizing negative indices with the
// default array/argv without advancing the raw pointer (realloc
// issues) or converting the indices to strings.
rf_val *t_lookup(rf_tbl *t, rf_val *k, int set, int offset) {
    if (set) set(lx);
    switch (k->type) {
    case TYPE_NULL:
        if (set && !t->nullx) {
            set(nullx);
            t->n++;
        }
        return t->nullv;
    case TYPE_INT:
        if ((k->u.i + offset) < 0) {
            rf_str *ik = s_int2str(k->u.i + offset);
            rf_val *v  = h_lookup(t->h, ik, set);
            m_freestr(ik);
            return v;
        }
        else
            return t_lookup_int(t, k->u.i + offset, set);
    case TYPE_FLT:
        if ((k->u.f == (rf_int) k->u.f) &&
            (((rf_int) k->u.f + offset) >= 0))
            return t_lookup_int(t, (rf_int) k->u.f + offset, set);
        else {
            rf_str *fk = s_flt2str(k->u.f + offset);
            rf_val *v  = h_lookup(t->h, fk, set);
            m_freestr(fk);
            return v;
        }
    case TYPE_STR: {
        rf_int si = str2intidx(k->u.s, offset);
        return si >= 0 ? t_lookup_int(t, si, set)
                       : h_lookup(t->h, k->u.s, set);

    }
    // TODO monitor
    case TYPE_TBL: {
        rf_str *tk = s_int2str((rf_int) k->u.t);
        rf_val *v  = h_lookup(t->h, s_int2str((rf_int) k->u.t), set);
        m_freestr(tk);
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

// VM currently initializes sequential tables backwards, inserting the
// last element first by calling t_insert_int() with `force` set to 1.
// This allows memory to be allocated once with the exact size needed
// instead of potentially resizing multiple times throughout
// initialization. Otherwise, t_insert_int() can defer to the hash
// table part of the rf_tbl if the rf_int `k` would cause the array
// part to be too sparsely populated.
rf_val *t_insert_int(rf_tbl *t, rf_int k, rf_val *v, int set, int force) {
    if (set) set(lx);
    double lf = potential_lf(t->an, t->cap, k);
    if (force || (lf >= MIN_LOAD_FACTOR)) {
        if (t->cap <= k) {
            set(lx);
            int oc = t->cap;
            int nc = force ? k + 1 : new_size(t->an, t->cap, k);
            // TODO the hackiest hack that ever hacked - maybe keep
            // track of integer keys in the hash part instead
            nc += (!force * h_length(t->h)) + 1;
            t->v = realloc(t->v, sizeof(rf_val *) * nc);

            // Collect valid integer keys from the hash part and move
            // them to the newly allocated array part
            for (int i = oc; i < nc; ++i) {
                if (exists(t,i)) continue;
                rf_str *ik = s_int2str(i);
                t->v[i] = h_delete(t->h, ik);
                if (t->v[i]) t->an++;
                m_freestr(ik);
            }
            t->cap = nc;
        }
        if (!exists(t, k)) {
            rf_val *nv = malloc(sizeof(rf_val));
            *nv     = *v;
            t->v[k] = nv;
            t->an++;
            if (set && !is_null(v))
                t->n++;
        } else {
            *t->v[k] = *v;
        }
        return t->v[k];
    }
    else {
        rf_str *ik = s_int2str(k);
        rf_val *rv = h_lookup(t->h, ik, set); // TODO?
        m_freestr(ik);
        return rv;
    }
}

rf_val *t_insert(rf_tbl *t, rf_val *k, rf_val *v, int set) {
    if (set) set(lx);
    switch (k->type) {
    case TYPE_NULL:
        *t->nullv = *v;
        if (!is_null(v)) t->n++;
        return t->nullv;
    case TYPE_INT: return t_insert_int(t, k->u.i, v, set, 0);
    case TYPE_FLT:
        if (k->u.f == (rf_int) k->u.f)
            return t_insert_int(t, (rf_int) k->u.f, v, set, 0);
        else {
            rf_str *fk = s_flt2str(k->u.f);
            rf_val *rv = h_insert(t->h, fk, v, set);
            m_freestr(fk);
            return rv;
        }
    case TYPE_STR: return h_insert(t->h, k->u.s, v, set);

    // TODO monitor
    case TYPE_TBL: {
        rf_str *tk = s_int2str((rf_int) k->u.t);
        rf_val *rv = h_insert(t->h, tk, v, set);
        m_freestr(tk);
        return rv;
    }
    case TYPE_RFN: // TODO
    default: break;
    }
    return NULL;
}
