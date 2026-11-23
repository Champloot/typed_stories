// Typing game: packs (text files) -> scenarios with hidden variants -> choices typed under a timer.
// Every outcome carries a mark; the marks of a run add up to its rating.
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <ncurses.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_CHOICES   4
#define MAX_VARIANTS  8
#define MAX_PHASES    12
#define MAX_VERDICTS  8
#define MAX_PACKS     9     // the menu picks a pack with one digit key
#define LINE_BUF      1024
#define MARK_LIMIT    10    // marks are integers in -10..10
#define TIMEOUT_MARK  (-1)  // default mark for hesitating

/* ---------- data ---------- */

/* What happens when the timer runs out. */
enum {
    TO_TEXT,    // a fixed outcome, described by the timeout text
    TO_RANDOM   // the timeout text, then a random choice happens anyway
};

/* Where a scenario sits in a run (packs without phases only). */
enum { SLOT_ANY, SLOT_FIRST, SLOT_LAST };

typedef struct {
    char *word;     // what to type
    char *result;   // what happens
    int   mark;     // how good it is for the run: +1 good, 0 neutral, -1 bad, ...
} Choice;

/* One hidden version of a scenario. The player sees its detail (the cue)
   and has to work out which choice is right this time. */
typedef struct {
    char   *detail;         // cue shown under the title, NULL if none
    int     weight;         // relative chance to be picked
    int     on_timeout;     // TO_*
    char   *timeout_text;
    int     timeout_mark;
    int     nchoices;
    Choice  choices[MAX_CHOICES];
    int     src_line;
} Variant;

typedef struct {
    char    *title;
    double   time_limit;    // seconds
    int      slot;          // SLOT_*
    int      phase;         // index into Pack.phases, -1 if none
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
} Pack;

static Pack *packs;
static int   npacks;

/* ---------- utils ---------- */

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void shuffle(int *a, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1), t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) { fputs("out of memory\n", stderr); exit(1); }
    return q;
}

static char *xstrdup(const char *s) {
    char *d = strdup(s);
    if (!d) { fputs("out of memory\n", stderr); exit(1); }
    return d;
}

/* ---------- pack loading ---------- */

__attribute__((format(printf, 3, 4), noreturn))
static void die_at(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "%s:%d: ", file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

// Trims spaces/tabs/newlines on both ends, in place.
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
    return s;
}

// Splits s on '|' in place and trims each part. Returns the number of parts;
// max + 1 means there were more than max.
static int split_pipe(char *s, char **parts, int max) {
    int n = 0;
    while (n < max) {
        char *bar = strchr(s, '|');
        if (bar) *bar = '\0';
        parts[n++] = trim(s);
        if (!bar) return n;
        s = bar + 1;
    }
    return n + 1;
}

static int is_ascii_text(const char *s) {
    for (; *s; s++)
        if ((unsigned char)*s > 126 || ((unsigned char)*s < 32 && *s != '\t' && *s != '\n' && *s != '\r'))
            return 0;
    return 1;
}

static int word_ok(const char *w) {
    if (!*w) return 0;
    for (; *w; w++)
        if ((unsigned char)*w < 32 || (unsigned char)*w > 126) return 0;
    return 1;
}

static int is_prefix(const char *a, const char *b) {
    return strncmp(a, b, strlen(a)) == 0;
}

