// ui: what the game shows, whatever shows it. Two implementations:
//   ui_tty.c  the terminal (ncurses): for development and the tests
//   ui_gl.c   the window (raylib): the game people play
// The game hands over plain data ("views") and asks for keys; it never draws itself.
#ifndef UI_H
#define UI_H

#include "common.h"

/* Keys as the game sees them. Printable ASCII (32..126) comes as itself. */
enum {
    K_NONE      = -1,   // nothing pressed before the timeout
    K_OTHER     = 1,    // a key the game has no use for: still counts as "any key"
    K_BACKSPACE = 8,
    K_ENTER     = '\n',
    K_CTRL_U    = 21,   // erase the line
    K_CTRL_W    = 23,   // erase a word
};

/* The strip on top of every in-game screen. */
typedef struct {
    int         nmeters;
    const char *meter_names[MAX_METERS];
    int         meters[MAX_METERS];
    const char *word;           // word of the day, NULL if there is none
    int         word_used;
} Hud;

/* A situation: where in the day, what happens, and the cue. */
typedef struct {
    int         idx, total;     // idx counts from 0
    const char *phase;          // NULL in packs without phases
    const char *title;
    const char *detail;         // the cue, NULL if none
} Scene;

typedef struct {
    const char *name;
    int         best, today;    // scores, -1 if there is no record
} MenuPack;

typedef struct {
    int         npacks;
    MenuPack    packs[MAX_PACKS];
    int         today_mode;
    const char *date;           // "Tue 24 Nov 2026"
    const char *word;           // NULL if there is none
} MenuView;

typedef struct {
    const char *pack;
    int         today_mode;
    const char *date;
    const char *word;           // NULL if there is none
    int         has_meters;
    int         best, today;    // -1 if there is no record
} DayStartView;

typedef struct {
    Scene       scene;
    int         nhints;
    const char *hints[MAX_CHOICES];
    const char *input;          // what the player has typed so far
    const char *msg;            // "Huh? ..." after a miss, "" otherwise
    double      left, limit;    // seconds
} PromptView;

/* Meter changes of an outcome (a random timeout can have two sets). */
typedef struct {
    int         n;
    const char *name[2 * MAX_EFFECTS];
    int         delta[2 * MAX_EFFECTS];
} EffectsView;

typedef struct {
    Scene       scene;
    const char *typed;          // the accepted answer; NULL when time ran out
    const char *text;           // the reaction, or the timeout text
    const char *fate;           // a random timeout: what happened anyway; NULL otherwise
    EffectsView fx;
    // style of the answer (when typed != NULL)
    char        rank;
    int         points;
    int         typo, repeated; // matched with a typo; the phrase was used recently
    int         word_bonus;     // the word of the day earned +WORD_BONUS here
    int         word_again;     // the word was used, but the bonus was taken already
    double      seconds;
} OutcomeView;

typedef struct {
    const char *meter;
    const char *text;
} EndingView;

typedef struct {
    const char *pack;
    int         ended_early;
    int         rating;         // 0..10
    const char *verdict;
    char        style_rank;
    int         style_avg;
    const char *word;           // NULL if there is none
    int         word_used;
    Hud         meters;         // meters at the end (word fields unused)
    double      accuracy, wpm;
    int         timeouts, misses;
    int         score, prev_best, new_best;     // prev_best -1 if this is the first
    int         today_mode, today_best, new_day;
} SummaryView;

void ui_init(void);
void ui_end(void);
void ui_set_hud(const Hud *h);  // NULL hides it
void ui_menu(const MenuView *v);
void ui_day_start(const DayStartView *v);
void ui_intro(const Scene *s);
void ui_prompt(const PromptView *v);
void ui_outcome(const OutcomeView *v);
void ui_ending(const EndingView *v);
void ui_summary(const SummaryView *v);
int  ui_key(int timeout_ms);    // waits for a key (forever if timeout_ms < 0); K_NONE on timeout
void ui_flush(void);            // forget keys pressed ahead

#endif
