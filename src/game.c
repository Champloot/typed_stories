// game: playing a run: input loop, outcomes, summary, menu
#include <ncurses.h>

#include "game.h"
#include "day.h"
#include "match.h"
#include "pack.h"
#include "style.h"
#include "ui.h"
#include "util.h"

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
// The outcome's flags are added to *flags.
static int play_event(const Event *e, int idx, int total, Stats *st, uint64_t *flags) {
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
            apply_sets(flags, &v->timeout_sets);
            if (fate) apply_sets(flags, &fate->sets);
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
        apply_sets(flags, &c->sets);
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

int choose_pack(void) {
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

// The day is built phase by phase, and each event picks its variant only when
// it comes up, so flags set earlier change what happens later.
void run_pack(const Pack *p) {
    int *order = xrealloc(NULL, (size_t)p->count * sizeof *order);
    uint64_t flags = 0;
    Stats st = {0};
    int played = 0;
    style_new_run();

    int nph = p->nphases ? p->nphases : 1;
    for (int ph = 0; ph < nph; ph++) {
        int n = p->nphases ? build_phase(p, ph, flags, order) : order_by_slots(p, order);
        for (int k = 0; k < n; k++) {
            Event e;
            e.s = &p->scen[order[k]];
            e.phase = p->nphases ? p->phases[ph].name : NULL;
            if (e.s->has_needs && !cond_holds(&e.s->needs, flags)) continue;
            e.v = pick_variant(e.s, flags);
            if (!e.v) continue;     // every variant needs flags that are not set
            int total = played + (n - k) + (p->nphases ? planned_after(p, ph + 1, flags) : 0);
            int lo, hi;
            mark_range(e.v, &lo, &hi);
            st.best += hi;
            st.worst += lo;
            st.marks += play_event(&e, played, total, &st, &flags);
            played++;
        }
    }
    free(order);
    style_save();
    show_summary(p, &st, played);
}

