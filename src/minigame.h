// minigame: short games a choice can start after its answer is typed
// ('act: mash 2.5 14 hold the doors!' under a choice in a pack). Winning gives
// the choice's outcome; losing gives its 'fail:' outcome (or the timeout one).
// The rules live here, so the window, the terminal and the tests share them.
//
//   mash    press keys n times before the time is up (doors, blankets, leashes)
//   timing  a needle swings; press Space while it is in the green zone, n tries
//   cross   cross the road from the bottom to the top, dodging bikes and cars
//   catch   move the basket left and right, catch n of the falling apples
//   lyrics  type the line (the text) before the time is up
#ifndef MINIGAME_H
#define MINIGAME_H

#include "config.h"
#include "rng.h"

enum { MG_MASH, MG_TIMING, MG_CROSS, MG_CATCH, MG_LYRICS, MG_KINDS };
enum { MG_GOING, MG_WON, MG_LOST };

#define MG_W        15          // grid of cross and catch
#define MG_H        9
#define MG_STEP_S   (cfg.grid_step) // seconds per cell while a direction is held (config.ini)
#define MG_APPLES   12          // at most this many apples fall in 'catch' (config.ini says how many)
#define MG_TEXT     64

typedef struct {
    int    dir;                 // -1 to the left, +1 to the right, 0 for a sidewalk
    double step, next;          // seconds per cell, when it moves next
    char   cells[MG_W];         // '.' free, 'b' a bike, 'c' a car
} Lane;

typedef struct {
    int    x, y;                // y < 0: not dropped yet
    int    state;               // 0 falling, 1 caught, 2 missed
    double drop_at;
} Apple;

typedef struct {
    int    kind, state, n;
    double start, limit, ended_at;
    char   text[MG_TEXT];       // a caption, or the line to type for 'lyrics'
    // mash
    double power;               // presses, slowly leaking
    double last_t;
    // timing
    double zone_lo, zone_hi, period;
    int    tries;               // presses left
    double missed_at;
    // grid games
    int    w, h, px, py, ppx, ppy;
    double moved_at, next_step;
    Lane   lanes[MG_H];
    Apple  apples[MG_APPLES];
    int    caught, missed, napples;
    double fall_s;
    // lyrics
    int    typed;
    double wrong_at;
} Minigame;

int         mg_kind(const char *name);                  // -1 if unknown
const char *mg_name(int kind);
void        mg_start(Minigame *g, int kind, double limit, int n, const char *text, Rng *r, double t);
void        mg_tick(Minigame *g, double t);             // time passes: things move, time runs out
void        mg_press(Minigame *g, int ch, double t);    // a key press: ' ' is the action; letters type in 'lyrics'
void        mg_move(Minigame *g, int dx, int dy, double t);
double      mg_needle(const Minigame *g, double t);     // timing: where the needle is, 0..1
double      mg_progress(const Minigame *g, double t);   // 0..1, how close to winning (pictures use it)
char        mg_cell(const Minigame *g, int x, int y);   // cross and catch: what is in a cell, '.' if nothing

#endif
