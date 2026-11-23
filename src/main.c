// Typing game prototype: packs (text files) -> scenarios -> choices typed under a timer.
#include <dirent.h>
#include <errno.h>
#include <ncurses.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_CHOICES   4
#define MAX_PACKS     9     // the menu picks a pack with one digit key
#define LINE_BUF      1024
#define BASE_POINTS   100   // default points per scenario (speed can double them)
#define ERROR_PENALTY 10    // points lost per wrong key

/* ---------- data ---------- */

/* What happens when the timer runs out. */
enum {
    TO_TEXT,    // a fixed outcome, described by the timeout text ("you go straight")
    TO_RANDOM   // the timeout text, then a random choice happens anyway
};

/* Where a scenario sits in a run. */
enum { SLOT_ANY, SLOT_FIRST, SLOT_LAST };

typedef struct {
    char *word;     // what to type
    char *result;   // what happens
    int   bonus;    // flat points on top of speed (can be negative)
} Choice;

typedef struct {
    char   *title;          // the situation
    double  time_limit;     // seconds
    int     on_timeout;     // TO_*
    char   *timeout_text;   // what happens if you hesitate
    int     base;           // points for this scenario
    int     slot;           // SLOT_*
    int     nchoices;
    Choice  choices[MAX_CHOICES];
    int     src_line;       // for error messages
} Scenario;

