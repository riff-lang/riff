#include "prng.h"
#include "util.h"

// Pseudo-random number generation

// xoshiro256**
// Source: https://prng.di.unimi.it

riff_uint riff_prng_next(riff_prng_state *s) {
    const riff_uint res = rol(s->u[1] * 5, 7) * 9;
    const riff_uint t = s->u[1] << 17;
    s->u[2] ^= s->u[0];
    s->u[3] ^= s->u[1];
    s->u[1] ^= s->u[2];
    s->u[0] ^= s->u[3];
    s->u[2] ^= t;
    s->u[3]  = rol(s->u[3], 45);
    return res;
}

// I believe this is how you would utilize splitmix64 to initialize the PRNG
// state
void riff_prng_seed(riff_prng_state *s, riff_uint seed) {
    s->u[0] = seed + 0x9e3779b97f4a7c15u;
    s->u[1] = (s->u[0] ^ (s->u[0] >> 30)) * 0xbf58476d1ce4e5b9u;
    s->u[2] = (s->u[1] ^ (s->u[1] >> 27)) * 0x94d049bb133111ebu;
    s->u[3] =  s->u[2] ^ (s->u[2] >> 31);
    for (int i = 0; i < 16; ++i) {
        riff_prng_next(s);
    }
}

