/*
 * prng.h — Mulberry32, the shared deterministic random source for ortho.
 *
 * This is the exact algorithm used by the JavaScript reference implementation
 * (src/prng.js in the `ortho` repo). Same seed MUST produce the same uint32
 * stream in both, because that is what makes a seed name the same invented
 * language on every host.
 *
 * No host types, no allocation, no globals. See SPEC.md §2.
 */

#ifndef ORTHO_PRNG_H
#define ORTHO_PRNG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t state;
} ortho_prng;

/* Initialize with a seed. Seed 0 is legal and deterministic. */
void ortho_prng_init(ortho_prng *r, uint32_t seed);

/* Next raw value in [0, 2^32). */
uint32_t ortho_prng_next_u32(ortho_prng *r);

/* Next float in [0, 1). Matches JS: nextU32() / 4294967296.0 */
double ortho_prng_next(ortho_prng *r);

/* Next integer in [0, n). Matches JS: floor(next() * n). */
int ortho_prng_below(ortho_prng *r, int n);

#ifdef __cplusplus
}
#endif

#endif /* ORTHO_PRNG_H */
