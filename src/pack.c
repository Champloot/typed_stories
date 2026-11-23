// pack: pack files: scenarios, variants, choices, flags and conditions
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <strings.h>

#include "pack.h"
#include "lexicon.h"
#include "util.h"

Pack   *packs;
int     npacks;

/* Flags the game sets by itself at the start of a run; packs may check them. */
static const char *const builtin_flags[] = {
    FLAG_WORDSMITH,     // the word of the day was used on WORDSMITH_DAYS different days
    FLAG_TODAY,         // the run is the Today one (the same day for everyone)
};

static int is_builtin_flag(const char *name) {
    for (size_t i = 0; i < sizeof builtin_flags / sizeof *builtin_flags; i++)
        if (!strcmp(builtin_flags[i], name)) return 1;
    return 0;
}

int pack_flag(const Pack *pk, const char *name) {
    for (int i = 0; i < pk->nflags; i++)
        if (!strcmp(pk->flags[i], name)) return i;
    return -1;
}

static int meter_index(const Pack *pk, const char *name) {
    for (int i = 0; i < pk->nmeters; i++)
        if (!strcasecmp(pk->meter_names[i], name)) return i;
    return -1;
}

// "nerves -1, money +2"
static void parse_effects(const char *file, int line, const Pack *pk, char *val, Effects *fx) {
    char *parts[MAX_EFFECTS + 1];
    int n = split_on(val, ',', parts, MAX_EFFECTS);
    if (n > MAX_EFFECTS) die_at(file, line, "at most %d meters in one 'effects'", MAX_EFFECTS);
    for (int i = 0; i < n; i++) {
        char *sp = strrchr(parts[i], ' ');
        if (!sp) die_at(file, line, "'effects' is: meter +1, meter -2 (got '%s')", parts[i]);
        *sp = '\0';
        char *name = trim(parts[i]);
        int m = meter_index(pk, name), d;
        if (m < 0) die_at(file, line, "no meter '%s' (declare it in 'meters:')", name);
        if (!parse_int(trim(sp + 1), -METER_MAX, METER_MAX, &d) || d == 0)
            die_at(file, line, "meter change must be a non-zero integer from %d to %d", -METER_MAX, METER_MAX);
        if (fx->n == MAX_EFFECTS) die_at(file, line, "at most %d meter changes per outcome", MAX_EFFECTS);
        fx->meter[fx->n] = m;
        fx->delta[fx->n] = d;
        fx->n++;
    }
}

static Variant *new_variant(const char *file, int line, Scenario *s) {
    if (s->nvariants == MAX_VARIANTS)
        die_at(file, line, "at most %d variants per scenario", MAX_VARIANTS);
    Variant *v = &s->variants[s->nvariants++];
    memset(v, 0, sizeof *v);
    v->weight = 1;
    v->on_timeout = TO_TEXT;
    v->timeout_mark = TIMEOUT_MARK;
    v->last_outcome = -1;
    v->src_line = line;
    return v;
}

static int flag_index(const char *file, int line, Pack *pk, const char *name) {
    if (!name_ok(name))
        die_at(file, line, "flag name '%s' must use lowercase letters, digits and '_'", name);
    for (int i = 0; i < pk->nflags; i++)
        if (!strcmp(pk->flags[i], name)) return i;
    if (pk->nflags == MAX_FLAGS) die_at(file, line, "at most %d different flags per pack", MAX_FLAGS);
    pk->flags[pk->nflags] = xstrdup(name);
    return pk->nflags++;
}

// "a, b or !c" -> a AND (b OR not c)
static void parse_cond(const char *file, int line, Pack *pk, char *val, Cond *out) {
    memset(out, 0, sizeof *out);
    char *clauses[MAX_CLAUSES + 1];
    int nc = split_on(val, ',', clauses, MAX_CLAUSES);
    if (nc > MAX_CLAUSES) die_at(file, line, "at most %d conditions joined with ','", MAX_CLAUSES);
    for (int i = 0; i < nc; i++) {
        char *s = clauses[i];
        if (!*s) die_at(file, line, "empty condition (check the ',' signs)");
        int nl = 0;
        while (1) {
            char *o = strstr(s, " or ");
            if (o) *o = '\0';
            char *lit = trim(s);
            if (nl == MAX_LITS) die_at(file, line, "at most %d flags joined with 'or'", MAX_LITS);
            int neg = 0;
            if (*lit == '!') { neg = 1; lit = trim(lit + 1); }
            if (!*lit) die_at(file, line, "empty flag name in condition");
            int fi = flag_index(file, line, pk, lit);
            if (!pk->flag_checked[fi]) pk->flag_checked[fi] = line;
            out->c[i].flag[nl] = fi;
            out->c[i].neg[nl] = neg;
            nl++;
            if (!o) break;
            s = o + 4;
        }
        out->c[i].n = nl;
    }
    out->nclauses = nc;
}

int cond_holds(const Cond *c, uint64_t flags) {
    for (int i = 0; i < c->nclauses; i++) {
        int any = 0;
        for (int j = 0; j < c->c[i].n; j++) {
            int set = (int)((flags >> c->c[i].flag[j]) & 1u);
            if (set != c->c[i].neg[j]) { any = 1; break; }
        }
        if (!any) return 0;
    }
    return 1;
}

