// day: building a day: phases, unlocked scenes, variants
#ifndef DAY_H
#define DAY_H

#include "common.h"

typedef struct {
    const Scenario *s;
    const Variant  *v;
    const char     *phase;  // NULL if the pack has no phases
} Event;

const Variant *pick_variant(const Scenario *s, uint64_t flags);
int order_by_slots(const Pack *p, int *order);
int build_phase(const Pack *p, int ph, uint64_t flags, int *out);
int planned_after(const Pack *p, int ph_from, uint64_t flags);
void mark_range(const Variant *v, int *lo, int *hi);

#endif
