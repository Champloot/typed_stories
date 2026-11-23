// rng: seeded randomness, so a date can give everyone the same day
#include "rng.h"

uint64_t rng_mix(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

uint64_t rng_hash(const char *s) {
    uint64_t h = 1469598103934665603ULL;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 1099511628211ULL;
    }
    return h;
}

Rng rng_for(uint64_t seed, unsigned what, unsigned id) {
    Rng r = { rng_mix(seed ^ rng_mix(((uint64_t)what << 32) | id)) };
    return r;
}

unsigned rng_next(Rng *r) {
    r->s = rng_mix(r->s);
    return (unsigned)(r->s >> 32);
}

int rng_below(Rng *r, int n) {
    return n > 0 ? (int)(rng_next(r) % (unsigned)n) : 0;
}

void rng_shuffle(Rng *r, int *a, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rng_below(r, i + 1), t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
}
