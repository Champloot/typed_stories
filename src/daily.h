// daily: today's date and the word of the day (words.lst)
#ifndef DAILY_H
#define DAILY_H

#include "common.h"

typedef struct {
    char iso[16];       // 2026-11-24: seeds Today mode, keys the records
    char pretty[40];    // Tue 24 Nov 2026
} Date;

// Local date. TYPED_STORIES_DATE=YYYY-MM-DD overrides it (for tests and time travel).
Date today(void);
// words.lst next to the packs, optional. Call after the packs are loaded:
// a word must not be part of any answer, or removing it would break matching.
void load_words(const char *dir);
const char *word_of_day(const Date *d);     // NULL if there are no words

#endif
