// pack: pack files: scenarios, variants, choices, flags and conditions
#ifndef PACK_H
#define PACK_H

#include "common.h"

extern Pack *packs;
extern int npacks;

int cond_holds(const Cond *c, uint64_t flags);
void apply_sets(uint64_t *flags, const FlagSet *fs);
void load_packs(const char *dir);

#endif
