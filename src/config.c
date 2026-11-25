#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "util.h"

Config cfg = {
    .answer_scale = 1.0, .walk_scale = 1.0, .game_scale = 1.0,
    .scenes_per_phase = 0, .minigames = 1, .walks = 1, .meter_start = 0,
    .tier1 = 10, .tier2 = 30, .tier3 = 60, .typo_factor = 0.5, .heat_decay = 0.5,
    .word_bonus = 100, .wordsmith_days = 5,
    .mash_presses = 1.0, .mash_leak = 1.0,
    .timing_sweep = 1.0, .timing_zone = 0.2,
    .cross_speed = 1.0, .cross_gap = 4,
    .catch_apples = 7, .catch_fall = 0.2, .catch_every = 0.75,
    .grid_step = 0.12,
    .cart_step = 0.30, .carts = 3,
    .width = 1280, .height = 800, .fullscreen = 0, .fade = 0.18,
};

typedef struct {
    const char *section, *key;
    char        type;           // 'i' int, 'd' double
    void       *ptr;
    double      lo, hi;
} Key;

static const Key keys[] = {
    { "time",   "answer_scale",     'd', &cfg.answer_scale,     0.2, 10 },
    { "time",   "walk_scale",       'd', &cfg.walk_scale,       0.2, 10 },
    { "time",   "minigame_scale",   'd', &cfg.game_scale,       0.2, 10 },
    { "day",    "scenes_per_phase", 'i', &cfg.scenes_per_phase, 0, 12 },
    { "day",    "minigames",        'i', &cfg.minigames,        0, 1 },
    { "day",    "store_walk",       'i', &cfg.walks,            0, 1 },
    { "day",    "meter_start",      'i', &cfg.meter_start,      0, 10 },
    { "style",  "plain",            'i', &cfg.tier1,            0, 1000 },
    { "style",  "nicer",            'i', &cfg.tier2,            0, 1000 },
    { "style",  "fancy",            'i', &cfg.tier3,            0, 1000 },
    { "style",  "typo_factor",      'd', &cfg.typo_factor,      0, 1 },
    { "style",  "wear_left",        'd', &cfg.heat_decay,       0, 1 },
    { "style",  "word_bonus",       'i', &cfg.word_bonus,       0, 10000 },
    { "style",  "wordsmith_days",   'i', &cfg.wordsmith_days,   1, 365 },
    { "mash",   "presses",          'd', &cfg.mash_presses,     0.1, 10 },
    { "mash",   "leak",             'd', &cfg.mash_leak,        0, 20 },
    { "timing", "sweep",            'd', &cfg.timing_sweep,     0.2, 10 },
    { "timing", "zone",             'd', &cfg.timing_zone,      0.02, 1 },
    { "cross",  "speed",            'd', &cfg.cross_speed,      0.1, 10 },
    { "cross",  "gap",              'i', &cfg.cross_gap,        1, 10 },
    { "catch",  "apples",           'i', &cfg.catch_apples,     1, 12 },
    { "catch",  "fall",             'd', &cfg.catch_fall,       0.03, 2 },
    { "catch",  "every",            'd', &cfg.catch_every,      0.1, 5 },
    { "grid",   "step",             'd', &cfg.grid_step,        0.02, 1 },
    { "store",  "cart_step",        'd', &cfg.cart_step,        0.05, 5 },
    { "store",  "carts",            'i', &cfg.carts,            0, 3 },
    { "window", "width",            'i', &cfg.width,            640, 7680 },
    { "window", "height",           'i', &cfg.height,           400, 4320 },
    { "window", "fullscreen",       'i', &cfg.fullscreen,       0, 1 },
    { "window", "fade",             'd', &cfg.fade,             0, 2 },
};
#define NKEYS ((int)(sizeof keys / sizeof *keys))

static void read_file(const char *path, FILE *f) {
    char line[LINE_BUF], section[32] = "";
    int lineno = 0;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        char *hash = strchr(line, '#');         // '#' starts a comment anywhere on a line
        if (hash) *hash = '\0';
        char *s = trim(line);
        if (!*s || *s == ';') continue;
        if (*s == '[') {
            char *end = strchr(s, ']');
            if (!end || end[1]) die_at(path, lineno, "a section is written as [name]");
            *end = '\0';
            int ok = 0;
            for (int i = 0; i < NKEYS; i++) ok |= !strcmp(keys[i].section, s + 1);
            if (!ok) die_at(path, lineno, "no section [%s] (see the comments in config.ini)", s + 1);
            snprintf(section, sizeof section, "%s", s + 1);
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq) die_at(path, lineno, "expected 'name = value'");
        *eq = '\0';
        char *key = trim(s), *val = trim(eq + 1);
        if (!*section) die_at(path, lineno, "'%s' must be inside a [section]", key);
        const Key *k = NULL;
        for (int i = 0; i < NKEYS && !k; i++)
            if (!strcmp(keys[i].section, section) && !strcmp(keys[i].key, key)) k = &keys[i];
        if (!k) die_at(path, lineno, "no setting '%s' in [%s]", key, section);
        char *end = NULL;
        double v = strtod(val, &end);
        if (!*val || *end) die_at(path, lineno, "'%s' must be a number, got '%s'", key, val);
        if (k->type == 'i' && v != (int)v) die_at(path, lineno, "'%s' must be a whole number", key);
        if (v < k->lo || v > k->hi)
            die_at(path, lineno, "'%s' must be from %g to %g, got %g", key, k->lo, k->hi, v);
        if (k->type == 'i') *(int *)k->ptr = (int)v;
        else *(double *)k->ptr = v;
    }
}

void config_load(const char *exe_path) {
    const char *env = getenv("TYPED_STORIES_CONFIG");
    if (env && !strcmp(env, "-")) return;
    char path[1024];
    FILE *f = NULL;
    if (env && *env) {
        snprintf(path, sizeof path, "%s", env);
        f = fopen(path, "r");
        if (!f) die_at(path, 0, "can't open this config file");
    }
    if (!f) {
        snprintf(path, sizeof path, "config.ini");
        f = fopen(path, "r");
    }
    if (!f && exe_path) {           // next to the program
        snprintf(path, sizeof path, "%s", exe_path);
        char *slash = strrchr(path, '/'), *bslash = strrchr(path, '\\');
        if (bslash > slash) slash = bslash;
        if (slash) {
            snprintf(slash + 1, sizeof path - (size_t)(slash + 1 - path), "config.ini");
            f = fopen(path, "r");
        }
    }
    if (!f) return;                 // no file: the defaults
    read_file(path, f);
    fclose(f);
}
