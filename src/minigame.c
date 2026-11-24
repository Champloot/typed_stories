#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "minigame.h"

static const char *const names[MG_KINDS] = { "mash", "timing", "cross", "catch", "lyrics" };

int mg_kind(const char *name) {
    for (int i = 0; i < MG_KINDS; i++)
        if (!strcmp(names[i], name)) return i;
    return -1;
}

const char *mg_name(int kind) { return names[kind]; }

static void end(Minigame *g, int state, double t) {
    if (g->state != MG_GOING) return;
    g->state = state;
    g->ended_at = t;
}

// A lane of traffic: cars (two cells) and bikes, with room to slip between.
static void fill_lane(Lane *l, Rng *r) {
    memset(l->cells, '.', sizeof l->cells);
    int x = rng_below(r, 3);
    while (x < MG_W - 1) {
        int car = rng_below(r, 2);
        l->cells[x] = car ? 'c' : 'b';
        if (car) l->cells[x + 1] = 'c';
        x += (car ? 2 : 1) + 4 + rng_below(r, 4);
    }
}

void mg_start(Minigame *g, int kind, double limit, int n, const char *text, Rng *r, double t) {
    memset(g, 0, sizeof *g);
    g->kind = kind;
    g->limit = limit;
    g->start = g->last_t = t;
    g->state = MG_GOING;
    g->wrong_at = g->missed_at = g->moved_at = -10;
    snprintf(g->text, sizeof g->text, "%s", text ? text : "");
    switch (kind) {
    case MG_MASH:
        g->n = n > 0 ? n : 12;
        break;
    case MG_TIMING: {
        g->n = g->tries = n > 0 ? n : 2;
        g->period = 2.0;           // one second per sweep
        double c = 0.3 + 0.4 * rng_below(r, 100) / 100.0;
        g->zone_lo = c - 0.1;
        g->zone_hi = c + 0.1;
        break;
    }
    case MG_CROSS:
        g->w = MG_W;
        g->h = 7;
        g->px = g->ppx = MG_W / 2;
        g->py = g->ppy = g->h - 1;
        for (int y = 1; y < g->h - 1; y++) {
            Lane *l = &g->lanes[y];
            l->dir = rng_below(r, 2) ? 1 : -1;
            l->step = 0.3 + 0.05 * rng_below(r, 4);
            l->next = t + l->step;
            fill_lane(l, r);
            l->cells[g->px] = '.';          // never start with something on the first step
        }
        break;
    case MG_CATCH:
        g->n = n > 0 ? n : 4;
        g->w = 9;
        g->h = 8;
        g->px = g->ppx = 4;
        g->py = g->ppy = g->h - 1;
        g->fall_s = 0.2;
        for (int i = 0; i < MG_APPLES; i++) {
            g->apples[i].x = rng_below(r, g->w);
            g->apples[i].y = -1;
            g->apples[i].drop_at = t + 0.5 + i * 0.75;
        }
        break;
    default:
        break;
    }
}

void mg_tick(Minigame *g, double t) {
    if (g->state != MG_GOING) return;
    if (g->kind == MG_MASH) {           // the doors push back
        g->power -= 1.0 * (t - g->last_t);
        if (g->power < 0) g->power = 0;
        g->last_t = t;
    }
    if (g->kind == MG_CROSS)
        for (int y = 1; y < g->h - 1; y++) {
            Lane *l = &g->lanes[y];
            while (t >= l->next) {
                l->next += l->step;
                char tmp[MG_W];
                for (int x = 0; x < MG_W; x++) tmp[(x + l->dir + MG_W) % MG_W] = l->cells[x];
                memcpy(l->cells, tmp, MG_W);
                if (g->py == y && l->cells[g->px] != '.') end(g, MG_LOST, t);
            }
        }
    if (g->kind == MG_CATCH)
        for (int i = 0; i < MG_APPLES; i++) {
            Apple *a = &g->apples[i];
            while (a->state == 0 && t >= a->drop_at) {
                a->drop_at += g->fall_s;
                a->y++;
                if (a->y == g->h - 1 && a->x == g->px) { a->state = 1; g->caught++; }
                else if (a->y >= g->h) { a->state = 2; g->missed++; }
            }
            if (g->caught >= g->n) end(g, MG_WON, t);
            if (g->missed > MG_APPLES - g->n) end(g, MG_LOST, t);
        }
    if (t - g->start >= g->limit) end(g, MG_LOST, t);
}

void mg_press(Minigame *g, int ch, double t) {
    if (g->state != MG_GOING) return;
    switch (g->kind) {
    case MG_MASH:
        g->power += 1;
        if (g->power >= g->n) end(g, MG_WON, t);
        break;
    case MG_TIMING:
        if (ch != ' ' && ch != '\n') break;
        {
            double x = mg_needle(g, t);
            if (x >= g->zone_lo && x <= g->zone_hi) { end(g, MG_WON, t); break; }
            g->missed_at = t;
            if (--g->tries <= 0) end(g, MG_LOST, t);
        }
        break;
    case MG_LYRICS: {
        char want = g->text[g->typed];
        if (!want) break;
        if (tolower(ch) == tolower((unsigned char)want)) {
            g->typed++;
            if (!g->text[g->typed]) end(g, MG_WON, t);
        } else {
            g->wrong_at = t;
        }
        break;
    }
    default:
        break;
    }
}

void mg_move(Minigame *g, int dx, int dy, double t) {
    if (g->state != MG_GOING) return;
    if (g->kind == MG_CATCH) dy = 0;
    if (g->kind != MG_CROSS && g->kind != MG_CATCH) return;
    int nx = g->px + dx, ny = g->py + dy;
    if (nx < 0 || nx >= g->w || ny < 0 || ny >= g->h || (!dx && !dy)) return;
    g->ppx = g->px;
    g->ppy = g->py;
    g->px = nx;
    g->py = ny;
    g->moved_at = t;
    g->next_step = t + MG_STEP_S;
    if (g->kind == MG_CROSS) {
        if (g->lanes[ny].dir && g->lanes[ny].cells[nx] != '.') end(g, MG_LOST, t);
        else if (ny == 0) end(g, MG_WON, t);
    }
}

double mg_needle(const Minigame *g, double t) {
    if (g->state != MG_GOING) t = g->ended_at;
    double ph = fmod((t - g->start) / g->period, 1.0);
    return ph < 0.5 ? ph * 2 : 2 - ph * 2;
}

double mg_progress(const Minigame *g, double t) {
    (void)t;
    if (g->state == MG_WON) return 1;
    switch (g->kind) {
    case MG_MASH:   return g->power / g->n;
    case MG_CROSS:  return (double)(g->h - 1 - g->py) / (g->h - 1);
    case MG_CATCH:  return (double)g->caught / g->n;
    case MG_LYRICS: return g->text[0] ? (double)g->typed / strlen(g->text) : 0;
    default:        return 0;
    }
}

char mg_cell(const Minigame *g, int x, int y) {
    if (g->kind == MG_CROSS && y > 0 && y < g->h - 1) return g->lanes[y].cells[x];
    if (g->kind == MG_CATCH)
        for (int i = 0; i < MG_APPLES; i++)
            if (g->apples[i].state == 0 && g->apples[i].x == x && g->apples[i].y == y) return 'o';
    return '.';
}