static int all_digits(const char *s) {
    if (!*s) return 0;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

static int parse_int(const char *s, int lo, int hi, int *out) {
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (end == s || *end || errno || v < lo || v > hi) return 0;
    *out = (int)v;
    return 1;
}

static Variant *new_variant(const char *file, int line, Scenario *s) {
    if (s->nvariants == MAX_VARIANTS)
        die_at(file, line, "at most %d variants per scenario", MAX_VARIANTS);
    Variant *v = &s->variants[s->nvariants++];
    memset(v, 0, sizeof *v);
    v->weight = 1;
    v->on_timeout = TO_TEXT;
    v->timeout_mark = TIMEOUT_MARK;
    v->src_line = line;
    return v;
}

// 'choice:' and friends before any 'variant:' line belong to an implicit
// variant with no detail, so simple scenarios need no 'variant:' at all.
static Variant *cur_variant(const char *file, int line, Scenario *s) {
    if (s->nvariants == 0) return new_variant(file, line, s);
    return &s->variants[s->nvariants - 1];
}

// A choice that is a prefix of another one in the same variant could never be
// told apart while typing, so reject it.
static void check_scenario(const char *file, const Scenario *s) {
    if (s->time_limit <= 0) die_at(file, s->src_line, "scenario '%s': missing 'time'", s->title);
    if (s->nvariants == 0)  die_at(file, s->src_line, "scenario '%s': needs at least one 'choice'", s->title);
    for (int k = 0; k < s->nvariants; k++) {
        const Variant *v = &s->variants[k];
        if (!v->timeout_text) die_at(file, v->src_line, "scenario '%s': missing 'timeout'", s->title);
        if (v->nchoices < 1)  die_at(file, v->src_line, "scenario '%s': needs at least one 'choice'", s->title);
        for (int i = 0; i < v->nchoices; i++)
            for (int j = i + 1; j < v->nchoices; j++) {
                const char *a = v->choices[i].word, *b = v->choices[j].word;
                if (is_prefix(a, b) || is_prefix(b, a))
                    die_at(file, v->src_line,
                           "scenario '%s': \"%s\" and \"%s\" overlap (one is the start of the other)",
                           s->title, a, b);
            }
    }
}

static void load_pack_file(const char *dir, const char *fname, Pack *pk) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, fname);
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open '%s': %s\n", path, strerror(errno)); exit(1); }

    char buf[LINE_BUF];
    int lineno = 0;
    while (fgets(buf, sizeof buf, f)) {
        lineno++;
        if (!strchr(buf, '\n') && !feof(f))
            die_at(path, lineno, "line too long (max %d characters)", LINE_BUF - 2);

        char *line = trim(buf);
        if (*line == '\0' || *line == '#') continue;
        if (!is_ascii_text(line))
            die_at(path, lineno, "only plain ASCII text is supported for now (no Cyrillic, no fancy quotes or dashes)");
        char *colon = strchr(line, ':');
        if (!colon) die_at(path, lineno, "expected 'key: value'");
        *colon = '\0';
        char *key = trim(line), *val = trim(colon + 1);
        Scenario *cur = pk->count ? &pk->scen[pk->count - 1] : NULL;

        if (!strcmp(key, "pack")) {
            if (!*val) die_at(path, lineno, "empty pack name");
            pk->name = xstrdup(val);
        } else if (!strcmp(key, "verdicts")) {
            if (pk->nverdicts) die_at(path, lineno, "second 'verdicts' line");
            char *parts[MAX_VERDICTS + 1];
            int n = split_pipe(val, parts, MAX_VERDICTS);
            if (n < 2 || n > MAX_VERDICTS)
                die_at(path, lineno, "'verdicts' needs 2 to %d entries separated by '|', worst first", MAX_VERDICTS);
            for (int i = 0; i < n; i++) {
                if (!*parts[i]) die_at(path, lineno, "empty verdict");
                pk->verdicts[i] = xstrdup(parts[i]);
            }
            pk->nverdicts = n;
        } else if (!strcmp(key, "phases")) {
            if (pk->nphases) die_at(path, lineno, "second 'phases' line");
            if (pk->count)   die_at(path, lineno, "'phases' must come before the first scenario");
            char *parts[MAX_PHASES + 1];
            int n = split_pipe(val, parts, MAX_PHASES);
            if (n > MAX_PHASES) die_at(path, lineno, "at most %d phases", MAX_PHASES);
            for (int i = 0; i < n; i++) {
                char *name = parts[i];
                int pick = 0;
                char *sp = strrchr(name, ' ');
                if (sp && all_digits(sp + 1)) {
                    if (!parse_int(sp + 1, 1, 100, &pick))
                        die_at(path, lineno, "phase count must be from 1 to 100");
                    *sp = '\0';
                    name = trim(name);
                }
                if (!*name) die_at(path, lineno, "empty phase name");
                for (int j = 0; j < i; j++)
                    if (!strcmp(pk->phases[j].name, name))
                        die_at(path, lineno, "phase '%s' is listed twice", name);
                pk->phases[i].name = xstrdup(name);
                pk->phases[i].pick = pick;
            }
            pk->nphases = n;
        } else if (!strcmp(key, "scenario")) {
            if (cur) check_scenario(path, cur);
            if (!*val) die_at(path, lineno, "empty scenario title");
            if (pk->count == pk->cap) {
                pk->cap = pk->cap ? pk->cap * 2 : 8;
                pk->scen = xrealloc(pk->scen, pk->cap * sizeof *pk->scen);
            }
            Scenario *s = &pk->scen[pk->count++];
            memset(s, 0, sizeof *s);
            s->title = xstrdup(val);
            s->slot = SLOT_ANY;
            s->phase = -1;
            s->src_line = lineno;
        } else if (!cur) {
            die_at(path, lineno, "'%s' before the first 'scenario:'", key);
        } else if (!strcmp(key, "time")) {
            char *end;
            double t = strtod(val, &end);
            if (end == val || *end || t <= 0 || t > 120)
                die_at(path, lineno, "'time' must be a number of seconds, more than 0 and up to 120");
            cur->time_limit = t;
        } else if (!strcmp(key, "slot")) {
            if (!strcmp(val, "first"))      cur->slot = SLOT_FIRST;
            else if (!strcmp(val, "last"))  cur->slot = SLOT_LAST;
            else if (!strcmp(val, "any"))   cur->slot = SLOT_ANY;
            else die_at(path, lineno, "'slot' must be 'first', 'last' or 'any'");
        } else if (!strcmp(key, "phase")) {
            if (!pk->nphases) die_at(path, lineno, "'phase' used, but the pack has no 'phases:' line");
            int found = -1;
            for (int i = 0; i < pk->nphases; i++)
                if (!strcmp(pk->phases[i].name, val)) found = i;
            if (found < 0) die_at(path, lineno, "phase '%s' is not listed in 'phases:'", val);
            cur->phase = found;
        } else if (!strcmp(key, "variant")) {
            Variant *v = new_variant(path, lineno, cur);
            v->detail = *val ? xstrdup(val) : NULL;
        } else if (!strcmp(key, "weight")) {
            Variant *v = cur_variant(path, lineno, cur);
            if (!parse_int(val, 1, 100, &v->weight))
                die_at(path, lineno, "'weight' must be an integer from 1 to 100");
        } else if (!strcmp(key, "on_timeout")) {
            Variant *v = cur_variant(path, lineno, cur);
            if (!strcmp(val, "text"))        v->on_timeout = TO_TEXT;
            else if (!strcmp(val, "random")) v->on_timeout = TO_RANDOM;
            else die_at(path, lineno, "'on_timeout' must be 'text' or 'random'");
        } else if (!strcmp(key, "timeout")) {
            Variant *v = cur_variant(path, lineno, cur);
            if (v->timeout_text) die_at(path, lineno, "second 'timeout' in the same variant");
            char *parts[3];
            int n = split_pipe(val, parts, 2);
            if (n > 2) die_at(path, lineno, "'timeout' is: what happens | mark (mark is optional)");
            if (!*parts[0]) die_at(path, lineno, "empty timeout text");
            if (n == 2 && !parse_int(parts[1], -MARK_LIMIT, MARK_LIMIT, &v->timeout_mark))
                die_at(path, lineno, "mark must be an integer from %d to %d", -MARK_LIMIT, MARK_LIMIT);
            v->timeout_text = xstrdup(parts[0]);
        } else if (!strcmp(key, "choice")) {
            Variant *v = cur_variant(path, lineno, cur);
            if (v->nchoices == MAX_CHOICES)
                die_at(path, lineno, "at most %d choices per variant", MAX_CHOICES);
            char *parts[4];
            int n = split_pipe(val, parts, 3);
            if (n < 2 || n > 3)
                die_at(path, lineno, "'choice' is: word | what happens | mark (mark is optional)");
            if (!word_ok(parts[0]))
                die_at(path, lineno, "choice word must be non-empty printable ASCII");
            if (!*parts[1]) die_at(path, lineno, "empty choice result");
            int mark = 0;
            if (n == 3 && !parse_int(parts[2], -MARK_LIMIT, MARK_LIMIT, &mark))
                die_at(path, lineno, "mark must be an integer from %d to %d", -MARK_LIMIT, MARK_LIMIT);
            Choice *c = &v->choices[v->nchoices++];
            c->word = xstrdup(parts[0]);
            c->result = xstrdup(parts[1]);
            c->mark = mark;
        } else {
            die_at(path, lineno, "unknown key '%s'", key);
        }
    }
    fclose(f);

    if (!pk->name)      die_at(path, lineno, "missing 'pack:' name");
    if (!pk->nverdicts) die_at(path, lineno, "missing 'verdicts:' line");
    if (pk->count == 0) die_at(path, lineno, "no scenarios");
    check_scenario(path, &pk->scen[pk->count - 1]);

    int firsts = 0, lasts = 0;
    for (int i = 0; i < pk->count; i++) {
        firsts += pk->scen[i].slot == SLOT_FIRST;
        lasts  += pk->scen[i].slot == SLOT_LAST;
    }
    if (firsts > 1 || lasts > 1)
        die_at(path, lineno, "at most one 'slot: first' and one 'slot: last' per pack");

    if (pk->nphases) {
        for (int i = 0; i < pk->count; i++) {
            const Scenario *s = &pk->scen[i];
            if (s->phase < 0)
                die_at(path, s->src_line, "scenario '%s': missing 'phase' (the pack has 'phases:')", s->title);
            if (s->slot != SLOT_ANY)
                die_at(path, s->src_line, "scenario '%s': 'slot' can't be used together with 'phases'", s->title);
        }
        for (int ph = 0; ph < pk->nphases; ph++) {
            int n = 0;
            for (int i = 0; i < pk->count; i++) n += pk->scen[i].phase == ph;
            if (n == 0) die_at(path, lineno, "phase '%s' has no scenarios", pk->phases[ph].name);
        }
    }
}

