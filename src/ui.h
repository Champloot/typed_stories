// ui: what the game shows, whatever shows it. Two implementations:
//   ui_tty.c  the terminal (ncurses): for development and the tests
//   ui_gl.c   the window (raylib): the game people play
// The game hands over plain data ("views") and asks for keys; it never draws itself.
#ifndef UI_H
#define UI_H

#include "common.h"
#include "minigame.h"

/* Keys as the game sees them. Printable ASCII (32..126) comes as itself. */
enum {
    K_NONE      = -1,   // nothing pressed before the timeout
    K_OTHER     = 1,    // a key the game has no use for: still counts as "any key"
    K_BACKSPACE = 8,
    K_ENTER     = '\n',
    K_CTRL_U    = 21,   // erase the line
    K_CTRL_W    = 23,   // erase a word
    K_UP = 2, K_DOWN = 3, K_LEFT = 4, K_RIGHT = 5,  // arrows (walking)
};

/* Held directions (ui_held), for walking. */
enum { DIR_UP = 1, DIR_DOWN = 2, DIR_LEFT = 4, DIR_RIGHT = 8 };

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
    const char *stage;          // the picture ('stage:'), NULL if none
    const char *props;          // what is in it: "dog=growl stranger=near", NULL if nothing
    const char *show;           // after the answer: how the picture reacts, NULL if it does not
} Scene;

typedef struct {
    const char *name;
    const char *style;          // the pack's look, so the menu can hint at it
    int         best, today;    // scores, -1 if there is no record
    int         locked;         // the level before it is not passed yet
    int         done;           // passed: lived through to a finale at least once
    int         nmedals;
    const char *medal[MAX_MEDALS];
    int         got[MAX_MEDALS];
} MenuPack;

typedef struct {
    int         npacks;
    MenuPack    packs[MAX_PACKS];
    int         today_mode;
    const char *date;           // "Tue 24 Nov 2026"
    const char *word;           // NULL if there is none
    int         all_done;       // every level passed: the game is won
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

/* An activity: the player walks a top-down map to the spot the answer named.
   Cells are as in activity.h; the window draws them, the terminal prints them. */
typedef struct {
    Scene        scene;
    const char  *answer;        // what was typed
    const char  *goal_name;     // "aisle 4", for the line on top
    char         goal;          // the goal's cell character
    int          w, h;
    const char *const *rows;
    int          px, py, ppx, ppy;
    double       moved_at;      // when the player last stepped (to slide between cells)
    int          ncarts;
    int          cart_x[4], cart_y[4], cart_dx[4], cart_px[4];
    double       cart_moved[4];
    int          bumped;        // a cart just hit the player
    double       left, limit;
} WalkView;

/* A minigame after the answer (see minigame.h). */
typedef struct {
    Scene        scene;
    int          kind, state;   // MG_*
    const char  *text;          // the caption, or the line to sing
    int          n;
    double       progress;      // 0..1
    double       needle, zone_lo, zone_hi;      // timing
    int          tries;
    int          typed;         // lyrics: letters typed right
    double       missed_ago, wrong_ago, ended_ago, moved_ago;   // seconds since
    int          w, h, px, py, ppx, ppy;        // cross and catch
    char         cells[MG_H][MG_W + 1];         // '.' free, 'b' bike, 'c' car, 'o' apple
    int          lane_dir[MG_H];                // cross: -1, +1, 0 for a sidewalk
    double       lane_step[MG_H], lane_next[MG_H];
    int          caught, missed;
    double       left, limit;
} MinigameView;

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
    const char *title;
    const char *text;
    int         special;        // a flag ending (finale_if), not one picked by rating
    int         good;           // 0 bad, 1 middling, 2 good: by where the rating fell
} FinaleView;

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
    int         nmedals;        // the pack's medals: won now, won before, or not
    const char *medal[MAX_MEDALS];
    int         medal_now[MAX_MEDALS];
    int         medal_before[MAX_MEDALS];
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
void ui_finale(const FinaleView *v);    // how the story ends, when the day was lived through
void ui_summary(const SummaryView *v);
void ui_set_theme(const char *style);   // a pack's 'style:'; NULL is the menu's look
void ui_walk(const WalkView *v);
void ui_minigame(const MinigameView *v);
int  ui_presses(void);          // real key presses since the last call (held keys don't repeat); -1 if every key event is one
int  ui_held(void);             // DIR_* held right now; -1 if this screen can't tell (then each key press is a step)
int  ui_key(int timeout_ms);    // waits for a key (forever if timeout_ms < 0); K_NONE on timeout
void ui_flush(void);            // forget keys pressed ahead

#endif
