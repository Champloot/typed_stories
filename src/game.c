// game: playing a run: input loop, outcomes, meters, summary, menu
#include <ctype.h>
#include <ncurses.h>
#include <time.h>
#include <unistd.h>

#include "game.h"
#include "daily.h"
#include "day.h"
#include "match.h"
#include "pack.h"
#include "records.h"
#include "rng.h"
#include "style.h"
#include "ui.h"
#include "util.h"

typedef struct {
    int    marks, best, worst;  // sum of marks, and the best/worst possible for this run
    int    typed, erased, sent; // printable keys, keys erased, characters in accepted answers
    double typing_time;
    int    timeouts, misses;    // misses: answers nobody understood
    int    style;               // style points, the word of the day bonus included
} Stats;

/* Everything one run (one day) keeps track of. */
typedef struct {
    const Pack *p;
    Stats       st;
    uint64_t    flags;
    uint64_t    seed;
    int         meters[MAX_METERS];
    int         ended;          // the meter that hit zero and ended the day, -1 if none
    const char *word;           // word of the day, NULL without words.lst
    int         word_used;      // its bonus was given (once per run)
    Date        date;
} Run;

static int today_mode;          // menu switch: the same day for everyone on a date

static int is_backspace(int ch) {
    return ch == KEY_BACKSPACE || ch == 127 || ch == 8;
}

static void upcase(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    for (; src[i] && i + 1 < cap; i++) dst[i] = (char)toupper((unsigned char)src[i]);
    dst[i] = '\0';
}

// Row 0 of every screen: "Nerves 5  Money 3!  People 6    word: hedgehog".
static void update_status(const Run *run) {
    char s[256];
    size_t n = 0;
    s[0] = '\0';
    for (int i = 0; i < run->p->nmeters && n < sizeof s; i++)
        n += (size_t)snprintf(s + n, sizeof s - n, "%s%s %d%s", i ? "  " : "",
                              run->p->meter_names[i], run->meters[i], run->meters[i] <= 2 ? "!" : "");
    if (run->word && n < sizeof s)
        snprintf(s + n, sizeof s - n, "%sword: %s%s", n ? "    " : "", run->word, run->word_used ? " (used)" : "");
    ui_set_status(s);
}

// Applies an outcome's meter changes and appends "nerves -1  money +2" to note.
static void apply_effects(Run *run, const Effects *fx, char *note, size_t cap) {
    for (int i = 0; i < fx->n; i++) {
        int m = fx->meter[i], v = run->meters[m] + fx->delta[i];
        if (v < 0) v = 0;
        if (v > METER_MAX) v = METER_MAX;
        run->meters[m] = v;
        size_t n = strlen(note);
        snprintf(note + n, cap - n, "%s%s %+d", n ? "   " : "", run->p->meter_names[m], fx->delta[i]);
    }
    if (run->ended < 0)
        for (int m = 0; m < run->p->nmeters; m++)
            if (run->meters[m] == 0) { run->ended = m; break; }
}

// Draws the meter changes of an outcome; returns the rows used.
static int draw_effects(int row, const char *fxnote) {
    if (!*fxnote) return 0;
    attron(COLOR_PAIR(C_WARN));
    int n = print_wrapped(row, 4, fxnote);
    attroff(COLOR_PAIR(C_WARN));
    return n;
}

