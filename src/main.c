// Typing game: packs (text files) -> scenarios with hidden variants -> answers typed in your own words.
// A lexicon maps phrases to intents ("of course" means yes). Every outcome carries a mark that adds
// up to the run's rating; fancier and fresher phrasing earns style points on top.
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <ncurses.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

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

/* ---------- data ---------- */

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
} Choice;

/* One hidden version of a scenario. The player sees its detail (the cue)
   and has to work out which answer is right this time. */
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

static Pack   *packs;
static int     npacks;
static Intent *lex;
static int     nlex, lexcap;

/* ---------- utils ---------- */

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
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

// Lowercase, drop apostrophes, turn other punctuation into spaces, collapse spaces:
// "I'm good!" -> "im good". Phrases from files and the player's input both go through it.
static void normalize(const char *in, char *out, size_t cap) {
    size_t n = 0;
    int space = 0;
    for (; *in && n + 1 < cap; in++) {
        unsigned char c = (unsigned char)*in;
        if (c == '\'') continue;
        if (isalnum(c)) {
            if (space && n > 0) {
                if (n + 2 >= cap) break;
                out[n++] = ' ';
            }
            space = 0;
            out[n++] = (char)tolower(c);
        } else {
            space = 1;
        }
    }
    out[n] = '\0';
}

// Adds a phrase to a list; a duplicate keeps the higher tier.
static void phrase_push(Phrase **arr, int *n, int *cap, const char *text, int tier) {
    for (int i = 0; i < *n; i++)
        if (!strcmp((*arr)[i].text, text)) {
            if (tier > (*arr)[i].tier) (*arr)[i].tier = tier;
            return;
        }
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 8;
        *arr = xrealloc(*arr, (size_t)*cap * sizeof **arr);
    }
    (*arr)[*n].text = xstrdup(text);
    (*arr)[*n].tier = tier;
    (*n)++;
}

/* ---------- file parsing helpers ---------- */

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

