// ui_gl: the window version of ui.h, on raylib. Everything is laid out in
// pixels from one unit (u) that scales with the window height.
#include <ctype.h>
#include <math.h>

#include "raylib.h"
#include "ui.h"
#include "util.h"

/* ---------- look ---------- */

#define C_BG     (Color){  17,  19,  24, 255 }
#define C_PANEL  (Color){  28,  31,  39, 255 }
#define C_FAINT  (Color){  52,  57,  70, 255 }
#define C_DIM    (Color){ 128, 134, 148, 255 }
#define C_TEXT   (Color){ 232, 230, 227, 255 }
#define C_CUE    (Color){ 214, 204, 182, 255 }
#define C_OK     (Color){ 123, 211, 137, 255 }
#define C_BAD    (Color){ 255, 107, 107, 255 }
#define C_WARN   (Color){ 247, 201,  72, 255 }
#define C_ACCENT (Color){ 108, 178, 255, 255 }

#define FONT_PX  64             // glyphs are rasterized once at this size and scaled down
#define FADE_S   0.18           // fade-in of a new screen

static Font  font, bold;
static float u;                 // layout unit: 1/28 of the window height
static float L, W;              // left edge and width of the content column
static float alpha = 1;         // fade of the current screen

static Color col(Color c) {
    return ColorAlpha(c, alpha * (float)c.a / 255.0f);
}

static float spacing(float size) { return size * 0.02f; }

static float text_w(Font f, const char *s, float size) {
    return MeasureTextEx(f, s, size, spacing(size)).x;
}

static void text(Font f, const char *s, float x, float y, float size, Color c) {
    DrawTextEx(f, s, (Vector2){ x, y }, size, spacing(size), col(c));
}

// Word-wrapped text; returns the height it took. With draw = 0 it only measures.
static float wrapped(Font f, const char *s, float x, float y, float maxw, float size, Color c, int draw) {
    char line[1024] = "", cand[1024];
    float yy = y, lh = size * 1.32f;
    const char *p = s;
    while (*p) {
        while (*p == ' ') p++;
        const char *w = p;
        while (*p && *p != ' ') p++;
        int wl = (int)(p - w);
        if (!wl) break;
        snprintf(cand, sizeof cand, "%s%s%.*s", line, *line ? " " : "", wl, w);
        if (*line && text_w(f, cand, size) > maxw) {
            if (draw) text(f, line, x, yy, size, c);
            yy += lh;
            snprintf(line, sizeof line, "%.*s", wl, w);
        } else {
            memcpy(line, cand, strlen(cand) + 1);
        }
    }
    if (*line) {
        if (draw) text(f, line, x, yy, size, c);
        yy += lh;
    }
    return yy - y;
}

// A rounded label; returns its width.
static float chip(const char *s, float x, float y, float size, Color fg, Color line, Color fill) {
    float pad = size * 0.55f, w = text_w(font, s, size) + 2 * pad, h = size * 1.6f;
    Rectangle r = { x, y, w, h };
    if (fill.a) DrawRectangleRounded(r, 0.5f, 8, col(fill));
    if (line.a) DrawRectangleRoundedLinesEx(r, 0.5f, 8, 1.5f, col(line));
    text(font, s, x + pad, y + (h - size) / 2, size, fg);
    return w;
}

static void upcase(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    for (; src[i] && i + 1 < cap; i++) dst[i] = (char)toupper((unsigned char)src[i]);
    dst[i] = '\0';
}

/* ---------- what is on screen ---------- */

/* The game hands over views whose strings may change or go away after the
   call, but the window redraws every frame, so everything is copied here. */

enum { S_NONE, S_MENU, S_DAY, S_INTRO, S_PROMPT, S_OUTCOME, S_ENDING, S_SUMMARY };

static int    screen = S_NONE;
static double screen_since;     // for the fade-in
static char   arena[32768];
static size_t arena_used;
static char   hud_arena[1024];
static size_t hud_used;

static union {
    MenuView     menu;
    DayStartView day;
    Scene        intro;
    PromptView   prompt;
    OutcomeView  out;
    EndingView   ending;
    SummaryView  sum;
} view;

static Hud    hud;
static int    hud_on;
static char   last_msg[200];
static double msg_since = -10;  // when the current "Huh?" appeared: the input shakes