// Plays one event and returns its mark. The mark is never shown: the player
// has to read it from the reaction. Style points and meter changes are shown.
// The outcome's flags and meter changes are applied to the run.
static int play_event(Run *run, Event *e, int idx, int total) {
    const Scenario *s = e->s;
    const Variant  *v = e->v;
    Stats *st = &run->st;
    char input[INPUT_MAX + 1] = "", msg[200] = "", fxnote[200] = "";
    int len = 0;

    show_intro(e, idx, total);
    double start = now();

    while (1) {
        double left = s->time_limit - (now() - start);
        if (left <= 0) {
            st->typing_time += s->time_limit;
            st->timeouts++;
            const Choice *fate = NULL;
            if (v->on_timeout == TO_RANDOM) fate = &v->choices[rng_below(&e->fate, v->nchoices)];
            apply_sets(&run->flags, &v->timeout_sets);
            apply_effects(run, &v->timeout_fx, fxnote, sizeof fxnote);
            if (fate) {
                apply_sets(&run->flags, &fate->sets);
                apply_effects(run, &fate->fx, fxnote, sizeof fxnote);
            }
            update_status(run);

            erase();
            draw_header(idx, total, e->phase);
            int r = draw_situation(s, v, 0) + 1;
            attron(COLOR_PAIR(C_BAD));
            mvprintw(r, 4, "TIME'S UP");
            attroff(COLOR_PAIR(C_BAD));
            r += 2;
            r += print_wrapped(r, 4, v->timeout_text);
            if (fate) r += print_wrapped(r, 4, fate->result);
            if (*fxnote) r += 1 + draw_effects(r + 1, fxnote);
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
        Match m = match_answer(v, norm, run->word);
        if (m.choice < 0) {
            if (m.only_word) snprintf(msg, sizeof msg, "Nice word. But what do you answer?");
            else snprintf(msg, sizeof msg, "Huh? Nobody gets '%s'. Try again.", input);
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
        int bonus = 0;
        if (m.word && !run->word_used) {
            bonus = WORD_BONUS;
            run->word_used = 1;
            record_word_day(run->date.iso);
        }
        st->style += pts + bonus;
        apply_sets(&run->flags, &c->sets);
        apply_effects(run, &c->fx, fxnote, sizeof fxnote);
        update_status(run);

        erase();
        draw_header(idx, total, e->phase);
        int r = draw_situation(s, v, 0) + 1;
        attron(COLOR_PAIR(C_OK));
        mvprintw(r, 4, "\"%s\"", input);
        attroff(COLOR_PAIR(C_OK));
        r += 2;
        r += print_wrapped(r, 4, c->result);
        if (*fxnote) r += 1 + draw_effects(r + 1, fxnote);
        char note[240];
        snprintf(note, sizeof note, "style %c +%d%s%s%s   %.1fs", style_rank(pts), pts,
                 m.typo ? "  (typo)" : "", before >= 0.5 ? "  (heard that before)" : "",
                 bonus ? "  WORD OF THE DAY +100" : m.word ? "  (word already used today)" : "", elapsed);
        attron(COLOR_PAIR(C_DIM));
        int nl = print_wrapped(r + 1, 4, note);
        attroff(COLOR_PAIR(C_DIM));
        mvprintw(r + 1 + nl + 1, 4, "(press any key)");
        refresh();
        wait_key();
        return c->mark;
    }
}

static void show_day_start(const Run *run) {
    char line[320], up[64];
    erase();
    ui_draw_status();
    int r = 2;
    attron(A_BOLD);
    r += print_wrapped(r, 4, run->p->name) + 1;
    attroff(A_BOLD);
    if (today_mode) snprintf(line, sizeof line, "Today, %s. Everyone gets the same day.", run->date.pretty);
    else snprintf(line, sizeof line, "Free play: a random day.");
    r += print_wrapped(r, 4, line) + 1;
    if (run->word) {
        upcase(up, sizeof up, run->word);
        snprintf(line, sizeof line, "Word of the day: %s", up);
        attron(A_BOLD);
        r += print_wrapped(r, 4, line);
        attroff(A_BOLD);
        r += print_wrapped(r, 4, "Slip it into any answer today for +100 style. It does not change your rating.") + 1;
    }
    if (run->p->nmeters)
        r += print_wrapped(r, 4, "Keep the meters at the top above zero, or the day ends early.") + 1;
    const Result *b = record_best(run->p->name);
    const Result *d = today_mode ? record_day(run->date.iso, run->p->name) : NULL;
    if (b) {
        if (d) snprintf(line, sizeof line, "Your best: %d. Today's best: %d.", b->score, d->score);
        else snprintf(line, sizeof line, "Your best: %d.", b->score);
        r += print_wrapped(r, 4, line) + 1;
    }
    attron(COLOR_PAIR(C_DIM));
    print_wrapped(r, 4, "Press Enter to start.");
    attroff(COLOR_PAIR(C_DIM));
    refresh();
    wait_enter();
}

static void show_ending(const Run *run) {
    char line[200], up[64];
    erase();
    ui_draw_status();
    upcase(up, sizeof up, run->p->meter_names[run->ended]);
    snprintf(line, sizeof line, "%s HIT ZERO. THE DAY IS OVER.", up);
    attron(COLOR_PAIR(C_BAD) | A_BOLD);
    int r = 3 + print_wrapped(3, 4, line) + 1;
    attroff(COLOR_PAIR(C_BAD) | A_BOLD);
    r += print_wrapped(r, 4, run->p->meter_endings[run->ended]);
    mvprintw(r + 1, 4, "(press any key)");
    refresh();
    wait_key();
}

// Rating scales the run's marks between the worst and the best possible run,
// in integer math so the boundaries are exact. Score adds style and what is
// left of the meters, and goes into the records.
static void show_summary(const Run *run, int events) {
    const Pack *p = run->p;
    const Stats *st = &run->st;
    int span = st->best - st->worst, got = st->marks - st->worst;
    int rating = span > 0 ? (got * 20 + span) / (2 * span) : 10;
    int vi = span > 0 ? got * p->nverdicts / span : p->nverdicts - 1;
    if (vi >= p->nverdicts) vi = p->nverdicts - 1;
    if (vi < 0) vi = 0;
    double acc = st->typed ? 100.0 * (st->typed - st->erased) / st->typed : 100.0;
    double wpm = st->typing_time > 0 ? (st->sent / 5.0) / (st->typing_time / 60.0) : 0;
    int avg = events ? (st->style + events / 2) / events : 0;
    int meters = 0;
    for (int m = 0; m < p->nmeters; m++) meters += run->meters[m];
    int score = rating * 100 + st->style + meters * 10;

    const Result *pb = record_best(p->name);
    int prev_best = pb ? pb->score : -1;
    Result res = { score, rating, avg };
    int new_best, new_day;
    record_result(run->date.iso, p->name, res, today_mode, &new_best, &new_day);
    records_save();

    erase();
    ui_draw_status();
    char line[320];
    snprintf(line, sizeof line, "%s: %s", run->ended >= 0 ? "DAY OVER EARLY" : "RUN COMPLETE", p->name);
    int r = 2 + print_wrapped(2, 4, line);
    attron(A_BOLD);
    mvprintw(r + 1, 4, "Rating: %d/10", rating);
    attroff(A_BOLD);
    r += 3;
    r += print_wrapped(r, 4, p->verdicts[vi]);
    r += 1;
    mvprintw(r++, 4, "Style:    %c (avg %d per answer)", style_rank(avg), avg);
    if (run->word)
        mvprintw(r++, 4, "Word:     %s, %s", run->word, run->word_used ? "used (+100)" : "not used");
    if (p->nmeters) {
        size_t n = (size_t)snprintf(line, sizeof line, "Meters:  ");
        for (int m = 0; m < p->nmeters && n < sizeof line; m++)
            n += (size_t)snprintf(line + n, sizeof line - n, " %s %d", p->meter_names[m], run->meters[m]);
        r += print_wrapped(r, 4, line);
    }
    mvprintw(r++, 4, "Typing:   %.0f%% accuracy, %.0f wpm", acc, wpm);
    mvprintw(r++, 4, "Timeouts: %d, not understood: %d", st->timeouts, st->misses);
    r++;
    attron(A_BOLD);
    if (new_best && prev_best >= 0) mvprintw(r++, 4, "Score:    %d  NEW RECORD! (was %d)", score, prev_best);
    else if (new_best)              mvprintw(r++, 4, "Score:    %d  (first record)", score);
    else                            mvprintw(r++, 4, "Score:    %d  (best %d)", score, prev_best);
    attroff(A_BOLD);
    if (today_mode) {
        const Result *d = record_day(run->date.iso, p->name);
        mvprintw(r++, 4, "Today:    best %d%s", d ? d->score : score, new_day ? " (that's you)" : "");
    }
    mvprintw(r + 1, 4, "(press any key)");
    refresh();
    wait_key();
}

int choose_pack(void) {
    timeout(-1);
    while (1) {
        Date d = today();
        const char *w = word_of_day(&d);
        erase();
        int r = 2 + print_wrapped(2, 4, "TYPING GAME - choose who you are today") + 1;
        for (int i = 0; i < npacks; i++) {
            mvprintw(r + i, 4, "%d) %s", i + 1, packs[i].name);
            const Result *b = record_best(packs[i].name);
            const Result *t = record_day(d.iso, packs[i].name);
            char rec[80] = "";
            if (b) snprintf(rec, sizeof rec, "best %d", b->score);
            if (t) snprintf(rec + strlen(rec), sizeof rec - strlen(rec), "%stoday %d", *rec ? "  " : "", t->score);
            if (*rec) {
                attron(COLOR_PAIR(C_DIM));
                printw("   %s", rec);
                attroff(COLOR_PAIR(C_DIM));
            }
        }
        r += npacks + 1;
        mvprintw(r, 4, "t) mode: %s", today_mode ? "today" : "free play");
        attron(COLOR_PAIR(C_DIM));
        printw(today_mode ? "  (the same day for everyone, %s)" : "  (a random day)", d.pretty);
        attroff(COLOR_PAIR(C_DIM));
        mvprintw(r + 1, 4, "q) quit");
        if (w) mvprintw(r + 3, 4, "Word of the day: %s", w);
        refresh();
        int ch = getch();
        if (ch == 'q') { timeout(16); return -1; }
        if (ch == 't') { today_mode = !today_mode; continue; }
        if (ch >= '1' && ch < '1' + npacks) { timeout(16); return ch - '1'; }
    }
}

static uint64_t fresh_seed(void) {
    return rng_mix((uint64_t)time(NULL) ^ rng_mix((uint64_t)(now() * 1e6)) ^ ((uint64_t)getpid() << 40));
}

// The day is built phase by phase, and each event picks its variant only when
// it comes up, so flags set earlier change what happens later. A meter at zero
// ends the day on the spot.
void run_pack(const Pack *p) {
    Run run;
    memset(&run, 0, sizeof run);
    run.p = p;
    run.ended = -1;
    run.date = today();
    run.word = word_of_day(&run.date);
    run.seed = today_mode ? rng_mix(rng_hash(run.date.iso) ^ rng_mix(rng_hash(p->name))) : fresh_seed();
    for (int m = 0; m < p->nmeters; m++) run.meters[m] = p->meter_start[m];
    int f = pack_flag(p, FLAG_WORDSMITH);
    if (f >= 0 && word_days() >= WORDSMITH_DAYS) run.flags |= (uint64_t)1 << f;
    f = pack_flag(p, FLAG_TODAY);
    if (f >= 0 && today_mode) run.flags |= (uint64_t)1 << f;

    style_new_run();
    update_status(&run);
    show_day_start(&run);

    int *order = xrealloc(NULL, (size_t)p->count * sizeof *order);
    int played = 0;
    int nph = p->nphases ? p->nphases : 1;
    for (int ph = 0; ph < nph && run.ended < 0; ph++) {
        int n = p->nphases ? build_phase(p, ph, run.flags, order, run.seed) : order_by_slots(p, order, run.seed);
        for (int k = 0; k < n && run.ended < 0; k++) {
            Event e;
            e.s = &p->scen[order[k]];
            e.phase = p->nphases ? p->phases[ph].name : NULL;
            if (e.s->has_needs && !cond_holds(&e.s->needs, run.flags)) continue;
            Rng vr = rng_for(run.seed, RNG_VARIANT, (unsigned)order[k]);
            e.v = pick_variant(e.s, run.flags, &vr);
            if (!e.v) continue;     // every variant needs flags that are not set
            e.fate = rng_for(run.seed, RNG_FATE, (unsigned)order[k]);
            int total = played + (n - k) + (p->nphases ? planned_after(p, ph + 1, run.flags) : 0);
            int lo, hi;
            mark_range(e.v, &lo, &hi);
            run.st.best += hi;
            run.st.worst += lo;
            run.st.marks += play_event(&run, &e, played, total);
            played++;
        }
    }
    free(order);
    if (run.ended >= 0) show_ending(&run);
    style_save();
    show_summary(&run, played);
    ui_set_status("");
}