static int cmp_names(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

// Loads every *.txt in dir, in file name order (01-..., 02-... sets the menu order).
static void load_packs(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) {
        fprintf(stderr, "cannot open packs directory '%s': %s\n"
                        "run the game from the project folder, or pass the directory: ./game <dir>\n",
                dir, strerror(errno));
        exit(1);
    }
    char **names = NULL;
    int n = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t len = strlen(e->d_name);
        if (e->d_name[0] == '.' || len < 5 || strcmp(e->d_name + len - 4, ".txt") != 0) continue;
        if (n == cap) { cap = cap ? cap * 2 : 8; names = xrealloc(names, cap * sizeof *names); }
        names[n++] = xstrdup(e->d_name);
    }
    closedir(d);
    if (n == 0) { fprintf(stderr, "no pack files (*.txt) in '%s'\n", dir); exit(1); }
    if (n > MAX_PACKS) { fprintf(stderr, "too many pack files in '%s' (max %d)\n", dir, MAX_PACKS); exit(1); }

    qsort(names, n, sizeof *names, cmp_names);
    packs = xrealloc(NULL, n * sizeof *packs);
    memset(packs, 0, n * sizeof *packs);
    for (int i = 0; i < n; i++) {
        load_pack_file(dir, names[i], &packs[i]);
        npacks++;
    }
}

