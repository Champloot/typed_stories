// common.h: limits and the data types shared by all modules.
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHOICES   4
#define MAX_VARIANTS  8
#define MAX_PHASES    12
#define MAX_VERDICTS  8
#define MAX_PACKS     9     // the menu picks a pack with one digit key
#define MAX_TOKENS    8     // intents/phrases joined with '+' in one choice
#define MAX_SPLIT     64    // phrases on one lexicon line
#define INPUT_MAX     60    // characters the player can type
#define LINE_BUF      1024
#define MARK_LIMIT    10    // marks are integers in -10..10
#define TIMEOUT_MARK  (-1)  // default mark for hesitating
#define MAX_FLAGS     64    // different flags per pack (one bit each)
#define MAX_SETS      4     // flags one outcome can set
#define MAX_CLAUSES   4     // conditions joined with ','
#define MAX_LITS      4     // flags joined with 'or' in one condition
#define MAX_METERS    4     // Reigns-like meters per pack
#define METER_MAX     10    // meters live in 0..METER_MAX; 0 ends the day
#define METER_START   5     // default start value
#define MAX_EFFECTS   4     // meters one outcome can change
#define WORD_BONUS    100   // style for the first answer with the word of the day
#define MAX_FINALES     8       // story endings by rating, worst -> best
#define MAX_FINALE_IF   8       // special endings chosen by flags
#define WORDSMITH_DAYS 5    // days with the word of the day that unlock the 'wordsmith' flag

/* Flags are what the day remembers: an outcome sets them ('sets:'), and later
   variants ('if:') and whole scenes ('needs:') check them. */

/* A condition: every clause (joined with ',') must hold; inside a clause one of
   the flags (joined with ' or ') is enough. '!flag' means the flag is not set. */
typedef struct {
    int nclauses;
    struct { int n; int flag[MAX_LITS]; int neg[MAX_LITS]; } c[MAX_CLAUSES];
} Cond;

typedef struct {
    int n;
    int flag[MAX_SETS];
} FlagSet;

/* Meter changes of one outcome ('effects: nerves -1, money +2'). */
typedef struct {
    int n;
    int meter[MAX_EFFECTS];
    int delta[MAX_EFFECTS];
} Effects;

/* A phrase the player may type. Text is normalized (see normalize()). */
typedef struct {
    char *text;
    int   tier;     // 1 plain, 2 good, 3 fancy: sets the style points
} Phrase;

/* One meaning with many ways to say it, from lexicon.lex. */
typedef struct {
    char   *name;
    char   *hint;   // what the answer means ("agree"), shown instead of exact words
    Phrase *ph;
    int     n, cap;
} Intent;

/* What happens when the timer runs out. */
enum {
    TO_TEXT,    // a fixed outcome, described by the timeout text
    TO_RANDOM   // the timeout text, then a random choice happens anyway
};

/* Where a scenario sits in a run (packs without phases only). */
enum { SLOT_ANY, SLOT_FIRST, SLOT_LAST };

typedef struct {
    char   *label;  // hint shown on screen: the intent's hint, or the choice's own
    char   *result; // what happens
    int     mark;   // how good it is for the run: +1 good, 0 neutral, -1 bad, ...
    Phrase *ph;     // every phrase that selects this choice
    int     nph, cap;
    FlagSet sets;   // what the day remembers if this happens
    Effects fx;     // how the meters move if this happens
} Choice;

/* One hidden version of a scenario. The player sees its detail (the cue)
   and has to work out which answer is right this time. */
typedef struct {
    char   *detail;         // cue shown under the title, NULL if none
    int     weight;         // relative chance to be picked
    int     on_timeout;     // TO_*
    char   *timeout_text;
    int     timeout_mark;
    FlagSet timeout_sets;
    Effects timeout_fx;
    int     nchoices;
    Choice  choices[MAX_CHOICES];
    int     has_cond;       // 'if:' given: only possible when cond holds, and then preferred
    Cond    cond;
    int     last_outcome;   // while parsing: what 'sets:' refers to (-1 none, -2 timeout, else a choice)
    int     src_line;
} Variant;

typedef struct {
    char    *title;
    double   time_limit;    // seconds
    int      slot;          // SLOT_*
    int      phase;         // index into Pack.phases, -1 if none
    int      has_needs;     // 'needs:' given: an unlocked scene, added to its phase when needs holds
    Cond     needs;
    int      nvariants;
    Variant  variants[MAX_VARIANTS];
    int      src_line;
} Scenario;

typedef struct {
    char *name;
    int   pick;             // scenarios of this phase per run, 0 = all
} Phase;

typedef struct {
    char     *name;
    char     *verdicts[MAX_VERDICTS];   // worst -> best
    int       nverdicts;
    Phase     phases[MAX_PHASES];
    int       nphases;
    Scenario *scen;
    int       count, cap;
    char     *flags[MAX_FLAGS];
    int       nflags;
    int       flag_is_set[MAX_FLAGS];   // some outcome sets it
    int       flag_checked[MAX_FLAGS];  // line of the first 'if'/'needs' that checks it, 0 if none
    char     *meter_names[MAX_METERS];  // as written in 'meters:', e.g. "Nerves"
    int       meter_start[MAX_METERS];
    char     *meter_endings[MAX_METERS];// what happens when the meter hits 0
    int       nmeters;
    char     *style;                    // look of the window: "abstract", "ornate", "terminal"
    char     *finale_title[MAX_FINALES];// the story's ending when the day is lived through, by rating
    char     *finale_text[MAX_FINALES];
    int       nfinales;
    Cond      finale_if[MAX_FINALE_IF]; // special endings, the first whose flags hold wins
    char     *finale_if_title[MAX_FINALE_IF];
    char     *finale_if_text[MAX_FINALE_IF];
    int       nfinale_if;
} Pack;

#endif
