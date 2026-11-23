// match: turning what the player typed into one of the choices
#ifndef MATCH_H
#define MATCH_H

#include "common.h"

typedef struct {
    int         choice;     // -1 if nothing matched
    int         tier;
    int         typo;       // matched only with a small typo
    const char *phrase;     // the phrase it matched (for repetition tracking)
} Match;

Match match_input(const Variant *v, const char *in);

#endif
