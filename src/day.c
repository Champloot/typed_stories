// day: building a day: phases, unlocked scenes, variants
#include "day.h"
#include "pack.h"
#include "util.h"

// Variants whose 'if' holds win over plain ones, so a consequence shows up
// whenever its flags are set. NULL if no variant fits right now.
const Variant *pick_variant(const Scenario *s, uint64_t flags, Rng *rng) {
    int want_cond = 0, total = 0;
    for (int i = 0; i < s->nvariants; i++)
        if (s->variants[i].has_cond && cond_holds(&s->variants[i].cond, flags)) want_cond = 1;
    for (int pass = 0; pass < 2; pass++) {
        int r = pass ? rng_below(rng, total) : 0;
        for (int i = 0; i < s->nvariants; i++) {
            const Variant *v = &s->variants[i];
            int ok = want_cond ? v->has_cond && cond_holds(&v->cond, flags) : !v->has_cond;
            if (!ok) continue;
            if (!pass) { total += v->weight; continue; }
            if (r < v->weight) return v;
            r -= v->weight;
        }
        if (!total) return NULL;
    }
    return NULL;
}

static int scenario_open(const Scenario *s, uint64_t flags) {
    if (s->has_needs && !cond_holds(&s->needs, flags)) return 0;
    for (int i = 0; i < s->nvariants; i++)
        if (!s->variants[i].has_cond || cond_holds(&s->variants[i].cond, flags)) return 1;
    return 0;
}

// Packs without phases: pinned scenarios go to the ends, the rest is shuffled in between.
int order_by_slots(const Pack *p, int *order, uint64_t seed) {
    int n = 0, first = -1, last = -1;
    for (int i = 0; i < p->count; i++) {
        if (p->scen[i].slot == SLOT_FIRST)     first = i;
        else if (p->scen[i].slot == SLOT_LAST) last = i;
        else order[n++] = i;
    }
    Rng rng = rng_for(seed, RNG_SLOTS, 0);
    rng_shuffle(&rng, order, n);
    if (first >= 0) {
        memmove(order + 1, order, (size_t)n * sizeof *order);
        order[0] = first;
        n++;
    }
    if (last >= 0) order[n++] = last;
    return n;
}

// One phase, built when the day reaches it (so earlier choices count):
// 'pick' random regular scenarios plus every unlocked scene ('needs:') that
// is open now, shuffled together.
int build_phase(const Pack *p, int ph, uint64_t flags, int *out, uint64_t seed) {
    int *pool = xrealloc(NULL, (size_t)p->count * sizeof *pool);
    int m = 0, n = 0;
    for (int i = 0; i < p->count; i++) {
        const Scenario *s = &p->scen[i];
        if (s->phase == ph && !s->has_needs && scenario_open(s, flags)) pool[m++] = i;
    }
    Rng rng = rng_for(seed, RNG_PHASE, (unsigned)ph);
    rng_shuffle(&rng, pool, m);
    int pick = p->phases[ph].pick, take = pick && pick < m ? pick : m;
    for (int k = 0; k < take; k++) out[n++] = pool[k];
    for (int i = 0; i < p->count; i++) {
        const Scenario *s = &p->scen[i];
        if (s->phase == ph && s->has_needs && scenario_open(s, flags)) out[n++] = i;
    }
    free(pool);
    rng_shuffle(&rng, out, n);
    return n;
}

// For the [3/11] header: phases not built yet, as they would be with today's flags.
// The number grows when a choice unlocks a new scene.
int planned_after(const Pack *p, int ph_from, uint64_t flags) {
    int t = 0;
    for (int ph = ph_from; ph < p->nphases; ph++) {
        int m = 0, u = 0;
        for (int i = 0; i < p->count; i++) {
            const Scenario *s = &p->scen[i];
            if (s->phase != ph || !scenario_open(s, flags)) continue;
            if (s->has_needs) u++; else m++;
        }
        int pick = p->phases[ph].pick;
        t += (pick && pick < m ? pick : m) + u;
    }
    return t;
}

// The best and worst mark this variant can give, so the rating can be scaled.
void mark_range(const Variant *v, int *lo, int *hi) {
    *lo = *hi = v->choices[0].mark;
    for (int c = 1; c < v->nchoices; c++) {
        if (v->choices[c].mark < *lo) *lo = v->choices[c].mark;
        if (v->choices[c].mark > *hi) *hi = v->choices[c].mark;
    }
    if (v->on_timeout == TO_TEXT) {
        if (v->timeout_mark < *lo) *lo = v->timeout_mark;
        if (v->timeout_mark > *hi) *hi = v->timeout_mark;
    }
    for (int c = 0; c < v->nchoices; c++)        // a lost minigame is an outcome too
        if (v->choices[c].has_fail) {
            if (v->choices[c].fail_mark < *lo) *lo = v->choices[c].fail_mark;
            if (v->choices[c].fail_mark > *hi) *hi = v->choices[c].fail_mark;
        }
}

