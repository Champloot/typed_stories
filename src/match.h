// match: turning what the player typed into one of the choices
#ifndef MATCH_H
#define MATCH_H

#include "common.h"

typedef struct {
    int         choice;     // -1 if nothing matched
    int         tier;
    int         typo;       // matched only with a small typo
    int         loose;      // the phrase was found inside a longer sentence
    int         word;       // the answer contained the word of the day
    int         only_word;  // ...and nothing else
    const char *phrase;     // the phrase it matched (for repetition tracking)
} Match;

Match match_input(const Variant *v, const char *in);
// match_input, after taking the word of the day out of the (normalized) answer.
Match match_answer(const Variant *v, const char *in, const char *word);

#endif