static const char *keep_in(char *buf, size_t cap, size_t *used, const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    if (*used + n > cap) return "";
    char *d = buf + *used;
    memcpy(d, s, n);
    *used += n;
    return d;
}

static const char *keep(const char *s) {
    return keep_in(arena, sizeof arena, &arena_used, s);
}

static void keep_scene(Scene *s) {
    s->phase = keep(s->phase);
    s->title = keep(s->title);
    s->detail = keep(s->detail);
}

// A different kind of screen, or the next situation, fades in.
static void set_screen(int kind, int idx) {
    static int last_idx = -1;
    if (kind != screen || idx != last_idx) screen_since = now();
    screen = kind;
    last_idx = idx;
    arena_used = 0;
}

void ui_set_hud(const Hud *h) {
    hud_on = h != NULL;
    if (!h) return;
    hud = *h;
    hud_used = 0;
    for (int i = 0; i < hud.nmeters; i++)
        hud.meter_names[i] = keep_in(hud_arena, sizeof hud_arena, &hud_used, hud.meter_names[i]);
    hud.word = keep_in(hud_arena, sizeof hud_arena, &hud_used, hud.word);
}

void ui_menu(const MenuView *v) {
    set_screen(S_MENU, -1);
    view.menu = *v;
    for (int i = 0; i < v->npacks; i++) view.menu.packs[i].name = keep(v->packs[i].name);
    view.menu.date = keep(v->date);
    view.menu.word = keep(v->word);
}

void ui_day_start(const DayStartView *v) {
    set_screen(S_DAY, -1);
    view.day = *v;
    view.day.pack = keep(v->pack);
    view.day.date = keep(v->date);
    view.day.word = keep(v->word);
}

void ui_intro(const Scene *s) {
    set_screen(S_INTRO, s->idx);
    view.intro = *s;
    keep_scene(&view.intro);
}

void ui_prompt(const PromptView *v) {
    set_screen(S_PROMPT, v->scene.idx);
    view.prompt = *v;
    keep_scene(&view.prompt.scene);
    for (int i = 0; i < v->nhints; i++) view.prompt.hints[i] = keep(v->hints[i]);
    view.prompt.input = keep(v->input);
    view.prompt.msg = keep(v->msg);
    if (strcmp(v->msg, last_msg)) {
        if (*v->msg) msg_since = now();
        snprintf(last_msg, sizeof last_msg, "%s", v->msg);
    }
}

void ui_outcome(const OutcomeView *v) {
    set_screen(S_OUTCOME, v->scene.idx);
    view.out = *v;
    keep_scene(&view.out.scene);
    view.out.typed = keep(v->typed);
    view.out.text = keep(v->text);
    view.out.fate = keep(v->fate);
    for (int i = 0; i < v->fx.n; i++) view.out.fx.name[i] = keep(v->fx.name[i]);
    last_msg[0] = '\0';
}

void ui_ending(const EndingView *v) {
    set_screen(S_ENDING, -1);
    view.ending.meter = keep(v->meter);
    view.ending.text = keep(v->text);
}

void ui_summary(const SummaryView *v) {
    set_screen(S_SUMMARY, -1);
    view.sum = *v;
    view.sum.pack = keep(v->pack);
    view.sum.verdict = keep(v->verdict);
    view.sum.word = keep(v->word);
    for (int i = 0; i < v->meters.nmeters; i++) view.sum.meters.meter_names[i] = keep(v->meters.meter_names[i]);
}

/* ---------- pieces ---------- */

static Color meter_color(int v) {
    return v <= 2 ? C_BAD : v <= 4 ? C_WARN : C_ACCENT;
}

// "Nerves ▮▮▮▮▮▯▯▯▯▯": name and ten segments; returns the width.
static float meter_bar(const char *name, int value, float x, float y, float size) {
    text(font, name, x, y, size, C_DIM);
    float bx = x + text_w(font, name, size) + size * 0.6f, seg = size * 0.55f, gap = size * 0.16f;
    for (int i = 0; i < METER_MAX; i++) {
        Rectangle r = { bx + i * (seg + gap), y + size * 0.18f, seg, size * 0.7f };
        DrawRectangleRounded(r, 0.3f, 4, col(i < value ? meter_color(value) : C_FAINT));
    }
    return bx - x + METER_MAX * (seg + gap);
}