/* ---------- building a run ---------- */

typedef struct {
    const Scenario *s;
    const Variant  *v;
    const char     *phase;  // NULL if the pack has no phases
} Event;

static const Variant *pick_variant(const Scenario *s) {
    int total = 0;
    for (int i = 0; i < s->nvariants; i++) total += s->variants[i].weight;
    int r = rand() % total;
    for (int i = 0; i < s->nvariants; i++) {
        if (r < s->variants[i].weight) return &s->variants[i];
        r -= s->variants[i].weight;
    }
    return &s->variants[s->nvariants - 1];
}

// Packs without phases: pinned scenarios go to the ends, the rest is shuffled in between.
static int order_by_slots(const Pack *p, int *order) {
    int n = 0, first = -1, last = -1;
    for (int i = 0; i < p->count; i++) {
        if (p->scen[i].slot == SLOT_FIRST)     first = i;
        else if (p->scen[i].slot == SLOT_LAST) last = i;
        else order[n++] = i;
    }
    shuffle(order, n);
    if (first >= 0) {
        memmove(order + 1, order, n * sizeof *order);
        order[0] = first;
        n++;
    }
    if (last >= 0) order[n++] = last;
    return n;
}

// Phases go in the listed order; inside a phase, 'pick' random scenarios.
// Every scenario then gets one of its variants, by weight.
static int build_run(const Pack *p, Event *ev) {
    int *idx = xrealloc(NULL, p->count * sizeof *idx);
    int n = 0;
    if (p->nphases) {
        for (int ph = 0; ph < p->nphases; ph++) {
            int m = 0;
            for (int i = 0; i < p->count; i++)
                if (p->scen[i].phase == ph) idx[m++] = i;
            shuffle(idx, m);
            int take = p->phases[ph].pick && p->phases[ph].pick < m ? p->phases[ph].pick : m;
            for (int k = 0; k < take; k++) {
                ev[n].s = &p->scen[idx[k]];
                ev[n].phase = p->phases[ph].name;
                n++;
            }
        }
    } else {
        int m = order_by_slots(p, idx);
        for (int k = 0; k < m; k++) {
            ev[n].s = &p->scen[idx[k]];
            ev[n].phase = NULL;
            n++;
        }
    }
    free(idx);
    for (int i = 0; i < n; i++) ev[i].v = pick_variant(ev[i].s);
    return n;
}