// Splits s on sep in place and trims each part. Returns the number of parts;
// max + 1 means there were more than max.
static int split_on(char *s, char sep, char **parts, int max) {
    int n = 0;
    while (n < max) {
        char *bar = strchr(s, sep);
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

static int all_digits(const char *s) {
    if (!*s) return 0;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

static int name_ok(const char *s) {
    if (!*s) return 0;
    for (; *s; s++)
        if (!(islower((unsigned char)*s) || isdigit((unsigned char)*s) || *s == '_')) return 0;
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

// Reads one non-empty, non-comment line as key + value. Returns 0 at end of file.
static int next_kv(FILE *f, const char *path, int *lineno, char *buf, char **key, char **val) {
    while (fgets(buf, LINE_BUF, f)) {
        (*lineno)++;
        if (!strchr(buf, '\n') && !feof(f))
            die_at(path, *lineno, "line too long (max %d characters)", LINE_BUF - 2);
        char *line = trim(buf);
        if (*line == '\0' || *line == '#') continue;
        if (!is_ascii_text(line))
            die_at(path, *lineno, "only plain ASCII text is supported for now (no Cyrillic, no fancy quotes or dashes)");
        char *colon = strchr(line, ':');
        if (!colon) die_at(path, *lineno, "expected 'key: value'");
        *colon = '\0';
        *key = trim(line);
        *val = trim(colon + 1);
        return 1;
    }
    return 0;
}

/* ---------- lexicon ---------- */

static const Intent *find_intent(const char *name) {
    for (int i = 0; i < nlex; i++)
        if (!strcmp(lex[i].name, name)) return &lex[i];
    return NULL;
}

// Every intent needs phrases and a hint. Typing the hint itself also works:
// it is the plainest possible answer, so it counts as tier 1.
static void finish_intent(const char *path, int line, Intent *in) {
    if (in->n == 0) die_at(path, line, "intent '%s' has no phrases", in->name);
    if (!in->hint)  die_at(path, line, "intent '%s' has no 'hint'", in->name);
    char norm[LINE_BUF];
    normalize(in->hint, norm, sizeof norm);
    phrase_push(&in->ph, &in->n, &in->cap, norm, 1);
}

// lexicon.lex lives next to the packs. Without it every choice is a literal phrase.
static void load_lexicon(const char *dir) {
    char path[1024];
    snprintf(path, sizeof path, "%s/lexicon.lex", dir);
    FILE *f = fopen(path, "r");
    if (!f) {
        if (errno == ENOENT) return;
        fprintf(stderr, "cannot open '%s': %s\n", path, strerror(errno));
        exit(1);
    }
    char buf[LINE_BUF], *key, *val;
    int lineno = 0;
    Intent *cur = NULL;
    while (next_kv(f, path, &lineno, buf, &key, &val)) {
        if (!strcmp(key, "intent")) {
            if (cur) finish_intent(path, lineno, cur);
            if (!name_ok(val)) die_at(path, lineno, "intent name must use lowercase letters, digits and '_'");
            if (find_intent(val)) die_at(path, lineno, "intent '%s' is defined twice", val);
            if (nlex == lexcap) {
                lexcap = lexcap ? lexcap * 2 : 32;
                lex = xrealloc(lex, (size_t)lexcap * sizeof *lex);
            }
            cur = &lex[nlex++];
            memset(cur, 0, sizeof *cur);
            cur->name = xstrdup(val);
        } else if (!strcmp(key, "1") || !strcmp(key, "2") || !strcmp(key, "3")) {
            if (!cur) die_at(path, lineno, "phrases before the first 'intent:'");
            int tier = key[0] - '0';
            char *parts[MAX_SPLIT + 1];
            int n = split_on(val, '|', parts, MAX_SPLIT);
            if (n > MAX_SPLIT) die_at(path, lineno, "too many phrases on one line (max %d)", MAX_SPLIT);
            for (int i = 0; i < n; i++) {
                char norm[LINE_BUF];
                normalize(parts[i], norm, sizeof norm);
                if (!*norm) die_at(path, lineno, "empty phrase (check the '|' signs)");
                for (int j = 0; j < cur->n; j++)
                    if (!strcmp(cur->ph[j].text, norm))
                        die_at(path, lineno, "phrase '%s' is listed twice in intent '%s'", parts[i], cur->name);
                phrase_push(&cur->ph, &cur->n, &cur->cap, norm, tier);
            }
        } else if (!strcmp(key, "hint")) {
            if (!cur) die_at(path, lineno, "'hint' before the first 'intent:'");
            if (cur->hint) die_at(path, lineno, "second 'hint' in intent '%s'", cur->name);
            char norm[LINE_BUF];
            normalize(val, norm, sizeof norm);
            if (!*norm) die_at(path, lineno, "empty hint");
            cur->hint = xstrdup(val);
        } else {
            die_at(path, lineno, "unknown key '%s' (expected 'intent', 'hint', '1', '2' or '3')", key);
        }
    }
    fclose(f);
    if (cur) finish_intent(path, lineno, cur);
}

/* ---------- pack loading ---------- */

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

// 'choice: yes + buy | ...': each part is an intent from the lexicon,
// or, if no intent has that name, a literal phrase to type.
static void parse_choice_words(const char *file, int line, Choice *c, char *words) {
    char *toks[MAX_TOKENS + 1];
    int nt = split_on(words, '+', toks, MAX_TOKENS);
    if (nt > MAX_TOKENS) die_at(file, line, "at most %d parts joined with '+'", MAX_TOKENS);
    for (int t = 0; t < nt; t++) {
        if (!*toks[t]) die_at(file, line, "empty part in choice (check the '+' signs)");
        const Intent *in = find_intent(toks[t]);
        if (in) {
            for (int i = 0; i < in->n; i++) phrase_push(&c->ph, &c->nph, &c->cap, in->ph[i].text, in->ph[i].tier);
            if (!c->label) c->label = xstrdup(in->hint);
            continue;
        }
        if (strchr(toks[t], '_'))
            die_at(file, line, "unknown intent '%s' (not in lexicon.lex)", toks[t]);
        char norm[LINE_BUF];
        normalize(toks[t], norm, sizeof norm);
        if (!*norm) die_at(file, line, "choice '%s' has no letters or digits to type", toks[t]);
        phrase_push(&c->ph, &c->nph, &c->cap, norm, 1);
        if (!c->label) c->label = xstrdup(toks[t]);
    }
}

// Two choices in one variant must not accept the same phrase,
// otherwise the game could not tell which one the player meant.
static void check_scenario(const char *file, const Scenario *s) {
    if (s->time_limit <= 0) die_at(file, s->src_line, "scenario '%s': missing 'time'", s->title);
    if (s->nvariants == 0)  die_at(file, s->src_line, "scenario '%s': needs at least one 'choice'", s->title);
    for (int k = 0; k < s->nvariants; k++) {
        const Variant *v = &s->variants[k];
        if (!v->timeout_text) die_at(file, v->src_line, "scenario '%s': missing 'timeout'", s->title);
        if (v->nchoices < 1)  die_at(file, v->src_line, "scenario '%s': needs at least one 'choice'", s->title);
        for (int i = 0; i < v->nchoices; i++)
            for (int j = i + 1; j < v->nchoices; j++)
                for (int a = 0; a < v->choices[i].nph; a++)
                    for (int b = 0; b < v->choices[j].nph; b++)
                        if (!strcmp(v->choices[i].ph[a].text, v->choices[j].ph[b].text))
                            die_at(file, v->src_line,
                                   "scenario '%s': choices '%s' and '%s' both accept \"%s\"",
                                   s->title, v->choices[i].label, v->choices[j].label,
                                   v->choices[i].ph[a].text);
    }
}

static void load_pack_file(const char *dir, const char *fname, Pack *pk) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, fname);
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open '%s': %s\n", path, strerror(errno)); exit(1); }

    char buf[LINE_BUF], *key, *val;
    int lineno = 0;
    while (next_kv(f, path, &lineno, buf, &key, &val)) {
        Scenario *cur = pk->count ? &pk->scen[pk->count - 1] : NULL;

        if (!strcmp(key, "pack")) {
            if (!*val) die_at(path, lineno, "empty pack name");
            pk->name = xstrdup(val);
        } else if (!strcmp(key, "verdicts")) {
            if (pk->nverdicts) die_at(path, lineno, "second 'verdicts' line");
            char *parts[MAX_VERDICTS + 1];
            int n = split_on(val, '|', parts, MAX_VERDICTS);
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
            int n = split_on(val, '|', parts, MAX_PHASES);
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
                pk->scen = xrealloc(pk->scen, (size_t)pk->cap * sizeof *pk->scen);
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
            int n = split_on(val, '|', parts, 2);
            if (n > 2) die_at(path, lineno, "'timeout' is: what happens | mark (mark is optional)");
            if (!*parts[0]) die_at(path, lineno, "empty timeout text");
            if (n == 2 && !parse_int(parts[1], -MARK_LIMIT, MARK_LIMIT, &v->timeout_mark))
                die_at(path, lineno, "mark must be an integer from %d to %d", -MARK_LIMIT, MARK_LIMIT);
            v->timeout_text = xstrdup(parts[0]);
        } else if (!strcmp(key, "choice")) {
            Variant *v = cur_variant(path, lineno, cur);
            if (v->nchoices == MAX_CHOICES)
                die_at(path, lineno, "at most %d choices per variant", MAX_CHOICES);
            char *parts[5];
            int n = split_on(val, '|', parts, 4);
            if (n < 2 || n > 4)
                die_at(path, lineno, "'choice' is: words | what happens | mark | hint (mark and hint are optional)");
            if (!*parts[1]) die_at(path, lineno, "empty choice result");
            int mark = 0;
            if (n >= 3 && !parse_int(parts[2], -MARK_LIMIT, MARK_LIMIT, &mark))
                die_at(path, lineno, "mark must be an integer from %d to %d", -MARK_LIMIT, MARK_LIMIT);
            Choice *c = &v->choices[v->nchoices++];
            memset(c, 0, sizeof *c);
            if (n == 4) {
                // the choice's own hint: shown instead of the intent's, and typing it works
                char norm[LINE_BUF];
                normalize(parts[3], norm, sizeof norm);
                if (!*norm) die_at(path, lineno, "empty hint");
                c->label = xstrdup(parts[3]);
                phrase_push(&c->ph, &c->nph, &c->cap, norm, 1);
            }
            parse_choice_words(path, lineno, c, parts[0]);
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
        if (n == cap) { cap = cap ? cap * 2 : 8; names = xrealloc(names, (size_t)cap * sizeof *names); }
        names[n++] = xstrdup(e->d_name);
    }
    closedir(d);
    if (n == 0) { fprintf(stderr, "no pack files (*.txt) in '%s'\n", dir); exit(1); }
    if (n > MAX_PACKS) { fprintf(stderr, "too many pack files in '%s' (max %d)\n", dir, MAX_PACKS); exit(1); }

    qsort(names, (size_t)n, sizeof *names, cmp_names);
    packs = xrealloc(NULL, (size_t)n * sizeof *packs);
    memset(packs, 0, (size_t)n * sizeof *packs);
    for (int i = 0; i < n; i++) {
        load_pack_file(dir, names[i], &packs[i]);
        npacks++;
    }
}

/* ---------- matching what the player typed ---------- */

// Edit distance, giving up early once it is surely above cap.
static int lev(const char *a, const char *b, int cap) {
    static int prev[LINE_BUF + 1], curr[LINE_BUF + 1];
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la > LINE_BUF || lb > LINE_BUF || abs(la - lb) > cap) return cap + 1;
    for (int j = 0; j <= lb; j++) prev[j] = j;
    for (int i = 1; i <= la; i++) {
        curr[0] = i;
        int rowmin = i;
        for (int j = 1; j <= lb; j++) {
            int best = prev[j] + 1;
            if (curr[j - 1] + 1 < best) best = curr[j - 1] + 1;
            int diag = prev[j - 1] + (a[i - 1] != b[j - 1]);
            if (diag < best) best = diag;
            curr[j] = best;
            if (best < rowmin) rowmin = best;
        }
        if (rowmin > cap) return cap + 1;
        memcpy(prev, curr, (size_t)(lb + 1) * sizeof *prev);
    }
    return prev[lb];
}

typedef struct {
    int         choice;     // -1 if nothing matched
    int         tier;
    int         typo;       // matched only with a small typo
    const char *phrase;     // the phrase it matched (for repetition tracking)
} Match;

// Exact match first; then, for inputs of 4+ characters, the single closest
// phrase within 1 edit (2 for 8+ characters). A tie between choices is no match.
static Match match_input(const Variant *v, const char *in) {
    Match m = { -1, 0, 0, NULL };
    for (int c = 0; c < v->nchoices; c++)
        for (int p = 0; p < v->choices[c].nph; p++)
            if (!strcmp(in, v->choices[c].ph[p].text)) {
                m.choice = c; m.tier = v->choices[c].ph[p].tier; m.phrase = v->choices[c].ph[p].text;
                return m;
            }
    int len = (int)strlen(in);
    if (len < 4) return m;
    int cap = len >= 8 ? 2 : 1;
    int best = cap + 1, who = -1, tie = 0, tier = 0;
    const char *phrase = NULL;
    for (int c = 0; c < v->nchoices; c++)
        for (int p = 0; p < v->choices[c].nph; p++) {
            const Phrase *ph = &v->choices[c].ph[p];
            int d = lev(in, ph->text, cap);
            if (d < best) {
                best = d; who = c; tie = 0; tier = ph->tier; phrase = ph->text;
            } else if (d == best && d <= cap) {
                if (c != who) tie = 1;
                else if (ph->tier > tier) { tier = ph->tier; phrase = ph->text; }
            }
        }
    if (who >= 0 && best <= cap && !tie) {
        m.choice = who; m.tier = tier; m.typo = 1; m.phrase = phrase;
    }
    return m;
}

/* ---------- style: fancy and fresh phrasing pays ---------- */

/* Every phrase has a "heat": +1 each time it is used, halved at the start of
   every run. Points are divided by (1 + heat), so a phrase used every day
   pays about a third, and recovers if you leave it alone for a while. */

typedef struct { char *phrase; double heat; } Heat;

static Heat *heats;
static int   nheats, heatcap;
static char  style_file[1024];

static double *heat_of(const char *phrase, int create) {
    for (int i = 0; i < nheats; i++)
        if (!strcmp(heats[i].phrase, phrase)) return &heats[i].heat;
    if (!create) return NULL;
    if (nheats == heatcap) {
        heatcap = heatcap ? heatcap * 2 : 64;
        heats = xrealloc(heats, (size_t)heatcap * sizeof *heats);
    }
    heats[nheats].phrase = xstrdup(phrase);
    heats[nheats].heat = 0;
    return &heats[nheats++].heat;
}

static void style_load(void) {
    const char *xdg = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
    if (xdg && *xdg)        snprintf(style_file, sizeof style_file, "%s/typed_stories/style.txt", xdg);
    else if (home && *home) snprintf(style_file, sizeof style_file, "%s/.local/share/typed_stories/style.txt", home);
    else return;
    FILE *f = fopen(style_file, "r");
    if (!f) return;
    char line[LINE_BUF];
    while (fgets(line, sizeof line, f)) {
        char *sp = strchr(line, ' ');
        if (!sp) continue;
        *sp = '\0';
        char *end;
        double h = strtod(line, &end);
        if (end == line || *end || h <= 0 || h > 1000) continue;
        char *ph = trim(sp + 1);
        if (*ph) *heat_of(ph, 1) = h;
    }
    fclose(f);
}

static void mkdirs_for(const char *file) {
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", file);
    for (char *p = tmp + 1; *p; p++)
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
}

// Best effort: if the file can't be written, style simply doesn't remember.
static void style_save(void) {
    if (!*style_file) return;
    mkdirs_for(style_file);
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", style_file);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    for (int i = 0; i < nheats; i++)
        if (heats[i].heat >= 0.01) fprintf(f, "%.4f %s\n", heats[i].heat, heats[i].phrase);
    if (fclose(f) == 0) rename(tmp, style_file);
    else remove(tmp);
}

static void style_new_run(void) {
    for (int i = 0; i < nheats; i++) heats[i].heat *= 0.5;
}

static int style_points(int tier, int typo, double heat) {
    static const int base[4] = { 0, 10, 30, 60 };
    double p = base[tier] / (1.0 + heat);
    if (typo) p *= 0.5;
    return (int)(p + 0.5);
}

static char style_rank(int pts) {
    return pts >= 50 ? 'S' : pts >= 35 ? 'A' : pts >= 20 ? 'B' : pts >= 10 ? 'C' : 'D';
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
        memmove(order + 1, order, (size_t)n * sizeof *order);
        order[0] = first;
        n++;
    }
    if (last >= 0) order[n++] = last;
    return n;
}

// Phases go in the listed order; inside a phase, 'pick' random scenarios.
// Every scenario then gets one of its variants, by weight.
static int build_run(const Pack *p, Event *ev) {
    int *idx = xrealloc(NULL, (size_t)p->count * sizeof *idx);
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
        for (const char *p = phase; *p; p++) addch((chtype)toupper((unsigned char)*p));
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

// The hints say what each answer means ("agree / refuse"); the words are up to the player.
static void draw_prompt(const Event *e, const char *input, const char *msg, double left,
                        int idx, int total) {
    const Variant *v = e->v;
    erase();
    draw_header(idx, total, e->phase);
    int r = draw_situation(e->s, v, 0) + 1;

    {
        char hint[512];
        size_t n = 0;
        hint[0] = '\0';
        for (int c = 0; c < v->nchoices && n < sizeof hint; c++)
            n += (size_t)snprintf(hint + n, sizeof hint - n, "%s%s", c ? "  /  " : "", v->choices[c].label);
        attron(COLOR_PAIR(C_DIM));
        r += print_wrapped(r, 4, hint) + 1;
        attroff(COLOR_PAIR(C_DIM));
    }

    mvaddstr(r, 4, "> ");
    attron(A_BOLD);
    addstr(input);
    attroff(A_BOLD);
    attron(A_DIM);
    addch('_');
    attroff(A_DIM);
    r += 2;

    // time bar
    int bar_w = COLS - 14;              // "[" + bar + "] 120.0s" must fit on one row
    if (bar_w > 30) bar_w = 30;
    if (bar_w < 5) bar_w = 5;
    double limit = e->s->time_limit;
    int filled = (int)(bar_w * (left / limit));
    if (filled < 0) filled = 0;
    int pair = left < limit * 0.3 ? C_BAD : (left < limit * 0.6 ? C_WARN : C_OK);
    attron(COLOR_PAIR(pair));
    mvprintw(r, 4, "[");
    for (int i = 0; i < bar_w; i++) addch(i < filled ? '#' : ' ');
    printw("] %.1fs", left > 0 ? left : 0);
    attroff(COLOR_PAIR(pair));

    if (*msg) {
        attron(COLOR_PAIR(C_BAD));
        print_wrapped(r + 2, 4, msg);
        attroff(COLOR_PAIR(C_BAD));
    }
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

// Situation screen: the player reads the text and the cue; the input
// and the timer start only when Enter is pressed.
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
    int    typed, erased, sent; // printable keys, keys erased, characters in accepted answers
    double typing_time;
    int    timeouts, misses;    // misses: answers nobody understood
    int    style;
} Stats;

static int is_backspace(int ch) {
    return ch == KEY_BACKSPACE || ch == 127 || ch == 8;
}

// Plays one event and returns its mark. The mark is never shown: the player
// has to read it from the reaction. Style points are shown, they are about words.
static int play_event(const Event *e, int idx, int total, Stats *st) {
    const Scenario *s = e->s;
    const Variant  *v = e->v;
    char input[INPUT_MAX + 1] = "", msg[200] = "";
    int len = 0;

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

        draw_prompt(e, input, msg, left, idx, total);

        int ch = getch();
        if (ch == ERR || ch == KEY_RESIZE) continue;

        if (is_backspace(ch)) {
            if (len) { input[--len] = '\0'; st->erased++; }
            continue;
        }
        if (ch == 23) {                         // Ctrl-W: erase the last word
            while (len && input[len - 1] == ' ') { input[--len] = '\0'; st->erased++; }
            while (len && input[len - 1] != ' ') { input[--len] = '\0'; st->erased++; }
            continue;
        }
        if (ch == 21) {                         // Ctrl-U: erase everything
            st->erased += len;
            len = 0;
            input[0] = '\0';
            continue;
        }
        if (ch >= 32 && ch <= 126) {
            if (len < INPUT_MAX) {
                input[len++] = (char)ch;
                input[len] = '\0';
                st->typed++;
                msg[0] = '\0';
            }
            continue;
        }
        if (ch != '\n' && ch != '\r' && ch != KEY_ENTER) continue;

        char norm[INPUT_MAX + 1];
        normalize(input, norm, sizeof norm);
        if (!*norm) continue;
        Match m = match_input(v, norm);
        if (m.choice < 0) {
            snprintf(msg, sizeof msg, "Huh? Nobody gets '%s'. Try again.", input);
            st->misses++;
            len = 0;
            input[0] = '\0';
            continue;
        }

        const Choice *c = &v->choices[m.choice];
        double elapsed = now() - start;
        st->typing_time += elapsed;
        st->sent += len;

        double *heat = heat_of(m.phrase, 1);
        double before = *heat;
        int pts = style_points(m.tier, m.typo, before);
        *heat += 1.0;
        st->style += pts;

        erase();
        draw_header(idx, total, e->phase);
        int r = draw_situation(s, v, 0) + 1;
        attron(COLOR_PAIR(C_OK));
        mvprintw(r, 4, "\"%s\"", input);
        attroff(COLOR_PAIR(C_OK));
        r += 2;
        r += print_wrapped(r, 4, c->result);
        char note[200];
        snprintf(note, sizeof note, "style %c +%d%s%s   %.1fs", style_rank(pts), pts,
                 m.typo ? "  (typo)" : "", before >= 0.5 ? "  (heard that before)" : "", elapsed);
        attron(COLOR_PAIR(C_DIM));
        int nl = print_wrapped(r + 1, 4, note);
        attroff(COLOR_PAIR(C_DIM));
        mvprintw(r + 1 + nl + 1, 4, "(press any key)");
        refresh();
        wait_key();
        return c->mark;
    }
}

// Rating scales the run's marks between the worst and the best possible run,
// in integer math so the boundaries are exact.
static void show_summary(const Pack *p, const Stats *st, int events) {
    int span = st->best - st->worst, got = st->marks - st->worst;
    int rating = span > 0 ? (got * 20 + span) / (2 * span) : 10;
    int vi = span > 0 ? got * p->nverdicts / span : p->nverdicts - 1;
    if (vi >= p->nverdicts) vi = p->nverdicts - 1;
    if (vi < 0) vi = 0;
    double acc = st->typed ? 100.0 * (st->typed - st->erased) / st->typed : 100.0;
    double wpm = st->typing_time > 0 ? (st->sent / 5.0) / (st->typing_time / 60.0) : 0;
    int avg = events ? (st->style + events / 2) / events : 0;

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
    mvprintw(r,     4, "Style:    %c (avg %d per answer)", style_rank(avg), avg);
    mvprintw(r + 1, 4, "Typing:   %.0f%% accuracy, %.0f wpm", acc, wpm);
    mvprintw(r + 2, 4, "Timeouts: %d, not understood: %d", st->timeouts, st->misses);
    mvprintw(r + 4, 4, "(press any key)");
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
    Event *ev = xrealloc(NULL, (size_t)p->count * sizeof *ev);
    int n = build_run(p, ev);

    style_new_run();
    Stats st = {0};
    for (int i = 0; i < n; i++) {
        int lo, hi;
        mark_range(ev[i].v, &lo, &hi);
        st.best += hi;
        st.worst += lo;
        st.marks += play_event(&ev[i], i, n, &st);
    }
    free(ev);
    style_save();
    show_summary(p, &st, n);
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "usage: %s [packs_dir]\n", argv[0]);
        return 1;
    }
    const char *dir = argc == 2 ? argv[1] : "packs";
    load_lexicon(dir);
    load_packs(dir);
    style_load();

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