static void draw_hud(void) {
    if (!hud_on) return;
    float y = u * 0.9f, size = u * 0.62f, x = L;
    for (int i = 0; i < hud.nmeters; i++)
        x += meter_bar(hud.meter_names[i], hud.meters[i], x, y, size) + u * 1.2f;
    if (hud.word) {
        char up[64], line[128];
        upcase(up, sizeof up, hud.word);
        snprintf(line, sizeof line, "%s%s", up, hud.word_used ? "  used" : "");
        float w = text_w(bold, line, size), lw = text_w(font, "word of the day  ", size);
        float wx = L + W - w;
        if (wx - lw > x) {      // only if it fits next to the meters
            text(font, "word of the day  ", wx - lw, y, size, C_DIM);
            text(bold, line, wx, y, size, hud.word_used ? C_OK : C_WARN);
        }
    }
}

static void draw_header(const Scene *s) {
    float y = u * 2.6f, size = u * 0.7f;
    char up[64], count[32];
    if (s->phase) {
        upcase(up, sizeof up, s->phase);
        text(bold, up, L, y, size, C_ACCENT);
    }
    snprintf(count, sizeof count, "%d / %d", s->idx + 1, s->total);
    text(font, count, L + W - text_w(font, count, size), y, size, C_DIM);
}

// Title and cue; returns the y below them.
static float draw_situation(const Scene *s) {
    float y = u * 4.4f;
    y += wrapped(bold, s->title, L, y, W, u * 1.2f, C_TEXT, 1);
    if (s->detail) {
        y += u * 0.5f;
        float h = wrapped(font, s->detail, L + u * 0.8f, y, W - u * 0.8f, u * 1.0f, C_CUE, 0);
        DrawRectangleRec((Rectangle){ L, y + u * 0.1f, u * 0.14f, h - u * 0.3f }, col(C_WARN));
        wrapped(font, s->detail, L + u * 0.8f, y, W - u * 0.8f, u * 1.0f, C_CUE, 1);
        y += h;
    }
    return y;
}

static void press(const char *s) {
    float size = u * 0.62f;
    double t = now();
    Color c = ColorAlpha(C_DIM, 0.55f + 0.45f * (float)(0.5 + 0.5 * sin(t * 3.0)));
    text(font, s, L, (float)GetScreenHeight() - u * 2.2f, size, c);
}

/* ---------- screens ---------- */

static void draw_menu(void) {
    const MenuView *v = &view.menu;
    float y = u * 4;
    text(bold, "typed_stories", L, y, u * 2.4f, C_TEXT);
    y += u * 3.0f;
    text(font, "a typing game about ordinary days. answer in your own words.", L, y, u * 0.7f, C_DIM);
    y += u * 2.6f;
    for (int i = 0; i < v->npacks; i++) {
        char key[8], rec[80] = "";
        snprintf(key, sizeof key, "%d", i + 1);
        float w = chip(key, L, y, u * 0.75f, C_ACCENT, C_ACCENT, (Color){ 0 });
        text(bold, v->packs[i].name, L + w + u * 0.8f, y + u * 0.15f, u * 0.95f, C_TEXT);
        if (v->packs[i].best >= 0) snprintf(rec, sizeof rec, "best %d", v->packs[i].best);
        if (v->packs[i].today >= 0)
            snprintf(rec + strlen(rec), sizeof rec - strlen(rec), "%stoday %d", *rec ? "   " : "", v->packs[i].today);
        if (*rec) text(font, rec, L + W - text_w(font, rec, u * 0.7f), y + u * 0.3f, u * 0.7f, C_DIM);
        y += u * 1.9f;
    }
    y += u * 0.8f;
    float w = chip("T", L, y, u * 0.75f, C_DIM, C_FAINT, (Color){ 0 });
    char mode[160];
    if (v->today_mode) snprintf(mode, sizeof mode, "today: the same day for everyone, %s", v->date);
    else snprintf(mode, sizeof mode, "free play: a random day");
    text(font, mode, L + w + u * 0.8f, y + u * 0.2f, u * 0.8f, v->today_mode ? C_WARN : C_DIM);
    y += u * 1.6f;
    w = chip("Q", L, y, u * 0.75f, C_DIM, C_FAINT, (Color){ 0 });
    text(font, "quit", L + w + u * 0.8f, y + u * 0.2f, u * 0.8f, C_DIM);
    if (v->word) {
        char up[64], line[128];
        upcase(up, sizeof up, v->word);
        snprintf(line, sizeof line, "%s", up);
        float yy = (float)GetScreenHeight() - u * 3.2f;
        text(font, "word of the day", L, yy, u * 0.62f, C_DIM);
        text(bold, line, L, yy + u * 0.9f, u * 1.1f, C_WARN);
    }
}

