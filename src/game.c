// game: playing a run: input loop, outcomes, meters, summary, menu.
// Knows nothing about the screen: it fills views from ui.h and reads keys.
#include <time.h>

#include "config.h"
#include "game.h"
#include "activity.h"
#include "minigame.h"
#include "progress.h"
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
    v.limit = s->play_time * cfg.walk_scale;
    ui_flush();
    int last_dx = 0;
    while (1) {
        double t = now();
        double left = v.limit - (t - start);
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

// A minigame after the answer ('act:'). Returns 1 if it was won.
static int play_minigame(const Event *e, const Choice *c, const Scene *scene) {
    Rng r = e->fate;
    Minigame g;
    double t0 = now();
    mg_start(&g, c->act - 1, c->act_time * cfg.game_scale, c->act_n, c->act_text, &r, t0);
    MinigameView v;
    memset(&v, 0, sizeof v);
    v.scene = *scene;
    v.kind = g.kind;
    v.text = g.text;
    v.n = g.n;
    v.limit = g.limit;
    v.zone_lo = g.zone_lo;
    v.zone_hi = g.zone_hi;
    ui_flush();
    ui_presses();
    int last_dx = 0;
    while (1) {
        double t = now();
        mg_tick(&g, t);
        v.state = g.state;
        v.progress = mg_progress(&g, t);
        v.needle = mg_needle(&g, t);
        v.tries = g.tries;
        v.typed = g.typed;
        v.missed_ago = t - g.missed_at;
        v.wrong_ago = t - g.wrong_at;
        v.ended_ago = g.state == MG_GOING ? 0 : t - g.ended_at;
        v.moved_ago = t - g.moved_at;
        v.w = g.w; v.h = g.h; v.px = g.px; v.py = g.py; v.ppx = g.ppx; v.ppy = g.ppy;
        for (int y = 0; y < g.h; y++) {
            for (int x = 0; x < g.w; x++) v.cells[y][x] = mg_cell(&g, x, y);
            v.cells[y][g.w] = '\0';
            v.lane_dir[y] = g.kind == MG_CROSS ? g.lanes[y].dir : 0;
            v.lane_step[y] = g.lanes[y].step;
            v.lane_next[y] = g.lanes[y].next - t;
        }
        v.caught = g.caught;
        v.missed = g.missed;
        double left = g.limit - (t - g.start);
        v.left = g.state != MG_GOING ? g.limit - (g.ended_at - g.start) : left > 0 ? left : 0;
        ui_minigame(&v);
        if (g.state != MG_GOING && t - g.ended_at > 0.9) return g.state == MG_WON;

        int ch = ui_key(16);
        t = now();
        if (g.state != MG_GOING || ch == K_NONE) {
            if (ch == K_NONE && (g.kind == MG_CROSS || g.kind == MG_CATCH)) goto held;
            continue;
        }
        if (g.kind == MG_MASH) {                // a held key does not count, only presses
            int n = ui_presses();
            if (n < 0) mg_press(&g, ' ', t);
            else while (n-- > 0) mg_press(&g, ' ', t);
            continue;
        }
        if (g.kind == MG_TIMING) { if (ch == ' ' || ch == K_ENTER) mg_press(&g, ' ', t); continue; }
        if (g.kind == MG_LYRICS) { if (ch >= 32 && ch <= 126) mg_press(&g, ch, t); continue; }
        {
            int dx = 0, dy = 0;
            switch (ch) {
            case 'w': case 'W': case K_UP:    dy = -1; break;
            case 's': case 'S': case K_DOWN:  dy = 1;  break;
            case 'a': case 'A': case K_LEFT:  dx = -1; break;
            case 'd': case 'D': case K_RIGHT: dx = 1;  break;
            default: break;
            }
            if (ui_held() < 0) { if (dx || dy) mg_move(&g, dx, dy, t); continue; }
            if ((dx || dy) && t >= g.next_step) { mg_move(&g, dx, dy, t); last_dx = dx != 0; continue; }
        }
    held: {
            int held = ui_held();
            if (held <= 0 || t < g.next_step) continue;
            int hx = !!(held & DIR_RIGHT) - !!(held & DIR_LEFT);
            int hy = !!(held & DIR_DOWN) - !!(held & DIR_UP);
            if (hx && hy) { if (last_dx) hx = 0; else hy = 0; }
            if (hx || hy) { mg_move(&g, hx, hy, t); last_dx = hx != 0; }
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
    pv.limit = s->time_limit * cfg.answer_scale;

    ui_intro(&out.scene);
    wait_enter();
    double start = now();

    while (1) {
        double left = pv.limit - (now() - start);
        if (left <= 0) {
            st->typing_time += pv.limit;
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
            bonus = cfg.word_bonus;
            run->word_used = 1;
            record_word_day(run->date.iso);
        }
        st->style += pts + bonus;

        // a minigame after the answer: lose it, and the choice's 'fail:' happens
        if (c->act && cfg.minigames && !play_minigame(e, c, &pv.scene)) {
            int has = c->has_fail;
            apply_sets(&run->flags, has ? &c->fail_sets : &v->timeout_sets);
            apply_effects(run, has ? &c->fail_fx : &v->timeout_fx, &out.fx);
            update_hud(run);
            out.typed = input;
            out.text = has ? c->fail_text : v->timeout_text;
            out.scene.show = has ? c->fail_show : v->timeout_show;
            out.rank = style_rank(pts);
            out.points = pts;
            out.typo = m.typo;
            out.repeated = before >= 0.5;
            out.word_bonus = bonus != 0;
            out.word_again = m.word && !bonus;
            out.seconds = elapsed;
            ui_outcome(&out);
            wait_key();
            return has ? c->fail_mark : v->timeout_mark;
        }

        // an activity scene: the answer only says where to go; now get there
        if (s->play >= 0 && cfg.walks && !walk_to(e, c, input, &pv.scene)) {
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
// Shows the story's ending; returns its title (NULL if the pack has none).
static const char *show_finale(const Run *run) {
    const Pack *p = run->p;
    if (!p->nfinales) return NULL;
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
    return v.title;
}

// A medal is won at the end of a day lived through, when every part of it holds.
static int medal_won(const Run *run, const Medal *md, const char *finale, int rating) {
    const Pack *p = run->p;
    if (run->ended >= 0) return 0;
    for (int m = 0; m < p->nmeters; m++) {
        if (md->meters_cmp > 0 && run->meters[m] < md->meters_val) return 0;
        if (md->meters_cmp < 0 && run->meters[m] > md->meters_val) return 0;
    }
    if (md->finale && (!finale || strcmp(md->finale, finale))) return 0;
    if (md->has_flags && !cond_holds(&md->flags, run->flags)) return 0;
    if (md->rating_min >= 0 && rating < md->rating_min) return 0;
    return 1;
}

// Rating scales the run's marks between the worst and the best possible run,
// in integer math so the boundaries are exact. Score adds style and what is
// left of the meters, and goes into the records.
static void show_summary(const Run *run, int events, const char *finale) {
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

    // a level: passed, and which medals now
    if (run->ended < 0) progress_set_done(p->name);
    v.nmedals = p->nmedals;
    for (int i = 0; i < p->nmedals; i++) {
        v.medal[i] = p->medals[i].name;
        v.medal_before[i] = progress_medal(p->name, p->medals[i].name);
        v.medal_now[i] = medal_won(run, &p->medals[i], finale, v.rating);
        if (v.medal_now[i]) progress_add_medal(p->name, p->medals[i].name);
    }
    progress_save();
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
        v.all_done = npacks > 0;
        for (int i = 0; i < npacks; i++) {
            // levels open one after another; config.ini or TYPED_STORIES_UNLOCK opens them all
            v.packs[i].locked = i > 0 && !progress_done(packs[i - 1].name) && !cfg.unlock_all;
            v.packs[i].done = progress_done(packs[i].name);
            v.all_done &= v.packs[i].done;
            v.packs[i].nmedals = packs[i].nmedals;
            for (int m = 0; m < packs[i].nmedals; m++) {
                v.packs[i].medal[m] = packs[i].medals[m].name;
                v.packs[i].got[m] = progress_medal(packs[i].name, packs[i].medals[m].name);
            }
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
        if (ch >= '1' && ch < '1' + npacks && !v.packs[ch - '1'].locked) return ch - '1';
    }
}

// Free play: something different every time (the clock, and where the stack happens to be).
static uint64_t fresh_seed(void) {
    int here;
    return rng_mix((uint64_t)time(NULL) ^ rng_mix((uint64_t)(now() * 1e6)) ^ rng_mix((uint64_t)(uintptr_t)&here));
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
    for (int m = 0; m < p->nmeters; m++) run.meters[m] = cfg.meter_start ? cfg.meter_start : p->meter_start[m];
    int f = pack_flag(p, FLAG_WORDSMITH);
    if (f >= 0 && word_days() >= cfg.wordsmith_days) run.flags |= (uint64_t)1 << f;
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
            int left = n - k;
            if (p->fixed) {         // a level: only the scenes still open count
                left = 0;
                for (int j = k; j < n; j++) left += scenario_open(&p->scen[order[j]], run.flags);
            }
            int total = played + left + (p->nphases ? planned_after(p, ph + 1, run.flags) : 0);
            int lo, hi;
            mark_range(e.v, &lo, &hi);
            run.st.best += hi;
            run.st.worst += lo;
            run.st.marks += play_event(&run, &e, played, total);
            played++;
        }
    }
    free(order);
    const char *finale = NULL;
    if (run.ended >= 0) show_ending(&run);
    else finale = show_finale(&run);
    style_save();
    show_summary(&run, played, finale);
    ui_set_hud(NULL);
    ui_set_theme(NULL);
}
