// activity: scenes where you do more than type. After the answer is typed, the
// player walks to the spot it names on a top-down map ('play: store 10' in a
// scenario, 'goal: aisle4' under each choice), with carts in the way.
// The rules live here so the window, the terminal and the tests share them.
#ifndef ACTIVITY_H
#define ACTIVITY_H

#include "config.h"
#include "rng.h"

#define WALK_MAX_CARTS 4
#define WALK_STEP_S    (cfg.grid_step)  // seconds per cell while a direction is held (config.ini)
#define CART_STEP_S    (cfg.cart_step)  // seconds per cell for a cart

/* Map cells:
     '#'  wall or shelf          'K'  checkout counter      'i'  info stand
     '.'  floor                  '1'..'8', 'k'  floor that belongs to a spot
   Walls, shelves, counters and the stand block the way. */
typedef struct {
    int    x, y, dx, dy;
    int    px;                  // where it was before its last move (for smooth drawing)
    double next;                // when it moves next
    double moved_at;
} Cart;

typedef struct {
    int         act;            // which map
    const char *const *rows;
    int         w, h;
    int         px, py;         // the player
    int         ppx, ppy;       // where the player was before the last step (for smooth drawing)
    double      moved_at;
    double      next_step;      // held keys: no step before this
    char        goal;           // the spot to reach (its cell character)
    Cart        carts[WALK_MAX_CARTS];
    int         ncarts;
    int         bumps;          // walked into a cart, or a cart into you
    double      bumped_at;
} Walk;

int         activity_find(const char *name);                // -1 if there is no such activity
const char *activity_name(int act);
const char *activity_spots(int act);                        // "aisle1 ... checkout", for error messages
char        activity_spot(int act, const char *spot);       // the spot's cell character, 0 if none

void walk_start(Walk *w, int act, char goal, Rng *r, double t);
void walk_tick(Walk *w, double t);                          // carts move
int  walk_step(Walk *w, int dx, int dy, double t);          // 1 moved, 0 blocked, -1 bumped a cart
int  walk_arrived(const Walk *w);
char walk_cell(const Walk *w, int x, int y);                // the map without the player and carts
int  walk_cart_at(const Walk *w, int x, int y);

#endif