static void draw_day(void) {
    const DayStartView *v = &view.day;
    float y = u * 4.4f;
    y += wrapped(bold, v->pack, L, y, W, u * 1.8f, C_TEXT, 1) + u * 0.6f;
    char line[200];
    if (v->today_mode) snprintf(line, sizeof line, "Today, %s. Everyone gets the same day.", v->date);
    else snprintf(line, sizeof line, "Free play: a random day.");
    y += wrapped(font, line, L, y, W, u * 0.85f, v->today_mode ? C_WARN : C_DIM, 1) + u * 1.2f;
    if (v->word) {
        char up[64];
        upcase(up, sizeof up, v->word);
        text(font, "word of the day", L, y, u * 0.65f, C_DIM);
        y += u * 1.0f;
        text(bold, up, L, y, u * 1.6f, C_WARN);
        y += u * 2.1f;
        y += wrapped(font, "Slip it into any answer today for +100 style. It does not change your rating.",
                     L, y, W, u * 0.8f, C_TEXT, 1) + u * 0.9f;
    }
    if (v->has_meters)
        y += wrapped(font, "Keep the meters at the top above zero, or the day ends early.", L, y, W, u * 0.8f, C_TEXT, 1) + u * 0.9f;
    if (v->best >= 0) {
        if (v->today >= 0) snprintf(line, sizeof line, "Your best: %d. Today's best: %d.", v->best, v->today);
        else snprintf(line, sizeof line, "Your best: %d.", v->best);
        wrapped(font, line, L, y, W, u * 0.8f, C_DIM, 1);
    }
    press("Press Enter to start.");
}

static void draw_intro(void) {
    draw_header(&view.intro);
    draw_situation(&view.intro);
    press("Press Enter when ready. The timer starts right after.");
}

static void draw_prompt(void) {
    const PromptView *v = &view.prompt;
    draw_header(&v->scene);
    float y = draw_situation(&v->scene) + u * 1.1f;

    // what each answer means; the words are up to the player
    float x = L, size = u * 0.68f;
    for (int i = 0; i < v->nhints; i++) {
        float w = text_w(font, v->hints[i], size) + size * 1.1f;
        if (x > L && x + w > L + W) { x = L; y += size * 2.1f; }
        x += chip(v->hints[i], x, y, size, C_DIM, C_FAINT, (Color){ 0 }) + u * 0.5f;
    }
    y += size * 2.6f;

    // the input line shakes for a moment after a miss
    double since = now() - msg_since;
    float shake = since < 0.35 ? (float)(sin(since * 70.0) * u * 0.35 * (1.0 - since / 0.35)) : 0;
    Rectangle box = { L + shake, y, W, u * 2.0f };
    DrawRectangleRounded(box, 0.25f, 8, col(C_PANEL));
    if (*v->msg) DrawRectangleRoundedLinesEx(box, 0.25f, 8, 2, col(C_BAD));
    float ts = u * 1.05f, tx = box.x + u * 0.8f, ty = box.y + (box.height - ts) / 2;
    text(bold, ">", tx, ty, ts, C_ACCENT);
    tx += text_w(bold, "> ", ts);
    text(font, v->input, tx, ty, ts, C_TEXT);
    if (fmod(now(), 1.0) < 0.6) {
        float cx = tx + text_w(font, v->input, ts) + (*v->input ? spacing(ts) : 0);
        DrawRectangleRec((Rectangle){ cx, ty + ts * 0.05f, ts * 0.5f, ts * 0.95f }, col(ColorAlpha(C_TEXT, 0.7f)));
    }
    y += box.height + u * 0.7f;

    // time
    float frac = (float)(v->left / v->limit);
    if (frac < 0) frac = 0;
    Color tc = frac < 0.3f ? C_BAD : frac < 0.6f ? C_WARN : C_OK;
    char secs[32];
    snprintf(secs, sizeof secs, "%.1fs", v->left > 0 ? v->left : 0);
    float sw = text_w(font, secs, u * 0.62f) + u * 0.6f;
    DrawRectangleRounded((Rectangle){ L, y, W - sw, u * 0.28f }, 1, 6, col(C_FAINT));
    DrawRectangleRounded((Rectangle){ L, y, (W - sw) * frac, u * 0.28f }, 1, 6, col(tc));
    text(font, secs, L + W - sw + u * 0.6f, y - u * 0.2f, u * 0.62f, tc);
    y += u * 1.1f;

    if (*v->msg) wrapped(font, v->msg, L, y, W, u * 0.8f, C_BAD, 1);
}