// The best and worst mark this variant can give, so the rating can be scaled.
static void mark_range(const Variant *v, int *lo, int *hi) {
    *lo = *hi = v->choices[0].mark;
    for (int c = 1; c < v->nchoices; c++) {
        if (v->choices[c].mark < *lo) *lo = v->choices[c].mark;
        if (v->choices[c].mark > *hi) *hi = v->choices[c].mark;
    }
    if (v->on_timeout == TO_TEXT) {
        if (v->timeout_mark < *lo) *lo = v->timeout_mark;
        if (v->timeout_mark > *hi) *hi = v->timeout_mark;
    }
}

enum { C_OK = 1, C_BAD, C_DIM, C_WARN };

/* ---------- rendering ---------- */

// Prints text word-wrapped to the terminal width, starting at (row, col).
// Returns how many rows it used, so the caller can lay out what comes next.
static int print_wrapped(int row, int col, const char *text) {
    int maxw = COLS - col - 2;
    if (maxw < 20) maxw = 20;
    int x = 0, r = row;
    const char *p = text;
    while (*p) {
        while (*p == ' ') p++;
        const char *w = p;
        while (*p && *p != ' ') p++;
        int wl = (int)(p - w);
        if (wl == 0) break;
        if (x > 0 && x + 1 + wl > maxw) { r++; x = 0; }
        if (x > 0) { mvaddch(r, col + x, ' '); x++; }
        mvaddnstr(r, col + x, w, wl);
        x += wl;
    }
    return r - row + 1;
}

static void draw_header(int idx, int total, const char *phase) {
    mvprintw(1, 4, "[%d/%d]", idx + 1, total);
    if (phase) {
        printw("  ");
        attron(A_BOLD);
        for (const char *p = phase; *p; p++) addch(toupper((unsigned char)*p));
        attroff(A_BOLD);
    }
}

// Title plus the variant's detail (the cue). Returns the first row after them.
static int draw_situation(const Scenario *s, const Variant *v, int bold) {
    int r = 3;
    if (bold) attron(A_BOLD);
    r += print_wrapped(r, 4, s->title);
    if (bold) attroff(A_BOLD);
    if (v->detail) r += print_wrapped(r, 4, v->detail);
    return r;
}

