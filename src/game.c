// game: playing a run: input loop, outcomes, meters, summary, menu.
// Knows nothing about the screen: it fills views from ui.h and reads keys.
#include <time.h>
#include <unistd.h>

#include "game.h"
#include "activity.h"
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

static void wait_key(void) {
    ui_flush();                 // drop keys typed ahead so the screen is not skipped
    while (ui_key(-1) == K_NONE) {}
}

static void wait_enter(void) {
    ui_flush();
    while (ui_key(-1) != K_ENTER) {}
}

static Hud hud_of(const Run *run) {
    Hud h;
    memset(&h, 0, sizeof h);
    h.nmeters = run->p->nmeters;
    for (int i = 0; i < h.nmeters; i++) {
        h.meter_names[i] = run->p->meter_names[i];
        h.meters[i] = run->meters[i];
    }
    h.word = run->word;
    h.word_used = run->word_used;
    return h;
}

static void update_hud(const Run *run) {
    Hud h = hud_of(run);
    ui_set_hud(&h);
}

// Applies an outcome's meter changes and lists them in fx.
static void apply_effects(Run *run, const Effects *e, EffectsView *fx) {
    for (int i = 0; i < e->n; i++) {
        int m = e->meter[i], v = run->meters[m] + e->delta[i];
        if (v < 0) v = 0;
        if (v > METER_MAX) v = METER_MAX;
        run->meters[m] = v;
        if (fx->n < 2 * MAX_EFFECTS) {
            fx->name[fx->n] = run->p->meter_names[m];
            fx->delta[fx->n] = e->delta[i];
            fx->n++;
        }
    }
    if (run->ended < 0)
        for (int m = 0; m < run->p->nmeters; m++)
            if (run->meters[m] == 0) { run->ended = m; break; }
}

// Plays one event and returns its mark. The mark is never shown: the player
// has to read it from the reaction. Style points and meter changes are shown.
// The outcome's flags and meter changes are applied to the run.
// After the answer of an activity scene: walk to the spot it named before the
// time runs out. Returns 1 if the player got there.
static int walk_to(const Event *e, const Choice *c, const char *answer, const Scene *scene) {
    const Scenario *s = e->s;
    Rng r = e->fate;
    Walk w;
    double start = now();
    walk_start(&w, s->play, c->goal, &r, start);

    char goal_name[32];
    if (c->goal >= '1' && c->goal <= '9') snprintf(goal_name, sizeof goal_name, "aisle %c", c->goal);
    else if (c->goal == 'k') snprintf(goal_name, sizeof goal_name, "the checkout");
    else snprintf(goal_name, sizeof goal_name, "the spot");

    WalkView v;
    memset(&v, 0, sizeof v);
    v.scene = *scene;
    v.answer = answer;
    v.goal_name = goal_name;
    v.goal = c->goal;
    v.w = w.w;
    v.h = w.h;
    v.rows = w.rows;
    v.limit = s->play_time;
    ui_flush();
    int last_dx = 0;
    while (1) {
        double t = now();
        double left = s->play_time - (t - start);
        walk_tick(&w, t);
        v.px = w.px; v.py = w.py; v.ppx = w.ppx; v.ppy = w.ppy; v.moved_at = w.moved_at;
        v.ncarts = w.ncarts;
        for (int i = 0; i < w.ncarts && i < 4; i++) {
            v.cart_x[i] = w.carts[i].x;
            v.cart_y[i] = w.carts[i].y;
            v.cart_dx[i] = w.carts[i].dx;
            v.cart_px[i] = w.carts[i].px;
            v.cart_moved[i] = w.carts[i].moved_at;
        }
        v.bumped = t - w.bumped_at < 0.5;
        v.left = left > 0 ? left : 0;
        ui_walk(&v);
        if (walk_arrived(&w)) return 1;
        if (left <= 0) return 0;

        int ch = ui_key(16);
        int dx = 0, dy = 0;
        switch (ch) {
        case 'w': case 'W': case K_UP:    dy = -1; break;
        case 's': case 'S': case K_DOWN:  dy = 1;  break;
        case 'a': case 'A': case K_LEFT:  dx = -1; break;
        case 'd': case 'D': case K_RIGHT: dx = 1;  break;
        default: break;
        }
        int held = ui_held();
        t = now();
        if (held < 0) {                         // every key press is one step
            if (dx || dy) walk_step(&w, dx, dy, t);
            continue;
        }
        if (!dx && !dy && held) {               // keys held down: a step every WALK_STEP_S
            int hx = !!(held & DIR_RIGHT) - !!(held & DIR_LEFT);
            int hy = !!(held & DIR_DOWN) - !!(held & DIR_UP);
            if (hx && hy) { if (last_dx) hx = 0; else hy = 0; }   // diagonals: take turns
            dx = hx;
            dy = hy;
        }
        if ((dx || dy) && t >= w.next_step) {
            walk_step(&w, dx, dy, t);
            last_dx = dx != 0;
        }
    }
}