static Color rank_color(char r) {
    switch (r) {
    case 'S': return C_WARN;
    case 'A': return C_OK;
    case 'B': return C_ACCENT;
    case 'C': return C_TEXT;
    default:  return C_DIM;
    }
}

static void draw_outcome(void) {
    const OutcomeView *v = &view.out;
    draw_header(&v->scene);
    float y = draw_situation(&v->scene) + u * 1.2f;
    if (v->typed) {
        char q[INPUT_MAX + 8];
        snprintf(q, sizeof q, "\"%s\"", v->typed);
        y += wrapped(bold, q, L, y, W, u * 1.15f, C_OK, 1) + u * 0.5f;
    } else {
        text(bold, "TIME'S UP", L, y, u * 1.15f, C_BAD);
        y += u * 2.0f;
    }
    y += wrapped(font, v->text, L, y, W, u * 0.95f, C_TEXT, 1);
    if (v->fate) y += wrapped(font, v->fate, L, y, W, u * 0.95f, C_TEXT, 1);
    if (v->fx.n) {
        y += u * 0.7f;
        float x = L;
        for (int i = 0; i < v->fx.n; i++) {
            char s[64];
            snprintf(s, sizeof s, "%s %+d", v->fx.name[i], v->fx.delta[i]);
            Color c = v->fx.delta[i] < 0 ? C_BAD : C_OK;
            x += chip(s, x, y, u * 0.7f, c, c, (Color){ 0 }) + u * 0.5f;
        }
        y += u * 1.6f;
    }
    if (v->typed) {
        y += u * 0.9f;
        float box = u * 1.9f;
        Color rc = rank_color(v->rank);
        DrawRectangleRoundedLinesEx((Rectangle){ L, y, box, box }, 0.3f, 8, 2.5f, col(rc));
        char r[2] = { v->rank, 0 };
        float rs = u * 1.3f;
        text(bold, r, L + (box - text_w(bold, r, rs)) / 2, y + (box - rs) / 2, rs, rc);
        char pts[32];
        snprintf(pts, sizeof pts, "+%d style", v->points);
        float x = L + box + u * 0.8f;
        text(bold, pts, x, y + u * 0.1f, u * 0.85f, C_TEXT);
        char secs[32];
        snprintf(secs, sizeof secs, "%.1fs", v->seconds);
        text(font, secs, L + W - text_w(font, secs, u * 0.62f), y + u * 0.2f, u * 0.62f, C_DIM);
        float ny = y + u * 1.1f;
        if (v->typo) x += chip("typo: half style", x, ny, u * 0.55f, C_DIM, C_FAINT, (Color){ 0 }) + u * 0.4f;
        if (v->repeated) x += chip("heard that before", x, ny, u * 0.55f, C_DIM, C_FAINT, (Color){ 0 }) + u * 0.4f;
        if (v->word_bonus) x += chip("WORD OF THE DAY +100", x, ny, u * 0.55f, C_WARN, C_WARN, (Color){ 0 }) + u * 0.4f;
        if (v->word_again) chip("word already used today", x, ny, u * 0.55f, C_DIM, C_FAINT, (Color){ 0 });
    }
    press("press any key");
}