void apply_sets(uint64_t *flags, const FlagSet *fs) {
    for (int i = 0; i < fs->n; i++) *flags |= (uint64_t)1 << fs->flag[i];
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
        } else if (!strcmp(key, "meters")) {
            if (pk->nmeters) die_at(path, lineno, "second 'meters' line");
            if (pk->count)   die_at(path, lineno, "'meters' must come before the first scenario");
            char *parts[MAX_METERS + 1];
            int n = split_on(val, '|', parts, MAX_METERS);
            if (n > MAX_METERS) die_at(path, lineno, "at most %d meters", MAX_METERS);
            for (int i = 0; i < n; i++) {
                char *name = parts[i];
                int start = METER_START;
                char *sp = strrchr(name, ' ');
                if (sp && all_digits(sp + 1)) {
                    if (!parse_int(sp + 1, 1, METER_MAX, &start))
                        die_at(path, lineno, "a meter starts at 1 to %d", METER_MAX);
                    *sp = '\0';
                    name = trim(name);
                }
                if (!*name) die_at(path, lineno, "empty meter name");
                for (const char *c = name; *c; c++)
                    if (!isalpha((unsigned char)*c)) die_at(path, lineno, "meter name '%s' must be one word of letters", name);
                if (meter_index(pk, name) >= 0) die_at(path, lineno, "meter '%s' is listed twice", name);
                pk->meter_names[pk->nmeters] = xstrdup(name);
                pk->meter_start[pk->nmeters] = start;
                pk->nmeters++;
            }
        } else if (!strcmp(key, "ending")) {
            char *parts[3];
            int n = split_on(val, '|', parts, 2);
            if (n != 2 || !*parts[1]) die_at(path, lineno, "'ending' is: meter | what happens when it hits zero");
            int m = meter_index(pk, parts[0]);
            if (m < 0) die_at(path, lineno, "no meter '%s' (declare it in 'meters:' first)", parts[0]);
            if (pk->meter_endings[m]) die_at(path, lineno, "second 'ending' for meter '%s'", parts[0]);
            pk->meter_endings[m] = xstrdup(parts[1]);
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
        } else if (!strcmp(key, "needs")) {
            if (cur->has_needs) die_at(path, lineno, "second 'needs' in one scenario");
            parse_cond(path, lineno, pk, val, &cur->needs);
            cur->has_needs = 1;
        } else if (!strcmp(key, "if")) {
            Variant *v = cur_variant(path, lineno, cur);
            if (v->has_cond) die_at(path, lineno, "second 'if' in one variant");
            parse_cond(path, lineno, pk, val, &v->cond);
            v->has_cond = 1;
        } else if (!strcmp(key, "sets")) {
            Variant *v = cur->nvariants ? &cur->variants[cur->nvariants - 1] : NULL;
            if (!v || v->last_outcome == -1)
                die_at(path, lineno, "'sets' must come right after a 'choice' or 'timeout' line");
            FlagSet *fs = v->last_outcome == -2 ? &v->timeout_sets : &v->choices[v->last_outcome].sets;
            char *names[MAX_SETS + 1];
            int n = split_on(val, ',', names, MAX_SETS);
            if (n > MAX_SETS) die_at(path, lineno, "at most %d flags in one 'sets'", MAX_SETS);
            for (int i = 0; i < n; i++) {
                if (!*names[i]) die_at(path, lineno, "empty flag name (check the ',' signs)");
                if (is_builtin_flag(names[i]))
                    die_at(path, lineno, "'%s' is set by the game itself, packs can only check it", names[i]);
                int fi = flag_index(path, lineno, pk, names[i]);
                pk->flag_is_set[fi] = 1;
                if (fs->n == MAX_SETS) die_at(path, lineno, "at most %d flags per outcome", MAX_SETS);
                fs->flag[fs->n++] = fi;
            }
        } else if (!strcmp(key, "effects")) {
            Variant *v = cur->nvariants ? &cur->variants[cur->nvariants - 1] : NULL;
            if (!v || v->last_outcome == -1)
                die_at(path, lineno, "'effects' must come right after a 'choice' or 'timeout' line");
            if (!pk->nmeters) die_at(path, lineno, "'effects' used, but the pack has no 'meters:' line");
            parse_effects(path, lineno, pk, val,
                          v->last_outcome == -2 ? &v->timeout_fx : &v->choices[v->last_outcome].fx);
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
            v->last_outcome = -2;
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
            v->last_outcome = v->nchoices - 1;
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

    // a flag that is checked but never set is almost always a typo
    for (int i = 0; i < pk->nflags; i++)
        if (pk->flag_checked[i] && !pk->flag_is_set[i] && !is_builtin_flag(pk->flags[i]))
            die_at(path, pk->flag_checked[i], "flag '%s' is checked here, but no outcome sets it", pk->flags[i]);

    for (int m = 0; m < pk->nmeters; m++)
        if (!pk->meter_endings[m])
            die_at(path, lineno, "meter '%s' has no 'ending:' (what happens when it hits zero)", pk->meter_names[m]);

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
void load_packs(const char *dir) {
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