static void draw_prompt(const Event *e, const int *alive, int pos, double left,
                        int flash_err, int idx, int total) {
    const Variant *v = e->v;
    erase();
    draw_header(idx, total, e->phase);
    int crow = draw_situation(e->s, v, 0) + 1;     // first row of choices

    // choices: typed part green, rest dim; choices you have moved away from fade
    for (int c = 0; c < v->nchoices; c++) {
        const char *w = v->choices[c].word;
        int len = (int)strlen(w);
        mvaddstr(crow + c, 4, alive[c] ? "> " : "  ");
        for (int i = 0; i < len; i++) {
            int pair, extra;
            if (!alive[c])                    { pair = C_DIM; extra = A_DIM;  }
            else if (i < pos)                 { pair = C_OK;  extra = A_BOLD; }
            else if (i == pos && flash_err)   { pair = C_BAD; extra = A_BOLD; }
            else                              { pair = C_DIM; extra = A_BOLD; }
            attron(COLOR_PAIR(pair) | extra);
            mvaddch(crow + c, 6 + i, (unsigned char)w[i]);
            attroff(COLOR_PAIR(pair) | extra);
        }
    }

    // time bar
    int bar_row = crow + v->nchoices + 1;
    int bar_w = COLS - 14;              // "[" + bar + "] 120.0s" must fit on one row
    if (bar_w > 30) bar_w = 30;
    if (bar_w < 5) bar_w = 5;
    double limit = e->s->time_limit;
    int filled = (int)(bar_w * (left / limit));
    if (filled < 0) filled = 0;
    int pair = left < limit * 0.3 ? C_BAD : (left < limit * 0.6 ? C_WARN : C_OK);
    attron(COLOR_PAIR(pair));
    mvprintw(bar_row, 4, "[");
    for (int i = 0; i < bar_w; i++) addch(i < filled ? '#' : ' ');
    printw("] %.1fs", left > 0 ? left : 0);
    attroff(COLOR_PAIR(pair));
    refresh();
}

static void wait_key(void) {
    int ch;
    timeout(-1);
    flushinp();             // drop keys typed ahead so the result is not skipped
    do {
        ch = getch();
    } while (ch == KEY_RESIZE);
    timeout(16);
}

static void wait_enter(void) {
    int ch;
    timeout(-1);
    flushinp();
    do {
        ch = getch();
    } while (ch != '\n' && ch != '\r' && ch != KEY_ENTER);
    timeout(16);
}

// Situation screen: the player reads the text and the cue; the choices stay
// hidden and the timer does not run until Enter is pressed.
static void show_intro(const Event *e, int idx, int total) {
    erase();
    draw_header(idx, total, e->phase);
    int r = draw_situation(e->s, e->v, 1);
    attron(COLOR_PAIR(C_DIM));
    print_wrapped(r + 1, 4, "Press Enter when ready. The timer starts right after.");
    attroff(COLOR_PAIR(C_DIM));
    refresh();
    wait_enter();
}

/* ---------- gameplay ---------- */

typedef struct {
    int    marks, best, worst;  // sum of marks, and the best/worst possible for this run
    int    keys_ok, keys_bad;
    double typing_time;
    int    timeouts;
} Stats;

// Plays one event and returns its mark. The mark is never shown: the player
// has to read it from the reaction.
static int play_event(const Event *e, int idx, int total, Stats *st) {
    const Scenario *s = e->s;
    const Variant  *v = e->v;
    int alive[MAX_CHOICES], pos = 0, typos = 0, flash_frames = 0;
    for (int c = 0; c < v->nchoices; c++) alive[c] = 1;

    show_intro(e, idx, total);
    double start = now();

    while (1) {
        double left = s->time_limit - (now() - start);
        if (left <= 0) {
            st->typing_time += s->time_limit;
            st->timeouts++;
            const Choice *fate = NULL;
            if (v->on_timeout == TO_RANDOM) fate = &v->choices[rand() % v->nchoices];

            erase();
            draw_header(idx, total, e->phase);
            int r = draw_situation(s, v, 0) + 1;
            attron(COLOR_PAIR(C_BAD));
            mvprintw(r, 4, "TIME'S UP");
            attroff(COLOR_PAIR(C_BAD));
            r += 2;
            r += print_wrapped(r, 4, v->timeout_text);
            if (fate) r += print_wrapped(r, 4, fate->result);
            mvprintw(r + 1, 4, "(press any key)");
            refresh();
            wait_key();
            return fate ? fate->mark : v->timeout_mark;
        }

        draw_prompt(e, alive, pos, left, flash_frames > 0, idx, total);
        if (flash_frames > 0) flash_frames--;

        int ch = getch();
        if (ch == ERR || ch == KEY_RESIZE) continue;

        // does any still-possible choice continue with this key?
        int match = 0;
        for (int c = 0; c < v->nchoices; c++)
            if (alive[c] && v->choices[c].word[pos] == ch) match = 1;
        if (!match) {
            typos++;
            st->keys_bad++;
            flash_frames = 6;   // brief red mark on the expected letter
            continue;
        }
        st->keys_ok++;

        for (int c = 0; c < v->nchoices; c++)
            if (alive[c] && v->choices[c].word[pos] != ch) alive[c] = 0;
        pos++;

        int chosen = -1;
        for (int c = 0; c < v->nchoices; c++)
            if (alive[c] && v->choices[c].word[pos] == '\0') chosen = c;
        if (chosen < 0) continue;

        const Choice *c = &v->choices[chosen];
        double elapsed = now() - start;
        st->typing_time += elapsed;

        erase();
        draw_header(idx, total, e->phase);
        int r = draw_situation(s, v, 0) + 1;
        attron(COLOR_PAIR(C_OK));
        mvprintw(r, 4, "%s", c->word);
        attroff(COLOR_PAIR(C_OK));
        r += 2;
        r += print_wrapped(r, 4, c->result);
        attron(COLOR_PAIR(C_DIM));
        mvprintw(r + 1, 4, "%.1fs, %d typos", elapsed, typos);
        attroff(COLOR_PAIR(C_DIM));
        mvprintw(r + 3, 4, "(press any key)");
        refresh();
        wait_key();
        return c->mark;
    }
}