static void draw_ending(void) {
    char up[64], line[160];
    upcase(up, sizeof up, view.ending.meter);
    snprintf(line, sizeof line, "%s HIT ZERO", up);
    float y = u * 6;
    text(bold, line, L, y, u * 1.8f, C_BAD);
    y += u * 2.6f;
    text(font, "The day is over.", L, y, u * 0.9f, C_DIM);
    y += u * 2.0f;
    wrapped(font, view.ending.text, L, y, W, u * 1.0f, C_TEXT, 1);
    press("press any key");
}

static void stat_row(const char *label, const char *value, float y, Color c) {
    text(font, label, L, y, u * 0.75f, C_DIM);
    text(font, value, L + u * 7, y, u * 0.75f, c);
}

static void draw_summary(void) {
    const SummaryView *v = &view.sum;
    float y = u * 2.6f;
    text(bold, v->ended_early ? "DAY OVER EARLY" : "DAY COMPLETE", L, y, u * 0.7f, v->ended_early ? C_BAD : C_ACCENT);
    y += u * 1.2f;
    y += wrapped(bold, v->pack, L, y, W, u * 1.1f, C_TEXT, 1) + u * 0.6f;

    char big[16];
    snprintf(big, sizeof big, "%d", v->rating);
    text(bold, big, L, y, u * 3.4f, v->rating >= 7 ? C_OK : v->rating >= 4 ? C_WARN : C_BAD);
    text(font, "/ 10", L + text_w(bold, big, u * 3.4f) + u * 0.5f, y + u * 1.9f, u * 0.9f, C_DIM);
    float vx = L + u * 8;
    wrapped(font, v->verdict, vx, y + u * 1.0f, W - u * 8, u * 1.0f, C_TEXT, 1);
    y += u * 4.4f;

    char s[160];
    snprintf(s, sizeof s, "%c   avg %d per answer", v->style_rank, v->style_avg);
    stat_row("style", s, y, rank_color(v->style_rank));
    y += u * 1.15f;
    if (v->word) {
        snprintf(s, sizeof s, "%s, %s", v->word, v->word_used ? "used (+100)" : "not used");
        stat_row("word", s, y, v->word_used ? C_OK : C_DIM);
        y += u * 1.15f;
    }
    snprintf(s, sizeof s, "%.0f%% accuracy, %.0f wpm", v->accuracy, v->wpm);
    stat_row("typing", s, y, C_TEXT);
    y += u * 1.15f;
    snprintf(s, sizeof s, "%d timeout%s, %d not understood", v->timeouts, v->timeouts == 1 ? "" : "s", v->misses);
    stat_row("misses", s, y, C_TEXT);
    y += u * 1.15f;
    if (v->meters.nmeters) {
        text(font, "meters", L, y, u * 0.75f, C_DIM);
        float x = L + u * 7;
        for (int i = 0; i < v->meters.nmeters; i++)
            x += meter_bar(v->meters.meter_names[i], v->meters.meters[i], x, y + u * 0.05f, u * 0.62f) + u * 1.0f;
        y += u * 1.15f;
    }
    y += u * 0.8f;

    snprintf(s, sizeof s, "%d", v->score);
    text(font, "score", L, y + u * 0.45f, u * 0.75f, C_DIM);
    text(bold, s, L + u * 7, y, u * 1.5f, C_TEXT);
    float x = L + u * 7 + text_w(bold, s, u * 1.5f) + u * 0.9f;
    if (v->new_best && v->prev_best >= 0) snprintf(s, sizeof s, "NEW RECORD (was %d)", v->prev_best);
    else if (v->new_best) snprintf(s, sizeof s, "first record");
    else snprintf(s, sizeof s, "best %d", v->prev_best);
    chip(s, x, y + u * 0.2f, u * 0.6f, v->new_best ? C_WARN : C_DIM, v->new_best ? C_WARN : C_FAINT, (Color){ 0 });
    if (v->today_mode) {
        y += u * 2.0f;
        snprintf(s, sizeof s, "best %d%s", v->today_best, v->new_day ? "  (that's you)" : "");
        stat_row("today", s, y, C_WARN);
    }
    press("press any key");
}

