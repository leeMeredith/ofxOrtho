/*
 * prng.c — Mulberry32.
 *
 * Mirrors src/prng.js exactly. The JS emulates uint32 overflow with `>>> 0`
 * and Math.imul; C gets it natively from uint32_t arithmetic. Keep the two in
 * lockstep — any change here changes every language on every host.
 */

#include "prng.h"

void ortho_prng_init(ortho_prng *r, uint32_t seed)
{
    r->state = seed;
}

uint32_t ortho_prng_next_u32(ortho_prng *r)
{
    uint32_t t;

    /* JS: this.state = (this.state + 0x6d2b79f5) >>> 0; */
    r->state += 0x6D2B79F5u;
    t = r->state;

    /* JS: t = Math.imul(t ^ (t >>> 15), t | 1); */
    t = (t ^ (t >> 15)) * (t | 1u);

    /* JS: t ^= t + Math.imul(t ^ (t >>> 7), t | 61); */
    t ^= t + (t ^ (t >> 7)) * (t | 61u);

    /* JS: return (t ^ (t >>> 14)) >>> 0; */
    return t ^ (t >> 14);
}

double ortho_prng_next(ortho_prng *r)
{
    return (double)ortho_prng_next_u32(r) / 4294967296.0;
}

int ortho_prng_below(ortho_prng *r, int n)
{
    if (n <= 0) {
        return 0;
    }
    return (int)(ortho_prng_next(r) * (double)n);
}
