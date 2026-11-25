#include <string.h>

#include "config.h"
#include "activity.h"

/* ---------- maps ---------- */

// The store from above: nine shelves make eight aisles, each with its goods
// in the middle (the digits). Checkouts at the bottom, the entrance bottom left.
static const char *const store_rows[] = {
    "###############################",
    "#.............................#",
    "#.............................#",
    "#..#..#..#..#..#..#..#..#..#..#",
    "#..#..#..#..#..#..#..#..#..#..#",
    "#..#..#..#..#..#..#..#..#..#..#",
    "#..#11#22#33#44#55#66#77#88#..#",
    "#..#11#22#33#44#55#66#77#88#..#",
    "#..#..#..#..#..#..#..#..#..#..#",
    "#..#..#..#..#..#..#..#..#..#..#",
    "#..#..#..#..#..#..#..#..#..#..#",
    "#.............................#",
    "#.............................#",
    "#....i...........KK.KK.KK.....#",
    "#...............kkkkkkkkkk....#",
    "###############################",
};

typedef struct { const char *name; char cell; } Spot;

static const Spot store_spots[] = {
    { "aisle1", '1' }, { "aisle2", '2' }, { "aisle3", '3' }, { "aisle4", '4' },
    { "aisle5", '5' }, { "aisle6", '6' }, { "aisle7", '7' }, { "aisle8", '8' },
    { "checkout", 'k' }, { NULL, 0 },
};

typedef struct {
    const char        *name;
    const char *const *rows;
    int                w, h;
    const Spot        *spots;
    const char        *spot_list;
    int                start_x, start_y;
    struct { int y, dx; } carts[WALK_MAX_CARTS];    // cart lanes: a row and a direction
    int                ncarts;
} Activity;

static const Activity acts[] = {
    { "store", store_rows, 31, 16, store_spots, "aisle1 .. aisle8, checkout", 2, 14,
      { { 11, 1 }, { 12, -1 }, { 1, -1 } }, 3 },
};
#define NACTS ((int)(sizeof acts / sizeof *acts))

int activity_find(const char *name) {
    for (int i = 0; i < NACTS; i++)
        if (!strcmp(acts[i].name, name)) return i;
    return -1;
}

const char *activity_name(int act) { return acts[act].name; }

const char *activity_spots(int act) { return acts[act].spot_list; }

char activity_spot(int act, const char *spot) {
    for (const Spot *s = acts[act].spots; s->name; s++)
        if (!strcmp(s->name, spot)) return s->cell;
    return 0;
}

/* ---------- walking ---------- */

char walk_cell(const Walk *w, int x, int y) {
    if (x < 0 || y < 0 || x >= w->w || y >= w->h) return '#';
    return w->rows[y][x];
}

static int blocked(char c) { return c == '#' || c == 'K' || c == 'i'; }

int walk_cart_at(const Walk *w, int x, int y) {
    for (int i = 0; i < w->ncarts; i++)
        if (w->carts[i].x == x && w->carts[i].y == y) return i;
    return -1;
}

void walk_start(Walk *w, int act, char goal, Rng *r, double t) {
    const Activity *a = &acts[act];
    memset(w, 0, sizeof *w);
    w->act = act;
    w->rows = a->rows;
    w->w = a->w;
    w->h = a->h;
    w->px = w->ppx = a->start_x;
    w->py = w->ppy = a->start_y;
    w->moved_at = t - 1;
    w->goal = goal;
    w->bumped_at = -10;
    for (int i = 0; i < a->ncarts && i < cfg.carts; i++) {
        Cart *c = &w->carts[w->ncarts];
        c->y = a->carts[i].y;
        c->dx = a->carts[i].dx;
        c->dy = 0;
        // somewhere along its lane, never on the player
        do c->x = 1 + (int)rng_below(r, a->w - 2);
        while (blocked(walk_cell(w, c->x, c->y)) || (c->x == w->px && c->y == w->py));
        c->px = c->x;
        c->moved_at = t - 1;
        c->next = t + CART_STEP_S * (1 + rng_below(r, 3));
        w->ncarts++;
    }
}

// Carts roll along their lane and turn around at walls. One that would roll
// into the player stops and bumps them instead.
void walk_tick(Walk *w, double t) {
    for (int i = 0; i < w->ncarts; i++) {
        Cart *c = &w->carts[i];
        while (t >= c->next) {
            c->next += CART_STEP_S;
            int nx = c->x + c->dx;
            if (blocked(walk_cell(w, nx, c->y)) || walk_cart_at(w, nx, c->y) >= 0) {
                c->dx = -c->dx;
                continue;
            }
            if (nx == w->px && c->y == w->py) {
                if (t - w->bumped_at > 1.0) { w->bumps++; w->bumped_at = t; }
                c->next += CART_STEP_S * 2;      // waits a moment
                continue;
            }
            c->px = c->x;
            c->x = nx;
            c->moved_at = c->next - CART_STEP_S;
        }
    }
}

int walk_step(Walk *w, int dx, int dy, double t) {
    int nx = w->px + dx, ny = w->py + dy;
    if (blocked(walk_cell(w, nx, ny))) return 0;
    if (walk_cart_at(w, nx, ny) >= 0) {
        if (t - w->bumped_at > 1.0) { w->bumps++; w->bumped_at = t; }
        return -1;
    }
    w->ppx = w->px;
    w->ppy = w->py;
    w->px = nx;
    w->py = ny;
    w->moved_at = t;
    w->next_step = t + WALK_STEP_S;
    return 1;
}

int walk_arrived(const Walk *w) {
    return walk_cell(w, w->px, w->py) == w->goal;
}
