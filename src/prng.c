#include "prng.h"

// Pseudo-random number generation

// xoshiro256**
// Source: https://prng.di.unimi.it

#define rol(x,n) (((x)<<(n)) | ((x)>>(-(int)(n)&(8*sizeof(x)-1))))

rf_uint prng_next(prng_state *s) {
    const rf_uint res = rol(s->u[1] * 5, 7) * 9;
    const rf_uint t = s->u[1] << 17;
    s->u[2] ^= s->u[0];
    s->u[3] ^= s->u[1];
    s->u[1] ^= s->u[2];
    s->u[0] ^= s->u[3];
    s->u[2] ^= t;
    s->u[3]  = rol(s->u[3], 45);
    return res;
}

// I believe this is how you would utilize splitmix64 to initialize
// the PRNG state
void prng_seed(prng_state *s, rf_uint seed) {
    s->u[0] = seed + 0x9e3779b97f4a7c15u;
    s->u[1] = (s->u[0] ^ (s->u[0] >> 30)) * 0xbf58476d1ce4e5b9u;
    s->u[2] = (s->u[1] ^ (s->u[1] >> 27)) * 0x94d049bb133111ebu;
    s->u[3] =  s->u[2] ^ (s->u[2] >> 31);
    for (int i = 0; i < 16; ++i)
        prng_next(s);
}

