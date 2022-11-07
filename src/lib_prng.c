#include "lib.h"

#include "prng.h"

#include <time.h>

// Dedicated user PRNG state
static riff_prng_state prngs;

// PRNG functions

// rand([x])
//   rand()       | random float ∈ [0..1)
//   rand(0)      | random int ∈ [INT64_MIN..INT64_MAX]
//   rand(n)      | random int ∈ [0..n]
//   rand(m,n)    | random int ∈ [m..n]
//   rand(range)  | random int ∈ range
LIB_FN(rand) {
    riff_uint rand = riff_prng_next(&prngs);
    if (!argc) {
        riff_float f = (riff_float) ((rand >> 11) * (0.5 / ((riff_uint)1 << 52)));
        set_flt(fp-1, f);
    }

    // If first argument is a range, ignore any succeeding args
    else if (is_range(fp)) {
        riff_int from = fp->q->from;
        riff_int to   = fp->q->to;
        riff_int itvl = fp->q->itvl;
        riff_uint range, offset;
        if (from < to) {
            //           <<<
            // [i64min..from] ∪ [to..i64max]
            if (itvl < 0) {
                range  = UINT64_MAX - (to - from) + 1;
                itvl   = llabs(itvl);
                offset = to + (range % itvl);
            } else {
                range  = to - from;
                offset = from;
            }
        }

        //                 >>>
        // [i64min..to] ∪ [from..i64max]
        else if (from > to) {
            if (itvl > 0) {
                range  = UINT64_MAX - (from - to) + 1;
                offset = from;
            } else {
                range  = from - to;
                itvl   = llabs(itvl);
                offset = to + (range % itvl);
            }
        } else {
            set_int(fp-1, from);
            return 1;
        }
        range /= itvl;
        range += !(range == UINT64_MAX);
        rand  %= range;
        rand  *= itvl;
        rand  += offset;
        set_int(fp-1, rand);
    }

    // 1 argument (0..n)
    else if (argc == 1) {
        riff_int n1 = intval(fp);
        if (n1 == 0) {
            set_int(fp-1, rand);
        } else {
            if (n1 > 0) {
                set_int(fp-1,   rand %  (n1 + 1));
            } else {
                set_int(fp-1, -(rand % -(n1 - 1)));
            }
        }
    }

    // 2 arguments (m..n)
    else {
        riff_int n1 = intval(fp);
        riff_int n2 = intval(fp+1);
        riff_uint range, offset;
        if (n1 < n2) {
            range  = n2 - n1;
            offset = n1;
        } else if (n1 > n2) {
            range  = n1 - n2;
            offset = n2;
        } else {
            set_int(fp-1, n1);
            return 1;
        }
        range += !(range == UINT64_MAX);
        set_int(fp-1, (rand % range) + offset);
    }
    return 1;
}

// srand([x])
// Initializes the PRNG with seed `x` or time(0) if no argument given.
// rand() will produce the same sequence when srand is initialized
// with a given seed every time.
LIB_FN(srand) {
    riff_int seed = 0;
    if (!argc) {
        seed = time(0);
    } else if (!is_null(fp)) {
        // Seed the PRNG with whatever 64 bits are in the riff_val union
        seed = fp->i;
    }
    riff_prng_seed(&prngs, seed);
    set_int(fp-1, seed);
    return 1;
}

static riff_lib_fn_reg prnglib[] = {
    LIB_FN_REG(rand,  0),
    LIB_FN_REG(srand, 0),
};

void riff_lib_register_prng(riff_htab *g) {
    // Initialize the PRNG with the current time
    riff_prng_seed(&prngs, time(0));
    FOREACH(prnglib, i) {
        riff_htab_insert_cstr(g, prnglib[i].name, &(riff_val) {TYPE_CFN, .cfn = &prnglib[i].fn});
    }
}
