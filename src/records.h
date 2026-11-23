// records: best results per pack and per date, and the days the word of the day was used
#ifndef RECORDS_H
#define RECORDS_H

#include "common.h"

typedef struct {
    int score;      // rating * 100 + style + meters * 10
    int rating;     // 0..10
    int style;      // average style per answer
} Result;

void records_load(void);
void records_save(void);
const Result *record_best(const char *pack);                    // any mode, NULL if none
const Result *record_day(const char *date, const char *pack);   // Today mode on that date
// Keeps the result if it beats the stored ones; says which records were beaten.
void record_result(const char *date, const char *pack, Result r, int daily, int *new_best, int *new_day);
void record_word_day(const char *date);
int word_days(void);        // on how many different days the word of the day was used

#endif