static void frame(void) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    u = (float)sh / 28.0f;
    W = (float)sw - u * 4;
    if (W > u * 44) W = u * 44;
    L = ((float)sw - W) / 2;
    double t = (now() - screen_since) / FADE_S;
    alpha = t >= 1 ? 1.0f : t <= 0 ? 0.0f : (float)t;

    BeginDrawing();
    ClearBackground(C_BG);
    if (screen != S_MENU) {
        float a = alpha;
        alpha = 1;              // the hud does not fade with the screen
        draw_hud();
        alpha = a;
    }
    switch (screen) {
    case S_MENU:    draw_menu();    break;
    case S_DAY:     draw_day();     break;
    case S_INTRO:   draw_intro();   break;
    case S_PROMPT:  draw_prompt();  break;
    case S_OUTCOME: draw_outcome(); break;
    case S_ENDING:  draw_ending();  break;
    case S_SUMMARY: draw_summary(); break;
    default: break;
    }
    EndDrawing();
}

/* ---------- keys ---------- */

static int queue[128];
static int qhead, qtail;

static void push(int k) {
    int next = (qtail + 1) % (int)(sizeof queue / sizeof *queue);
    if (next == qhead) return;
    queue[qtail] = k;
    qtail = next;
}

static void toggle_fullscreen(void) {
    ToggleBorderlessWindowed();
}

// Collects this frame's input into the queue: raylib forgets it next frame.
static void poll_keys(void) {
    int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    int alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    int c, k;
    while ((c = GetCharPressed()) > 0)
        if (!ctrl && c >= 32 && c <= 126) push(c);
    while ((k = GetKeyPressed()) > 0) {
        if (k == KEY_ENTER || k == KEY_KP_ENTER) { if (alt) toggle_fullscreen(); else push(K_ENTER); }
        else if (k == KEY_F11) toggle_fullscreen();
        else if (k == KEY_BACKSPACE) push(ctrl ? K_CTRL_W : K_BACKSPACE);
        else if (ctrl && k == KEY_W) push(K_CTRL_W);
        else if (ctrl && k == KEY_U) push(K_CTRL_U);
        else if (k >= 32 && k <= 96) {}                 // printable: it came as a character
        else if (k >= KEY_LEFT_SHIFT && k <= KEY_KB_MENU) {}   // modifiers alone
        else push(K_OTHER);
    }
    if (IsKeyPressedRepeat(KEY_BACKSPACE)) push(ctrl ? K_CTRL_W : K_BACKSPACE);
}

int ui_key(int timeout_ms) {
    double end = now() + (timeout_ms < 0 ? 1e9 : timeout_ms / 1000.0);
    for (;;) {
        if (qhead != qtail) {
            int k = queue[qhead];
            qhead = (qhead + 1) % (int)(sizeof queue / sizeof *queue);
            return k;
        }
        frame();
        if (WindowShouldClose()) {      // the window was closed: leave right away
            ui_end();
            exit(0);
        }
        poll_keys();
        if (qhead == qtail && now() >= end) return K_NONE;
    }
}

void ui_flush(void) {
    while (GetCharPressed() > 0) {}
    while (GetKeyPressed() > 0) {}
    qhead = qtail = 0;
}

/* ---------- window ---------- */

// Fonts live in assets/fonts, next to the program or in the working directory.
static Font load_font(const char *file) {
    char path[1024];
    const char *bases[] = { "assets/fonts", NULL };
    snprintf(path, sizeof path, "%s/%s", bases[0], file);
    if (!FileExists(path)) snprintf(path, sizeof path, "%sassets/fonts/%s", GetApplicationDirectory(), file);
    if (!FileExists(path)) return GetFontDefault();
    Font f = LoadFontEx(path, FONT_PX, NULL, 0);
    GenTextureMipmaps(&f.texture);
    SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
    return f;
}

void ui_init(void) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "typed_stories");
    SetWindowMinSize(800, 500);
    SetExitKey(KEY_NULL);           // Esc is a key like any other, not "quit"
    SetTargetFPS(60);
    font = load_font("JetBrainsMono-Regular.ttf");
    bold = load_font("JetBrainsMono-Bold.ttf");
}

void ui_end(void) {
    if (font.texture.id != GetFontDefault().texture.id) UnloadFont(font);
    if (bold.texture.id != GetFontDefault().texture.id) UnloadFont(bold);
    CloseWindow();
}