static int play_event(Run *run, Event *e, int idx, int total) {
    const Scenario *s = e->s;
    const Variant  *v = e->v;
    Stats *st = &run->st;
    char input[INPUT_MAX + 1] = "", msg[200] = "";
    int len = 0;

    OutcomeView out;
    memset(&out, 0, sizeof out);
    out.scene.idx = idx;
    out.scene.total = total;
    out.scene.phase = e->phase;
    out.scene.title = s->title;
    out.scene.detail = v->detail;
    out.scene.stage = s->stage;
    out.scene.props = v->props;

    PromptView pv;
    memset(&pv, 0, sizeof pv);
    pv.scene = out.scene;
    pv.nhints = v->nchoices;
    for (int c = 0; c < v->nchoices; c++) pv.hints[c] = v->choices[c].label;
    pv.input = input;
    pv.msg = msg;
    pv.limit = s->time_limit;

    ui_intro(&out.scene);
    wait_enter();
    double start = now();

    while (1) {
        double left = s->time_limit - (now() - start);
        if (left <= 0) {
            st->typing_time += s->time_limit;
            st->timeouts++;
            const Choice *fate = NULL;
            if (v->on_timeout == TO_RANDOM) fate = &v->choices[rng_below(&e->fate, v->nchoices)];
            apply_sets(&run->flags, &v->timeout_sets);
            apply_effects(run, &v->timeout_fx, &out.fx);
            if (fate) {
                apply_sets(&run->flags, &fate->sets);
                apply_effects(run, &fate->fx, &out.fx);
            }
            update_hud(run);
            out.typed = NULL;
            out.text = v->timeout_text;
            out.fate = fate ? fate->result : NULL;
            out.scene.show = fate && fate->show ? fate->show : v->timeout_show;
            ui_outcome(&out);
            wait_key();
            return fate ? fate->mark : v->timeout_mark;
        }

        pv.left = left;
        ui_prompt(&pv);

        int ch = ui_key(16);
        if (ch == K_NONE || ch == K_OTHER) continue;

        if (ch == K_BACKSPACE) {
            if (len) { input[--len] = '\0'; st->erased++; }
            continue;
        }
        if (ch == K_CTRL_W) {                   // erase the last word
            while (len && input[len - 1] == ' ') { input[--len] = '\0'; st->erased++; }
            while (len && input[len - 1] != ' ') { input[--len] = '\0'; st->erased++; }
            continue;
        }
        if (ch == K_CTRL_U) {                   // erase everything
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
        if (ch != K_ENTER) continue;

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

        // an activity scene: the answer only says where to go; now get there
        if (s->play >= 0 && !walk_to(e, c, input, &pv.scene)) {
            st->timeouts++;
            apply_sets(&run->flags, &v->timeout_sets);
            apply_effects(run, &v->timeout_fx, &out.fx);
            update_hud(run);
            out.typed = input;
            out.text = v->timeout_text;
            out.scene.show = v->timeout_show;
            out.rank = style_rank(pts);
            out.points = pts;
            out.typo = m.typo;
            out.repeated = before >= 0.5;
            out.word_bonus = bonus != 0;
            out.word_again = m.word && !bonus;
            out.seconds = elapsed;
            ui_outcome(&out);
            wait_key();
            return v->timeout_mark;
        }

        apply_sets(&run->flags, &c->sets);
        apply_effects(run, &c->fx, &out.fx);
        update_hud(run);

        out.typed = input;
        out.scene.show = c->show;
        out.text = c->result;
        out.fate = NULL;
        out.rank = style_rank(pts);
        out.points = pts;
        out.typo = m.typo;
        out.repeated = before >= 0.5;
        out.word_bonus = bonus != 0;
        out.word_again = m.word && !bonus;
        out.seconds = elapsed;
        ui_outcome(&out);
        wait_key();
        return c->mark;
    }
}

static void show_day_start(const Run *run) {
    DayStartView v;
    memset(&v, 0, sizeof v);
    v.pack = run->p->name;
    v.today_mode = today_mode;
    v.date = run->date.pretty;
    v.word = run->word;
    v.has_meters = run->p->nmeters > 0;
    const Result *b = record_best(run->p->name);
    const Result *d = today_mode ? record_day(run->date.iso, run->p->name) : NULL;
    v.best = b ? b->score : -1;
    v.today = d ? d->score : -1;
    ui_day_start(&v);
    wait_enter();
}

static void show_ending(const Run *run) {
    EndingView v = { run->p->meter_names[run->ended], run->p->meter_endings[run->ended] };
    ui_ending(&v);
    wait_key();
}

// Where the run's marks fall between the worst and the best possible run,
// as one of n steps: 0 for the worst, n-1 for the best.
static int marks_step(const Run *run, int n) {
    const Stats *st = &run->st;
    int span = st->best - st->worst, got = st->marks - st->worst;
    int i = span > 0 ? got * n / span : n - 1;
    if (i >= n) i = n - 1;
    if (i < 0) i = 0;
    return i;
}

// The first special finale whose flags hold, otherwise the one for the rating.
static void show_finale(const Run *run) {
    const Pack *p = run->p;
    if (!p->nfinales) return;
    FinaleView v;
    memset(&v, 0, sizeof v);
    v.good = marks_step(run, 3);
    for (int i = 0; i < p->nfinale_if && !v.title; i++)
        if (cond_holds(&p->finale_if[i], run->flags)) {
            v.title = p->finale_if_title[i];
            v.text = p->finale_if_text[i];
            v.special = 1;
        }
    if (!v.title) {
        int i = marks_step(run, p->nfinales);
        v.title = p->finale_title[i];
        v.text = p->finale_text[i];
    }
    ui_finale(&v);
    wait_key();
}

// Rating scales the run's marks between the worst and the best possible run,
// in integer math so the boundaries are exact. Score adds style and what is
// left of the meters, and goes into the records.
static void show_summary(const Run *run, int events) {
    const Pack *p = run->p;
    const Stats *st = &run->st;
    SummaryView v;
    memset(&v, 0, sizeof v);
    int span = st->best - st->worst, got = st->marks - st->worst;
    v.rating = span > 0 ? (got * 20 + span) / (2 * span) : 10;
    v.verdict = p->verdicts[marks_step(run, p->nverdicts)];
    v.accuracy = st->typed ? 100.0 * (st->typed - st->erased) / st->typed : 100.0;
    v.wpm = st->typing_time > 0 ? (st->sent / 5.0) / (st->typing_time / 60.0) : 0;
    v.style_avg = events ? (st->style + events / 2) / events : 0;
    v.style_rank = style_rank(v.style_avg);
    int meters = 0;
    for (int m = 0; m < p->nmeters; m++) meters += run->meters[m];
    v.score = v.rating * 100 + st->style + meters * 10;

    const Result *pb = record_best(p->name);
    v.prev_best = pb ? pb->score : -1;
    Result res = { v.score, v.rating, v.style_avg };
    record_result(run->date.iso, p->name, res, today_mode, &v.new_best, &v.new_day);
    records_save();
    const Result *d = record_day(run->date.iso, p->name);

    v.pack = p->name;
    v.ended_early = run->ended >= 0;
    v.word = run->word;
    v.word_used = run->word_used;
    v.meters = hud_of(run);
    v.timeouts = st->timeouts;
    v.misses = st->misses;
    v.today_mode = today_mode;
    v.today_best = d ? d->score : v.score;
    ui_summary(&v);
    wait_key();
}

int choose_pack(void) {
    ui_set_hud(NULL);
    while (1) {
        Date d = today();
        MenuView v;
        memset(&v, 0, sizeof v);
        v.npacks = npacks;
        for (int i = 0; i < npacks; i++) {
            const Result *b = record_best(packs[i].name);
            const Result *t = record_day(d.iso, packs[i].name);
            v.packs[i].name = packs[i].name;
            v.packs[i].style = packs[i].style;
            v.packs[i].best = b ? b->score : -1;
            v.packs[i].today = t ? t->score : -1;
        }
        v.today_mode = today_mode;
        v.date = d.pretty;
        v.word = word_of_day(&d);
        ui_menu(&v);
        int ch = ui_key(-1);
        if (ch == 'q') return -1;
        if (ch == 't') { today_mode = !today_mode; continue; }
        if (ch >= '1' && ch < '1' + npacks) return ch - '1';
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
    ui_set_theme(p->style);
    update_hud(&run);
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
    else show_finale(&run);
    style_save();
    show_summary(&run, played);
    ui_set_hud(NULL);
    ui_set_theme(NULL);
}
