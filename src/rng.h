// rng: seeded randomness, so a date can give everyone the same day
#ifndef RNG_H
#define RNG_H

#include "common.h"

typedef struct { uint64_t s; } Rng;

/* Every random decision draws from its own stream (what + id), so the same
   seed gives the same morning even if two players chose differently since. */
enum { RNG_PHASE = 1, RNG_SLOTS, RNG_VARIANT, RNG_FATE };

uint64_t rng_mix(uint64_t x);           // splitmix64 finalizer
uint64_t rng_hash(const char *s);       // FNV-1a
Rng rng_for(uint64_t seed, unsigned what, unsigned id);
unsigned rng_next(Rng *r);
int rng_below(Rng *r, int n);           // 0..n-1
void rng_shuffle(Rng *r, int *a, int n);

#endif