// Rating scales the run's marks between the worst and the best possible run,
// in integer math so the boundaries are exact.
static void show_summary(const Pack *p, const Stats *st) {
    int span = st->best - st->worst, got = st->marks - st->worst;
    int rating = span > 0 ? (got * 20 + span) / (2 * span) : 10;
    int vi = span > 0 ? got * p->nverdicts / span : p->nverdicts - 1;
    if (vi >= p->nverdicts) vi = p->nverdicts - 1;
    if (vi < 0) vi = 0;
    int keys = st->keys_ok + st->keys_bad;
    double acc = keys ? 100.0 * st->keys_ok / keys : 100.0;
    double wpm = st->typing_time > 0 ? (st->keys_ok / 5.0) / (st->typing_time / 60.0) : 0;

    erase();
    char head[256];
    snprintf(head, sizeof head, "RUN COMPLETE: %s", p->name);
    int r = 2 + print_wrapped(2, 4, head);
    attron(A_BOLD);
    mvprintw(r + 1, 4, "Rating: %d/10", rating);
    attroff(A_BOLD);
    r += 3;
    r += print_wrapped(r, 4, p->verdicts[vi]);
    r += 1;
    mvprintw(r, 4, "Typing:   %.0f%% accuracy, %.0f wpm", acc, wpm);
    mvprintw(r + 1, 4, "Timeouts: %d", st->timeouts);
    mvprintw(r + 3, 4, "(press any key)");
    refresh();
    wait_key();
}

static int choose_pack(void) {
    timeout(-1);
    while (1) {
        erase();
        int r = 2 + print_wrapped(2, 4, "TYPING GAME - choose who you are today") + 1;
        for (int i = 0; i < npacks; i++)
            mvprintw(r + i, 4, "%d) %s", i + 1, packs[i].name);
        mvprintw(r + npacks + 1, 4, "q) quit");
        refresh();
        int ch = getch();
        if (ch == 'q') { timeout(16); return -1; }
        if (ch >= '1' && ch < '1' + npacks) { timeout(16); return ch - '1'; }
    }
}

static void run_pack(const Pack *p) {
    Event *ev = xrealloc(NULL, p->count * sizeof *ev);
    int n = build_run(p, ev);

    Stats st = {0};
    for (int i = 0; i < n; i++) {
        int lo, hi;
        mark_range(ev[i].v, &lo, &hi);
        st.best += hi;
        st.worst += lo;
        st.marks += play_event(&ev[i], i, n, &st);
    }
    free(ev);
    show_summary(p, &st);
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "usage: %s [packs_dir]\n", argv[0]);
        return 1;
    }
    load_packs(argc == 2 ? argv[1] : "packs");

    srand((unsigned)time(NULL));
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();
    init_pair(C_OK,   COLOR_GREEN,  -1);
    init_pair(C_BAD,  COLOR_RED,    -1);
    init_pair(C_DIM,  COLOR_WHITE,  -1);
    init_pair(C_WARN, COLOR_YELLOW, -1);
    timeout(16);

    int p;
    while ((p = choose_pack()) >= 0) run_pack(&packs[p]);

    endwin();
    return 0;
}