typedef struct {
    char     *name;
    char     *verdicts[4];  // worst -> best
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
    void *q = realloc(p, n);
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

static int word_ok(const char *w) {
    if (!*w) return 0;
    for (; *w; w++)
        if ((unsigned char)*w < 32 || (unsigned char)*w > 126) return 0;
    return 1;
}

static int is_prefix(const char *a, const char *b) {
    return strncmp(a, b, strlen(a)) == 0;
}

// A choice that is a prefix of another one in the same scenario could never be
// told apart while typing, so reject it.
static void validate_scenario(const char *file, const Scenario *s) {
    if (s->time_limit <= 0) die_at(file, s->src_line, "scenario '%s': missing 'time'", s->title);
    if (!s->timeout_text)   die_at(file, s->src_line, "scenario '%s': missing 'timeout'", s->title);
    if (s->nchoices < 1)    die_at(file, s->src_line, "scenario '%s': needs at least one 'choice'", s->title);
    for (int i = 0; i < s->nchoices; i++)
        for (int j = i + 1; j < s->nchoices; j++) {
            const char *a = s->choices[i].word, *b = s->choices[j].word;
            if (is_prefix(a, b) || is_prefix(b, a))
                die_at(file, s->src_line,
                       "scenario '%s': \"%s\" and \"%s\" overlap (one is the start of the other)",
                       s->title, a, b);
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
        char *colon = strchr(line, ':');
        if (!colon) die_at(path, lineno, "expected 'key: value'");
        *colon = '\0';
        char *key = trim(line), *val = trim(colon + 1);
        Scenario *cur = pk->count ? &pk->scen[pk->count - 1] : NULL;

        if (!strcmp(key, "pack")) {
            if (!*val) die_at(path, lineno, "empty pack name");
            pk->name = xstrdup(val);
        } else if (!strcmp(key, "verdicts")) {
            char *parts[5];
            if (split_pipe(val, parts, 5) != 4)
                die_at(path, lineno, "'verdicts' needs exactly 4 entries separated by '|'");
            for (int i = 0; i < 4; i++) {
                if (!*parts[i]) die_at(path, lineno, "empty verdict");
                pk->verdicts[i] = xstrdup(parts[i]);
            }
        } else if (!strcmp(key, "scenario")) {
            if (cur) validate_scenario(path, cur);
            if (!*val) die_at(path, lineno, "empty scenario title");
            if (pk->count == pk->cap) {
                pk->cap = pk->cap ? pk->cap * 2 : 8;
                pk->scen = xrealloc(pk->scen, pk->cap * sizeof *pk->scen);
            }
            Scenario *s = &pk->scen[pk->count++];
            memset(s, 0, sizeof *s);
            s->title = xstrdup(val);
            s->base = BASE_POINTS;
            s->slot = SLOT_ANY;
            s->on_timeout = TO_TEXT;
            s->src_line = lineno;
        } else if (!cur) {
            die_at(path, lineno, "'%s' before the first 'scenario:'", key);
        } else if (!strcmp(key, "time")) {
            char *end;
            double t = strtod(val, &end);
            if (end == val || *end || t <= 0 || t > 120)
                die_at(path, lineno, "'time' must be a number of seconds, more than 0 and up to 120");
            cur->time_limit = t;
        } else if (!strcmp(key, "timeout")) {
            if (!*val) die_at(path, lineno, "empty timeout text");
            cur->timeout_text = xstrdup(val);
        } else if (!strcmp(key, "on_timeout")) {
            if (!strcmp(val, "text"))        cur->on_timeout = TO_TEXT;
            else if (!strcmp(val, "random")) cur->on_timeout = TO_RANDOM;
            else die_at(path, lineno, "'on_timeout' must be 'text' or 'random'");
        } else if (!strcmp(key, "base")) {
            char *end;
            long b = strtol(val, &end, 10);
            if (end == val || *end || b <= 0 || b > 1000)
                die_at(path, lineno, "'base' must be an integer from 1 to 1000");
            cur->base = (int)b;
        } else if (!strcmp(key, "slot")) {
            if (!strcmp(val, "first"))      cur->slot = SLOT_FIRST;
            else if (!strcmp(val, "last"))  cur->slot = SLOT_LAST;
            else if (!strcmp(val, "any"))   cur->slot = SLOT_ANY;
            else die_at(path, lineno, "'slot' must be 'first', 'last' or 'any'");
        } else if (!strcmp(key, "choice")) {
            if (cur->nchoices == MAX_CHOICES)
                die_at(path, lineno, "at most %d choices per scenario", MAX_CHOICES);
            char *parts[4];
            int n = split_pipe(val, parts, 4);
            if (n < 2 || n > 3)
                die_at(path, lineno, "'choice' is: word | what happens | bonus (bonus is optional)");
            if (!word_ok(parts[0]))
                die_at(path, lineno, "choice word must be non-empty printable ASCII");
            if (!*parts[1]) die_at(path, lineno, "empty choice result");
            int bonus = 0;
            if (n == 3) {
                char *end;
                long b = strtol(parts[2], &end, 10);
                if (end == parts[2] || *end) die_at(path, lineno, "bonus must be an integer");
                bonus = (int)b;
            }
            Choice *c = &cur->choices[cur->nchoices++];
            c->word = xstrdup(parts[0]);
            c->result = xstrdup(parts[1]);
            c->bonus = bonus;
        } else {
            die_at(path, lineno, "unknown key '%s'", key);
        }
    }
    fclose(f);

    if (!pk->name)         die_at(path, lineno, "missing 'pack:' name");
    if (!pk->verdicts[0])  die_at(path, lineno, "missing 'verdicts:' line");
    if (pk->count == 0)    die_at(path, lineno, "no scenarios");
    validate_scenario(path, &pk->scen[pk->count - 1]);

    int firsts = 0, lasts = 0;
    for (int i = 0; i < pk->count; i++) {
        firsts += pk->scen[i].slot == SLOT_FIRST;
        lasts  += pk->scen[i].slot == SLOT_LAST;
    }
    if (firsts > 1 || lasts > 1)
        die_at(path, lineno, "at most one 'slot: first' and one 'slot: last' per pack");
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

static void draw_prompt(const Scenario *s, const int *alive, int pos, double left,
                        int flash_err, int idx, int total) {
    erase();
    mvprintw(1, 4, "[%d/%d]", idx + 1, total);
    int crow = 3 + print_wrapped(3, 4, s->title) + 2;   // first row of choices

    // choices: typed part green, rest dim; choices you have moved away from fade
    for (int c = 0; c < s->nchoices; c++) {
        const char *w = s->choices[c].word;
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
    int bar_row = crow + s->nchoices + 2;
    int bar_w = COLS - 14;              // "[" + bar + "] 120.0s" must fit on one row
    if (bar_w > 30) bar_w = 30;
    if (bar_w < 5) bar_w = 5;
    int filled = (int)(bar_w * (left / s->time_limit));
    if (filled < 0) filled = 0;
    int pair = left < s->time_limit * 0.3 ? C_BAD : (left < s->time_limit * 0.6 ? C_WARN : C_OK);
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

// Situation screen: the player reads the text; the choices stay hidden
// and the timer does not run until Enter is pressed.
static void show_intro(const Scenario *s, int idx, int total) {
    erase();
    mvprintw(1, 4, "[%d/%d]", idx + 1, total);
    attron(A_BOLD);
    int r = 4 + print_wrapped(4, 4, s->title);
    attroff(A_BOLD);
    attron(COLOR_PAIR(C_DIM));
    print_wrapped(r + 2, 4, "Press Enter when ready. The timer starts right after.");
    attroff(COLOR_PAIR(C_DIM));
    refresh();
    wait_enter();
}

/* ---------- gameplay ---------- */

// returns points for this scenario; fills errors and result flag
static int play_scenario(const Scenario *s, int idx, int total, int *errors_out, int *ok_out) {
    int alive[MAX_CHOICES], pos = 0, errors = 0, flash_frames = 0;
    for (int c = 0; c < s->nchoices; c++) alive[c] = 1;

    show_intro(s, idx, total);
    double start = now();

    while (1) {
        double left = s->time_limit - (now() - start);
        if (left <= 0) {
            // hesitated: no points, and the scenario decides what happens
            *ok_out = 0; *errors_out += errors;
            const Choice *fate = NULL;
            if (s->on_timeout == TO_RANDOM) fate = &s->choices[rand() % s->nchoices];

            erase();
            int r = 3 + print_wrapped(3, 4, s->title) + 2;
            attron(COLOR_PAIR(C_BAD));
            mvprintw(r, 4, "TIME'S UP");
            attroff(COLOR_PAIR(C_BAD));
            r += 2;
            r += print_wrapped(r, 4, s->timeout_text);
            if (fate) r += print_wrapped(r, 4, fate->result);
            mvprintw(r + 2, 4, "(press any key)");
            refresh();
            wait_key();
            return 0;
        }

        draw_prompt(s, alive, pos, left, flash_frames > 0, idx, total);
        if (flash_frames > 0) flash_frames--;

        int ch = getch();
        if (ch == ERR || ch == KEY_RESIZE) continue;

        // does any still-possible choice continue with this key?
        int match = 0;
        for (int c = 0; c < s->nchoices; c++)
            if (alive[c] && s->choices[c].word[pos] == ch) match = 1;
        if (!match) {
            errors++;
            flash_frames = 6;   // brief red mark on the expected letter
            continue;
        }

        for (int c = 0; c < s->nchoices; c++)
            if (alive[c] && s->choices[c].word[pos] != ch) alive[c] = 0;
        pos++;

        int chosen = -1;
        for (int c = 0; c < s->nchoices; c++)
            if (alive[c] && s->choices[c].word[pos] == '\0') chosen = c;
        if (chosen < 0) continue;

        // points do not depend on word length: the time limit already scales with it
        const Choice *c = &s->choices[chosen];
        double elapsed = now() - start;
        double ratio = 1.0 - elapsed / s->time_limit;       // 0..1
        if (ratio < 0) ratio = 0;
        int speed = (int)(s->base * ratio);
        int pts   = s->base + speed + c->bonus - errors * ERROR_PENALTY;
        if (pts < 0) pts = 0;
        *ok_out = 1; *errors_out += errors;

        erase();
        int r = 3 + print_wrapped(3, 4, s->title) + 2;
        attron(COLOR_PAIR(C_OK));
        mvprintw(r, 4, "%s", c->word);
        attroff(COLOR_PAIR(C_OK));
        r += 2;
        r += print_wrapped(r, 4, c->result);
        mvprintw(r + 1, 4, "+%d  (%.2fs, %d errors)", pts, elapsed, errors);
        r += 2;
        if (c->bonus) { mvprintw(r, 4, "choice effect: %+d", c->bonus); r++; }
        mvprintw(r + 1, 4, "(press any key)");
        refresh();
        wait_key();
        return pts;
    }
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

// Pinned scenarios go to the ends, everything else is shuffled in between.
static void build_order(const Pack *p, int *order) {
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
}

static void run_pack(const Pack *p) {
    int *order = xrealloc(NULL, p->count * sizeof *order);
    build_order(p, order);

    int score = 0, errors = 0, wins = 0;
    for (int i = 0; i < p->count; i++) {
        int ok = 0;
        score += play_scenario(&p->scen[order[i]], i, p->count, &errors, &ok);
        wins += ok;
    }
    free(order);

    int v = wins * 3 / p->count;       // 0..3
    if (wins == p->count && errors == 0) v = 3;
    erase();
    char head[256];
    snprintf(head, sizeof head, "RUN COMPLETE: %s", p->name);
    int r = 2 + print_wrapped(2, 4, head);          // 3 for a one-line header
    mvprintw(r + 1, 4, "Score:   %d", score);
    mvprintw(r + 2, 4, "Success: %d/%d", wins, p->count);
    mvprintw(r + 3, 4, "Errors:  %d", errors);
    int vrow = r + 5;
    attron(A_BOLD);
    int vl = print_wrapped(vrow, 4, p->verdicts[v]);
    attroff(A_BOLD);
    mvprintw(vrow + vl + 2, 4, "(press any key)");
    refresh();
    wait_key();
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
