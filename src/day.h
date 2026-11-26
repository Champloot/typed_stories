// day: building a day: phases, unlocked scenes, variants
#ifndef DAY_H
#define DAY_H

#include "common.h"
#include "rng.h"

typedef struct {
    const Scenario *s;
    const Variant  *v;
    const char     *phase;  // NULL if the pack has no phases
    Rng             fate;   // for 'on_timeout: random'
} Event;

const Variant *pick_variant(const Scenario *s, uint64_t flags, Rng *rng);
int order_by_slots(const Pack *p, int *order, uint64_t seed);
int build_phase(const Pack *p, int ph, uint64_t flags, int *out, uint64_t seed);
int planned_after(const Pack *p, int ph_from, uint64_t flags);
int scenario_open(const Scenario *s, uint64_t flags);   // its needs hold and some variant can be played
void mark_range(const Variant *v, int *lo, int *hi);

#endif
