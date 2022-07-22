#ifndef PRNG_H
#define PRNG_H

#include "types.h"

typedef struct {
    riff_uint u[4];
} riff_prng_state;

riff_uint riff_prng_next(riff_prng_state *);
void      riff_prng_seed(riff_prng_state *, riff_uint);

#endif
