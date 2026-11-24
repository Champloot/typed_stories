// pack: pack files: scenarios, variants, choices, flags, conditions and meters
#ifndef PACK_H
#define PACK_H

#include "common.h"

/* Flags the game sets by itself; packs can check them with 'if:'/'needs:'. */
#define FLAG_WORDSMITH "wordsmith"
#define FLAG_TODAY     "today"

extern Pack *packs;
extern int npacks;
extern const char *const pack_styles[];   // allowed values of 'style:', NULL-terminated; the first is the default

int cond_holds(const Cond *c, uint64_t flags);
void apply_sets(uint64_t *flags, const FlagSet *fs);
int pack_flag(const Pack *pk, const char *name);    // index of a flag, -1 if the pack never uses it
void load_packs(const char *dir);

#endif
