// ui_gl: the window version of ui.h, on raylib. Everything is laid out in
// pixels from one unit (u) that scales with the window height.
//
// Each pack has a look ('style:' in the pack file), kept in a Theme: palette,
// fonts and the decorations drawn around the same screens.
//   abstract  the ordinary day: dark, clean, geometric compositions per phase
//   ornate    the knight: parchment, old print, flourished frame, drop caps
//   terminal  the robot: phosphor green, scanlines, glow, typed-out text, glitches
#include <ctype.h>
#include <math.h>

#include "config.h"
#include "raylib.h"
#include "rng.h"
#include "ui.h"
#include "util.h"
#include "activity.h"
#include "stage_gl.h"

#define FONT_PX  64             // glyphs are rasterized once at this size and scaled down
#define FADE_S   (cfg.fade)     // fade-in of a new screen (config.ini)
#define SHAKE_S  0.35           // how long the input shakes (or glitches) after a miss

/* ---------- themes ---------- */

enum { TH_ABSTRACT, TH_ORNATE, TH_TERMINAL, TH_COUNT };

// fonts, loaded once
enum { F_MONO, F_MONO_BOLD, F_FELL, F_FELL_IT, F_FELL_SC, F_FRAKTUR, F_VT, F_COUNT };
static Font fonts[F_COUNT];

typedef struct {
    Color bg, panel, faint, dim, text, cue, ok, bad, warn, accent;
    Color menu;                 // the pack's name in the menu, drawn on the default background
    int   body, head, cue_font, cap;    // fonts
    float scale;                // glyph size against the layout (old print and pixel fonts run small)
    float round;                // corners of panels and chips
    const char *prompt;         // in front of the input
    const char *timeout;        // when time runs out
    const char *the_end;        // above the finale's title
    const char *any_key;
} Theme;

static const Theme themes[TH_COUNT] = {
    [TH_ABSTRACT] = {
        .bg = { 17, 19, 24, 255 },     .panel = { 28, 31, 39, 255 },   .faint = { 52, 57, 70, 255 },
        .dim = { 128, 134, 148, 255 }, .text = { 232, 230, 227, 255 }, .cue = { 214, 204, 182, 255 },
        .ok = { 123, 211, 137, 255 },  .bad = { 255, 107, 107, 255 },  .warn = { 247, 201, 72, 255 },
        .accent = { 108, 178, 255, 255 }, .menu = { 232, 230, 227, 255 },
        .body = F_MONO, .head = F_MONO_BOLD, .cue_font = F_MONO, .cap = -1,
        .scale = 1.0f, .round = 0.5f, .prompt = ">", .timeout = "TIME'S UP",
        .the_end = "THE END", .any_key = "press any key",
    },
    [TH_ORNATE] = {
        .bg = { 236, 224, 196, 255 },  .panel = { 226, 210, 176, 255 }, .faint = { 196, 172, 128, 255 },
        .dim = { 122, 96, 66, 255 },   .text = { 50, 34, 20, 255 },     .cue = { 92, 50, 28, 255 },
        .ok = { 54, 104, 50, 255 },    .bad = { 150, 34, 26, 255 },     .warn = { 160, 112, 24, 255 },
        .accent = { 132, 28, 28, 255 }, .menu = { 222, 182, 104, 255 },
        .body = F_FELL, .head = F_FELL_SC, .cue_font = F_FELL_IT, .cap = F_FRAKTUR,
        .scale = 1.22f, .round = 0.18f, .prompt = "~", .timeout = "Too late!",
        .the_end = "Finis", .any_key = "press any key to turn the page",
    },
    [TH_TERMINAL] = {
        .bg = { 4, 10, 6, 255 },       .panel = { 8, 24, 13, 255 },     .faint = { 22, 62, 34, 255 },
        .dim = { 54, 150, 80, 255 },   .text = { 150, 255, 168, 255 },  .cue = { 255, 194, 92, 255 },
        .ok = { 150, 255, 168, 255 },  .bad = { 255, 86, 86, 255 },     .warn = { 255, 194, 92, 255 },
        .accent = { 96, 236, 196, 255 }, .menu = { 120, 255, 150, 255 },
        .body = F_VT, .head = F_VT, .cue_font = F_VT, .cap = -1,
        .scale = 1.38f, .round = 0.0f, .prompt = "$", .timeout = "ERR: TIMEOUT",
        .the_end = "PROCESS EXITED", .any_key = "[ press any key ]",
    },
};

static int          th_id = TH_ABSTRACT;
static const Theme *th = &themes[TH_ABSTRACT];

#define C_BG     th->bg
#define C_PANEL  th->panel
#define C_FAINT  th->faint
#define C_DIM    th->dim
#define C_TEXT   th->text
#define C_CUE    th->cue
#define C_OK     th->ok
#define C_BAD    th->bad
#define C_WARN   th->warn
#define C_ACCENT th->accent
#define font     fonts[th->body]
#define bold     fonts[th->head]
#define cuefont  fonts[th->cue_font]
#define NONE     ((Color){ 0 })

static int theme_of(const char *style) {
    if (style && !strcmp(style, "ornate")) return TH_ORNATE;
    if (style && !strcmp(style, "terminal")) return TH_TERMINAL;
    return TH_ABSTRACT;
}

void ui_set_theme(const char *style) {
    th_id = theme_of(style);
    th = &themes[th_id];
}

/* ---------- drawing text ---------- */

static float u;                 // layout unit: 1/28 of the window height
static float L, W;              // left edge and width of the content column
static float alpha = 1;         // fade of the current screen
static long  reveal = -1;       // terminal: characters left to type out this frame, -1 = all

static Color col(Color c) {
    return ColorAlpha(c, alpha * (float)c.a / 255.0f);
}

static float spacing(float size) { return size * (th_id == TH_TERMINAL ? 0.0f : 0.02f); }

static float text_w(Font f, const char *s, float size) {
    size *= th->scale;
    return MeasureTextEx(f, s, size, spacing(size)).x;
}

static void text(Font f, const char *s, float x, float y, float size, Color c) {
    char buf[1024];
    if (reveal >= 0) {          // typed out a few characters at a time
        size_t n = strlen(s);
        if (reveal == 0) return;
        if ((long)n > reveal) {
            snprintf(buf, sizeof buf, "%.*s", (int)reveal, s);
            s = buf;
            n = (size_t)reveal;
        }
        reveal -= (long)n;
    }
    float ds = size * th->scale;
    // old print and pixel fonts sit lower in their box than the layout expects
    float dy = th_id == TH_TERMINAL ? -size * 0.22f : th_id == TH_ORNATE ? -size * 0.16f : 0;
    Vector2 p = { x, y + dy };
    if (th_id == TH_TERMINAL) {     // phosphor glow
        float g = size * 0.06f;
        Color gc = col(ColorAlpha(c, 0.16f));
        DrawTextEx(f, s, (Vector2){ p.x - g, p.y }, ds, spacing(ds), gc);
        DrawTextEx(f, s, (Vector2){ p.x + g, p.y }, ds, spacing(ds), gc);
        DrawTextEx(f, s, (Vector2){ p.x, p.y - g }, ds, spacing(ds), gc);
        DrawTextEx(f, s, (Vector2){ p.x, p.y + g }, ds, spacing(ds), gc);
    }
    DrawTextEx(f, s, p, ds, spacing(ds), col(c));
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

// A label in a box; returns its width.
static float chip(const char *s, float x, float y, float size, Color fg, Color line, Color fill) {
    float pad = size * 0.55f, w = text_w(font, s, size) + 2 * pad, h = size * 1.6f;
    Rectangle r = { x, y, w, h };
    if (fill.a) DrawRectangleRounded(r, th->round, 8, col(fill));
    if (line.a) {
        if (th->round > 0) DrawRectangleRoundedLinesEx(r, th->round, 8, 1.5f, col(line));
        else DrawRectangleLinesEx(r, 1.5f, col(line));
    }
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

enum { S_NONE, S_MENU, S_DAY, S_INTRO, S_PROMPT, S_WALK, S_MINI, S_OUTCOME, S_ENDING, S_FINALE, S_SUMMARY };

static int    screen = S_NONE;
static double screen_since;     // for the fade-in
static char   arena[32768];
static size_t arena_used;
static char   hud_arena[1024];
static size_t hud_used;
static char   mood_key[128] = "day";    // abstract: which composition is in the background

static union {
    MenuView     menu;
    DayStartView day;
    Scene        intro;
    PromptView   prompt;
    OutcomeView  out;
    EndingView   ending;
    FinaleView   finale;
    SummaryView  sum;
    WalkView     walk;
    MinigameView mini;
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

// The picture's clocks: when this scene first appeared (people walk in), and
// when its reaction started.
static int    stage_idx = -1;
static double stage_since, show_since;
static char   last_show[32];

static void keep_scene(Scene *s) {
    s->phase = keep(s->phase);
    s->title = keep(s->title);
    s->detail = keep(s->detail);
    s->stage = keep(s->stage);
    s->props = keep(s->props);
    s->show = keep(s->show);
    if (s->idx != stage_idx) { stage_idx = s->idx; stage_since = now(); last_show[0] = '\0'; }
    if (strcmp(s->show ? s->show : "", last_show)) {
        snprintf(last_show, sizeof last_show, "%s", s->show ? s->show : "");
        show_since = now();
    }
    snprintf(mood_key, sizeof mood_key, "%s", s->phase ? s->phase : "day");
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
    for (int i = 0; i < v->npacks; i++) {
        view.menu.packs[i].name = keep(v->packs[i].name);
        view.menu.packs[i].style = keep(v->packs[i].style);
    }
    view.menu.date = keep(v->date);
    view.menu.word = keep(v->word);
}

void ui_day_start(const DayStartView *v) {
    set_screen(S_DAY, -1);
    stage_idx = -1;
    view.day = *v;
    view.day.pack = keep(v->pack);
    view.day.date = keep(v->date);
    view.day.word = keep(v->word);
    snprintf(mood_key, sizeof mood_key, "%s", v->pack);
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

void ui_finale(const FinaleView *v) {
    set_screen(S_FINALE, -1);
    view.finale = *v;
    view.finale.title = keep(v->title);
    view.finale.text = keep(v->text);
}

void ui_walk(const WalkView *v) {
    set_screen(S_WALK, v->scene.idx);
    view.walk = *v;
    keep_scene(&view.walk.scene);
    view.walk.answer = keep(v->answer);
    view.walk.goal_name = keep(v->goal_name);
}

void ui_minigame(const MinigameView *v) {
    set_screen(S_MINI, v->scene.idx);
    view.mini = *v;
    view.mini.scene.show = "act";
    keep_scene(&view.mini.scene);
    view.mini.text = keep(v->text);
}

static int presses;             // key downs, without the repeats of a held key

int ui_presses(void) {
    int n = presses;
    presses = 0;
    return n;
}

// Walking keys go by the key's place on the keyboard, so WASD works in any layout.
int ui_held(void) {
    int m = 0;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    m |= DIR_UP;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  m |= DIR_DOWN;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  m |= DIR_LEFT;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) m |= DIR_RIGHT;
    return m;
}

void ui_summary(const SummaryView *v) {
    set_screen(S_SUMMARY, -1);
    view.sum = *v;
    view.sum.pack = keep(v->pack);
    view.sum.verdict = keep(v->verdict);
    view.sum.word = keep(v->word);
    for (int i = 0; i < v->meters.nmeters; i++) view.sum.meters.meter_names[i] = keep(v->meters.meter_names[i]);
}

/* ---------- abstract: compositions ---------- */

static uint64_t next_bits(uint64_t *h) {
    *h = rng_mix(*h + 0x9e3779b97f4a7c15ULL);
    return *h;
}

static float unit_rand(uint64_t *h) { return (float)(next_bits(h) % 10000) / 10000.0f; }

// One shape of a composition, centered at (x, y) with size r.
static void shape(int kind, float x, float y, float r, float rot, Color c) {
    switch (kind) {
    case 0: DrawCircleV((Vector2){ x, y }, r, c); break;
    case 1: DrawRing((Vector2){ x, y }, r * 0.82f, r, 0, 360, 64, c); break;
    case 2: DrawRectanglePro((Rectangle){ x, y, r * 2.2f, r * 0.55f }, (Vector2){ r * 1.1f, r * 0.275f }, rot, c); break;
    case 3: DrawPoly((Vector2){ x, y }, 3, r, rot, c); break;
    case 4: {
        float a = rot * DEG2RAD, dx = cosf(a) * r * 1.8f, dy = sinf(a) * r * 1.8f;
        DrawLineEx((Vector2){ x - dx, y - dy }, (Vector2){ x + dx, y + dy }, u * 0.16f, c);
        break;
    }
    default: DrawCircleSector((Vector2){ x, y }, r, rot, rot + 180, 32, c); break;
    }
}

// Each phase of the day has its own arrangement of a few shapes, drifting slowly.
static void abstract_backdrop(const char *key) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    uint64_t h = rng_hash(key);
    Color pal[5] = { C_ACCENT, C_WARN, C_BAD, C_OK, C_CUE };
    double t = now();
    float ax = L + W * 0.74f, ay = sh * 0.6f;
    // a faint dot grid ties the shapes together
    for (float gx = ax - u * 9; gx < sw; gx += u * 1.4f)
        for (float gy = ay - u * 9; gy < sh; gy += u * 1.4f)
            DrawCircleV((Vector2){ gx, gy }, u * 0.05f, col(ColorAlpha(C_FAINT, 0.6f)));
    for (int i = 0; i < 7; i++) {
        int kind = (int)(next_bits(&h) % 6);
        float r = u * (1.2f + 4.0f * unit_rand(&h));
        float x = ax + (unit_rand(&h) - 0.5f) * u * 16 + (float)sin(t * 0.13 + i) * u * 0.5f;
        float y = ay + (unit_rand(&h) - 0.5f) * u * 14 + (float)cos(t * 0.11 + i * 1.7) * u * 0.4f;
        float rot = unit_rand(&h) * 360 + (float)t * (i % 2 ? 3.0f : -2.0f);
        Color c = ColorAlpha(pal[next_bits(&h) % 5], 0.10f + 0.12f * unit_rand(&h));
        shape(kind, x, y, r, rot, col(c));
    }
    (void)sw;
}

// How the day ended, as shapes: settled and whole when it went well, scattered when not.
static void abstract_finale(int good, int special) {
    int sh = GetScreenHeight();
    float cx = L + W * 0.8f, cy = sh * 0.42f;
    double t = now();
    if (special) {
        for (int i = 0; i < 8; i++) {
            float r = u * (1.0f + i * 0.8f) + (float)sin(t * 0.8 + i) * u * 0.15f;
            DrawRing((Vector2){ cx, cy }, r - u * 0.08f, r, 0, 360, 96, col(ColorAlpha(C_WARN, 0.32f - i * 0.03f)));
        }
        for (int i = 0; i < 12; i++) {
            float a = (float)(i * 30 + t * 6) * DEG2RAD;
            DrawCircleV((Vector2){ cx + cosf(a) * u * 7.8f, cy + sinf(a) * u * 7.8f }, u * 0.28f, col(ColorAlpha(C_CUE, 0.5f)));
        }
    } else if (good == 2) {
        DrawCircleV((Vector2){ cx, cy }, u * 4.2f, col(ColorAlpha(C_OK, 0.22f)));
        DrawRing((Vector2){ cx, cy }, u * 5.4f, u * 5.7f, 0, 360, 96, col(ColorAlpha(C_ACCENT, 0.35f)));
        DrawRing((Vector2){ cx, cy }, u * 7.2f, u * 7.35f, 0, 360, 96, col(ColorAlpha(C_ACCENT, 0.2f)));
        for (int i = 0; i < 3; i++)
            DrawRectangle((int)(cx - u * 6), (int)(cy + u * (9 + i * 1.1f)), (int)(u * 12), (int)(u * 0.4f), col(ColorAlpha(C_WARN, 0.3f - i * 0.08f)));
    } else if (good == 1) {
        DrawCircleV((Vector2){ cx - u * 1.5f, cy }, u * 3.4f, col(ColorAlpha(C_ACCENT, 0.2f)));
        DrawPoly((Vector2){ cx + u * 3, cy + u * 2 }, 3, u * 3.0f, 30, col(ColorAlpha(C_WARN, 0.22f)));
        DrawRectanglePro((Rectangle){ cx, cy + u * 7, u * 11, u * 0.5f }, (Vector2){ u * 5.5f, u * 0.25f }, -6, col(ColorAlpha(C_CUE, 0.3f)));
    } else {
        uint64_t h = 77;
        for (int i = 0; i < 9; i++) {
            float x = cx + (unit_rand(&h) - 0.5f) * u * 18, y = cy + (unit_rand(&h) - 0.2f) * u * 16;
            float r = u * (0.8f + 2.4f * unit_rand(&h)), rot = unit_rand(&h) * 360 + (float)t * 4;
            shape((int)(next_bits(&h) % 6), x, y + (float)sin(t * 0.5 + i) * u * 0.3f, r, rot,
                  col(ColorAlpha(i % 3 ? C_FAINT : C_BAD, 0.35f)));
        }
    }
}

/* ---------- ornate: paper and flourishes ---------- */

static Texture paper;           // noise for the parchment, made at start

static void ornate_line(const Vector2 *pts, int n, float thick, Color c) {
    DrawSplineLinear(pts, n, thick, c);
}

// A curl: a spiral that starts at p, going along d, and winds toward the side n.
static void curl(Vector2 p, Vector2 d, Vector2 n, float R, float turns, float thick, Color c) {
    Vector2 ctr = { p.x + n.x * R, p.y + n.y * R };
    float a0 = atan2f(-n.y, -n.x);
    float tx = -sinf(a0), ty = cosf(a0);
    float dir = tx * d.x + ty * d.y >= 0 ? 1.0f : -1.0f;
    Vector2 pts[96];
    int k = 0;
    for (; k < 96; k++) {
        float f = (float)k / 95.0f;
        float a = a0 + dir * f * turns * 2 * PI;
        float r = R * (1.0f - 0.82f * f);
        pts[k] = (Vector2){ ctr.x + cosf(a) * r, ctr.y + sinf(a) * r };
    }
    ornate_line(pts, k, thick, c);
    DrawCircleV(pts[k - 1], thick * 1.4f, c);
}

static void diamond(float x, float y, float r, Color c) {
    DrawPoly((Vector2){ x, y }, 4, r, 0, c);
}

// A corner of the frame: a diamond and a scroll along each edge.
static void corner(float x, float y, float sx, float sy) {
    float s = u * 0.72f, th_ = u * 0.06f;
    Color c = col(C_WARN), c2 = col(ColorAlpha(C_ACCENT, 0.85f));
    diamond(x, y, s * 0.42f, c2);
    diamond(x, y, s * 0.2f, col(C_BG));
    // along the horizontal edge, then along the vertical one
    Vector2 h0 = { x + sx * s * 0.6f, y }, h1 = { x + sx * s * 3.6f, y };
    Vector2 v0 = { x, y + sy * s * 0.6f }, v1 = { x, y + sy * s * 3.6f };
    Vector2 hl[2] = { h0, h1 }, vl[2] = { v0, v1 };
    ornate_line(hl, 2, th_, c);
    ornate_line(vl, 2, th_, c);
    curl(h1, (Vector2){ sx, 0 }, (Vector2){ 0, sy }, s * 0.85f, 1.25f, th_, c);
    curl(v1, (Vector2){ 0, sy }, (Vector2){ sx, 0 }, s * 0.85f, 1.25f, th_, c);
    // a small leaf curling the other way
    curl((Vector2){ x + sx * s * 2.0f, y }, (Vector2){ sx, 0 }, (Vector2){ 0, -sy }, s * 0.35f, 1.0f, th_ * 0.8f, c);
    curl((Vector2){ x, y + sy * s * 2.0f }, (Vector2){ 0, sy }, (Vector2){ -sx, 0 }, s * 0.35f, 1.0f, th_ * 0.8f, c);
    DrawCircleV((Vector2){ x + sx * s * 1.2f, y + sy * s * 1.2f }, s * 0.12f, c2);
}

// A divider: two lines, curls facing a diamond in the middle.
static void divider(float cx, float y, float half) {
    float s = u, th_ = u * 0.07f;
    Color c = col(C_WARN);
    Vector2 l[2] = { { cx - half, y }, { cx - s * 2.2f, y } }, r[2] = { { cx + s * 2.2f, y }, { cx + half, y } };
    ornate_line(l, 2, th_, c);
    ornate_line(r, 2, th_, c);
    DrawCircleV((Vector2){ cx - half, y }, th_ * 1.8f, c);
    DrawCircleV((Vector2){ cx + half, y }, th_ * 1.8f, c);
    curl((Vector2){ cx - s * 2.2f, y }, (Vector2){ 1, 0 }, (Vector2){ 0, -1 }, s * 0.45f, 1.1f, th_, c);
    curl((Vector2){ cx + s * 2.2f, y }, (Vector2){ -1, 0 }, (Vector2){ 0, -1 }, s * 0.45f, 1.1f, th_, c);
    curl((Vector2){ cx - s * 2.2f, y }, (Vector2){ 1, 0 }, (Vector2){ 0, 1 }, s * 0.3f, 1.0f, th_ * 0.8f, c);
    curl((Vector2){ cx + s * 2.2f, y }, (Vector2){ -1, 0 }, (Vector2){ 0, 1 }, s * 0.3f, 1.0f, th_ * 0.8f, c);
    diamond(cx, y, s * 0.36f, col(C_ACCENT));
}

static void ornate_backdrop(void) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (paper.id)
        DrawTexturePro(paper, (Rectangle){ 0, 0, (float)paper.width, (float)paper.height },
                       (Rectangle){ 0, 0, (float)sw, (float)sh }, (Vector2){ 0 }, 0, (Color){ 110, 76, 40, 46 });
    // darker toward the edges, like old paper
    float e = u * 4;
    Color dark = { 96, 64, 30, 70 }, clear = { 96, 64, 30, 0 };
    DrawRectangleGradientV(0, 0, sw, (int)e, dark, clear);
    DrawRectangleGradientV(0, sh - (int)e, sw, (int)e, clear, dark);
    DrawRectangleGradientH(0, 0, (int)e, sh, dark, clear);
    DrawRectangleGradientH(sw - (int)e, 0, (int)e, sh, clear, dark);
    // the frame
    float m = u * 0.9f;
    Rectangle outer = { m, m, sw - 2 * m, sh - 2 * m }, inner = { m + u * 0.32f, m + u * 0.32f, sw - 2 * m - u * 0.64f, sh - 2 * m - u * 0.64f };
    DrawRectangleLinesEx(outer, u * 0.08f, ColorAlpha(C_WARN, 0.9f));
    DrawRectangleLinesEx(inner, u * 0.035f, ColorAlpha(C_WARN, 0.7f));
    float a = alpha;
    alpha = 1;
    float cm = m + u * 0.16f;
    corner(cm, cm, 1, 1);
    corner(sw - cm, cm, -1, 1);
    corner(cm, sh - cm, 1, -1);
    corner(sw - cm, sh - cm, -1, -1);
    alpha = a;
}

/* ---------- terminal: noise, scanlines, glitches ---------- */

static RenderTexture2D crt;     // the terminal is drawn here first, then onto the screen with effects
static int crt_w, crt_h;

static void terminal_backdrop(void) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    // a memory dump scrolling by, too faint to read
    static const char hex[] = "0123456789ABCDEF";
    float size = u * 0.55f, x0 = L + W * 0.74f;
    long tick = (long)(now() * 3);
    for (int row = 0; row * size * 1.25f < sh; row++) {
        char line[64];
        int k = 0;
        uint64_t h = rng_mix((uint64_t)(row + tick));
        for (int i = 0; i < 8 && k + 4 < (int)sizeof line; i++) {
            uint64_t b = next_bits(&h);
            line[k++] = hex[b & 15];
            line[k++] = hex[(b >> 4) & 15];
            line[k++] = ' ';
        }
        line[k] = '\0';
        DrawTextEx(fonts[F_VT], line, (Vector2){ x0, row * size * 1.25f }, size * th->scale, 0, ColorAlpha(C_DIM, 0.07f));
    }
    (void)sw;
}

// Scanlines and a dark rim over the finished frame.
static void terminal_overlay(void) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    for (int y = 0; y < sh; y += 3) DrawRectangle(0, y, sw, 1, (Color){ 0, 0, 0, 70 });
    float e = u * 5;
    Color dark = { 0, 0, 0, 150 }, clear = { 0, 0, 0, 0 };
    DrawRectangleGradientV(0, 0, sw, (int)e, dark, clear);
    DrawRectangleGradientV(0, sh - (int)e, sw, (int)e, clear, dark);
    DrawRectangleGradientH(0, 0, (int)e, sh, dark, clear);
    DrawRectangleGradientH(sw - (int)e, 0, (int)e, sh, clear, dark);
}

// The frame drawn from the texture: whole, or sliced and shifted while it glitches.
static void terminal_present(float glitch) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Texture t = crt.texture;
    if (glitch <= 0) {
        DrawTextureRec(t, (Rectangle){ 0, 0, (float)sw, -(float)sh }, (Vector2){ 0 }, WHITE);
        return;
    }
    uint64_t h = (uint64_t)(now() * 30);
    int slices = 14;
    float sh_ = (float)sh / slices;
    for (int i = 0; i < slices; i++) {
        float off = 0;
        if (unit_rand(&h) < 0.45f) off = (unit_rand(&h) - 0.5f) * u * 2.4f * glitch;
        // texture rows run bottom-up
        Rectangle src = { 0, (float)sh - (i + 1) * sh_, (float)sw, -sh_ };
        Vector2 at = { off, i * sh_ };
        if (off != 0) {
            DrawTextureRec(t, src, (Vector2){ at.x + u * 0.25f * glitch, at.y }, (Color){ 255, 60, 60, 120 });
            DrawTextureRec(t, src, (Vector2){ at.x - u * 0.25f * glitch, at.y }, (Color){ 60, 255, 255, 120 });
        }
        DrawTextureRec(t, src, at, off != 0 ? (Color){ 255, 255, 255, 200 } : WHITE);
    }
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
        Color c = col(i < value ? meter_color(value) : C_FAINT);
        if (th_id == TH_ORNATE) DrawPoly((Vector2){ r.x + seg / 2, r.y + r.height / 2 }, 4, seg * 0.62f, 0, c);
        else DrawRectangleRounded(r, th->round * 0.6f, 4, c);
    }
    return bx - x + METER_MAX * (seg + gap);
}

static void draw_hud(void) {
    if (!hud_on) return;
    float y = u * (th_id == TH_ORNATE ? 2.0f : 0.9f), size = u * 0.62f, x = L;
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

static float header_y(void) { return u * (th_id == TH_ORNATE ? 3.3f : 2.6f); }

static void draw_header(const Scene *s) {
    float y = header_y(), size = u * 0.7f;
    char up[64], line[96], count[32];
    if (s->phase) {
        upcase(up, sizeof up, s->phase);
        Color pc = C_ACCENT;
        if (th_id == TH_ABSTRACT) {     // every phase has its own color
            Color pal[5] = { C_ACCENT, C_WARN, C_BAD, C_OK, C_CUE };
            uint64_t h = rng_hash(s->phase);
            next_bits(&h);
            pc = pal[h % 5];
        }
        if (th_id == TH_TERMINAL) snprintf(line, sizeof line, "> %s", up);
        else snprintf(line, sizeof line, "%s", th_id == TH_ORNATE ? s->phase : up);
        text(bold, line, L, y, size, pc);
    }
    if (th_id == TH_ORNATE) snprintf(count, sizeof count, "page %d of %d", s->idx + 1, s->total);
    else if (th_id == TH_TERMINAL) snprintf(count, sizeof count, "[%02d/%02d]", s->idx + 1, s->total);
    else snprintf(count, sizeof count, "%d / %d", s->idx + 1, s->total);
    text(font, count, L + W - text_w(font, count, size), y, size, C_DIM);
    if (th_id == TH_ORNATE) divider(L + W / 2, y + u * 1.25f, W / 2);
}

// The first letter of a page, big and red, in a gilded box; returns its size.
static float drop_cap(char ch, float x, float y) {
    float box = u * 3.1f;
    char s[2] = { ch, 0 };
    DrawRectangleRec((Rectangle){ x, y, box, box }, col(ColorAlpha(C_PANEL, 0.9f)));
    DrawRectangleLinesEx((Rectangle){ x, y, box, box }, u * 0.07f, col(C_WARN));
    DrawRectangleLinesEx((Rectangle){ x + u * 0.2f, y + u * 0.2f, box - u * 0.4f, box - u * 0.4f }, u * 0.03f, col(C_WARN));
    for (int i = 0; i < 4; i++)
        DrawCircleV((Vector2){ x + (i & 1 ? box : 0), y + (i & 2 ? box : 0) }, u * 0.14f, col(C_ACCENT));
    Font f = fonts[th->body];
    float size = u * 3.0f, w = MeasureTextEx(f, s, size, 0).x;
    DrawTextEx(f, s, (Vector2){ x + (box - w) / 2, y + (box - size) / 2 - u * 0.1f }, size, 0, col(C_ACCENT));
    return box;
}

// Title and cue; returns the y below them.
static float stage_act;         // a minigame's progress, for the picture above it

static void draw_stage(const Scene *s, Rectangle box) {
    StagePal p = { C_BG, C_PANEL, C_FAINT, C_DIM, C_TEXT, C_CUE, C_OK, C_BAD, C_WARN, C_ACCENT, alpha, font, bold, stage_act };
    double t = now();
    stage_draw(s->stage, s->props, s->show, box, t - stage_since, s->show ? t - show_since : 0, &p);
}

static float draw_situation(const Scene *s) {
    float y = u * (th_id == TH_ORNATE ? 5.6f : 4.4f), x = L, w = W, top = y, cap = 0;
    float title_size = u * 1.2f, cue_size = u * 1.0f;
    const char *title = s->title;
    if (s->stage) {             // the picture on top, the words become its caption
        float a = alpha;
        alpha = 1;              // the picture stays while the text around it changes
        Rectangle box = { L, u * 3.6f, W, u * 10.6f };
        draw_stage(s, box);
        alpha = a;
        y = top = box.y + box.height + u * 0.8f;
        title_size = u * 0.95f;
        cue_size = u * 0.82f;
    }
    if (th_id == TH_ORNATE && !s->stage && isalpha((unsigned char)title[0])) {
        cap = drop_cap((char)toupper((unsigned char)title[0]), L, y);
        title++;
        x = L + cap + u * 0.7f;
        w = W - cap - u * 0.7f;
    }
    char tbuf[512];
    if (th_id == TH_TERMINAL) { snprintf(tbuf, sizeof tbuf, "%s", title); title = tbuf; }
    y += wrapped(bold, title, x, y, w, title_size, C_TEXT, 1);
    if (s->detail) {
        y += u * 0.4f;
        float cx = x + u * 0.8f, cw = w - u * 0.8f;
        float h = wrapped(cuefont, s->detail, cx, y, cw, cue_size, C_CUE, 0);
        if (th_id == TH_TERMINAL) {
            text(font, "!", x, y, u * 1.0f, C_WARN);
        } else if (th_id == TH_ORNATE) {
            DrawRectangleRec((Rectangle){ x, y + u * 0.1f, u * 0.08f, h - u * 0.3f }, col(C_ACCENT));
        } else {
            DrawRectangleRec((Rectangle){ x, y + u * 0.1f, u * 0.14f, h - u * 0.3f }, col(C_WARN));
        }
        wrapped(cuefont, s->detail, cx, y, cw, cue_size, C_CUE, 1);
        y += h;
    }
    if (cap && y < top + cap) y = top + cap;
    return y;
}

static void press(const char *s) {
    float size = u * 0.62f;
    double t = now();
    long r = reveal;
    reveal = -1;
    Color c = ColorAlpha(C_DIM, 0.55f + 0.45f * (float)(0.5 + 0.5 * sin(t * 3.0)));
    float y = (float)GetScreenHeight() - u * (th_id == TH_ORNATE ? 3.0f : 2.2f);
    text(font, s, L, y, size, c);
    reveal = r;
}

/* ---------- screens ---------- */

static void draw_menu(void) {
    const MenuView *v = &view.menu;
    float y = u * 4;
    text(bold, "typed_stories", L, y, u * 2.4f, C_TEXT);
    y += u * 3.0f;
    text(font, "a typing game of short stories. answer in your own words.", L, y, u * 0.7f, C_DIM);
    y += u * 2.6f;
    for (int i = 0; i < v->npacks; i++) {
        char key[8], rec[80] = "";
        snprintf(key, sizeof key, "%d", i + 1);
        float w = chip(key, L, y, u * 0.75f, C_ACCENT, C_ACCENT, NONE);
        // each story's name in its own look
        int t = theme_of(v->packs[i].style);
        const Theme *pt = &themes[t];
        Font f = fonts[pt->head];
        float size = u * 0.95f * pt->scale, dy = t == TH_TERMINAL ? -u * 0.2f : t == TH_ORNATE ? -u * 0.12f : 0;
        DrawTextEx(f, v->packs[i].name, (Vector2){ L + w + u * 0.8f, y + u * 0.15f + dy }, size, size * 0.02f, col(pt->menu));
        if (v->packs[i].best >= 0) snprintf(rec, sizeof rec, "best %d", v->packs[i].best);
        if (v->packs[i].today >= 0)
            snprintf(rec + strlen(rec), sizeof rec - strlen(rec), "%stoday %d", *rec ? "   " : "", v->packs[i].today);
        if (*rec) text(font, rec, L + W - text_w(font, rec, u * 0.7f), y + u * 0.3f, u * 0.7f, C_DIM);
        y += u * 1.9f;
    }
    y += u * 0.8f;
    float w = chip("T", L, y, u * 0.75f, C_DIM, C_FAINT, NONE);
    char mode[160];
    if (v->today_mode) snprintf(mode, sizeof mode, "today: the same day for everyone, %s", v->date);
    else snprintf(mode, sizeof mode, "free play: a random day");
    text(font, mode, L + w + u * 0.8f, y + u * 0.2f, u * 0.8f, v->today_mode ? C_WARN : C_DIM);
    y += u * 1.6f;
    w = chip("Q", L, y, u * 0.75f, C_DIM, C_FAINT, NONE);
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
    float y = u * (th_id == TH_ORNATE ? 5.0f : 4.4f);
    if (th_id == TH_ORNATE && th->cap >= 0) {
        float s = u * 2.6f;
        y += wrapped(fonts[th->cap], v->pack, L, y, W, s / th->scale, C_ACCENT, 1);
        divider(L + W / 2, y + u * 0.4f, W / 2);
        y += u * 1.6f;
    } else {
        if (th_id == TH_TERMINAL) text(font, "boot: loading story", L, y - u * 1.4f, u * 0.7f, C_DIM);
        y += wrapped(bold, v->pack, L, y, W, u * 1.8f, C_TEXT, 1) + u * 0.6f;
    }
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
        char h[128];
        snprintf(h, sizeof h, th_id == TH_TERMINAL ? "--%s" : "%s", v->hints[i]);
        float w = text_w(font, h, size) + size * 1.1f;
        if (x > L && x + w > L + W) { x = L; y += size * 2.1f; }
        x += chip(h, x, y, size, C_DIM, C_FAINT, NONE) + u * 0.5f;
    }
    y += size * 2.6f;

    // the input line shakes for a moment after a miss (the terminal glitches instead)
    double since = now() - msg_since;
    float shake = since < SHAKE_S && th_id != TH_TERMINAL ? (float)(sin(since * 70.0) * u * 0.35 * (1.0 - since / SHAKE_S)) : 0;
    Rectangle box = { L + shake, y, W, u * 2.0f };
    if (th_id == TH_ORNATE) {
        DrawRectangleRec(box, col(ColorAlpha(C_PANEL, 0.75f)));
        DrawLineEx((Vector2){ box.x, box.y + box.height }, (Vector2){ box.x + box.width, box.y + box.height }, u * 0.06f, col(*v->msg ? C_BAD : C_WARN));
    } else {
        DrawRectangleRounded(box, th->round * 0.5f, 8, col(C_PANEL));
        if (*v->msg) {
            if (th->round > 0) DrawRectangleRoundedLinesEx(box, th->round * 0.5f, 8, 2, col(C_BAD));
            else DrawRectangleLinesEx(box, 2, col(C_BAD));
        }
    }
    float ts = u * 1.05f, tx = box.x + u * 0.8f, ty = box.y + (box.height - ts) / 2;
    text(bold, th->prompt, tx, ty, ts, C_ACCENT);
    tx += text_w(bold, th->prompt, ts) + text_w(bold, " ", ts);
    text(font, v->input, tx, ty, ts, C_TEXT);
    if (fmod(now(), 1.0) < 0.6) {
        float cx = tx + text_w(font, v->input, ts) + (*v->input ? spacing(ts) : 0);
        float cw = th_id == TH_ORNATE ? ts * 0.08f : th_id == TH_TERMINAL ? ts * 0.55f : ts * 0.5f;
        DrawRectangleRec((Rectangle){ cx, ty + ts * 0.05f, cw, ts * 0.95f }, col(ColorAlpha(C_TEXT, 0.7f)));
    }
    y += box.height + u * 0.7f;

    // time
    float frac = (float)(v->left / v->limit);
    if (frac < 0) frac = 0;
    Color tc = frac < 0.3f ? C_BAD : frac < 0.6f ? C_WARN : C_OK;
    char secs[32];
    snprintf(secs, sizeof secs, "%.1fs", v->left > 0 ? v->left : 0);
    float sw = text_w(font, secs, u * 0.62f) + u * 0.6f;
    if (th_id == TH_TERMINAL) {         // [#########.......]
        int cells = (int)((W - sw) / (u * 0.5f)), full = (int)(cells * frac + 0.5f);
        for (int i = 0; i < cells; i++)
            DrawRectangle((int)(L + i * u * 0.5f), (int)y, (int)(u * 0.36f), (int)(u * 0.36f), col(i < full ? tc : C_FAINT));
    } else {
        DrawRectangleRounded((Rectangle){ L, y, W - sw, u * 0.28f }, th->round > 0 ? 1 : 0, 6, col(C_FAINT));
        DrawRectangleRounded((Rectangle){ L, y, (W - sw) * frac, u * 0.28f }, th->round > 0 ? 1 : 0, 6, col(tc));
    }
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
        snprintf(q, sizeof q, th_id == TH_TERMINAL ? "$ %s" : "\"%s\"", v->typed);
        y += wrapped(th_id == TH_ORNATE ? cuefont : bold, q, L, y, W, u * 1.15f, C_OK, 1) + u * 0.5f;
    } else {
        text(bold, th->timeout, L, y, u * 1.15f, C_BAD);
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
            x += chip(s, x, y, u * 0.7f, c, c, NONE) + u * 0.5f;
        }
        y += u * 1.6f;
    }
    if (v->typed) {
        y += u * 0.9f;
        float box = u * 1.9f;
        Color rc = rank_color(v->rank);
        if (th_id == TH_ORNATE) {           // a wax seal
            DrawCircleV((Vector2){ L + box / 2, y + box / 2 }, box * 0.55f, col(ColorAlpha(C_ACCENT, 0.9f)));
            DrawRing((Vector2){ L + box / 2, y + box / 2 }, box * 0.4f, box * 0.44f, 0, 360, 48, col(ColorAlpha(C_BG, 0.5f)));
            rc = C_BG;
        } else if (th->round > 0) {
            DrawRectangleRoundedLinesEx((Rectangle){ L, y, box, box }, 0.3f, 8, 2.5f, col(rc));
        } else {
            DrawRectangleLinesEx((Rectangle){ L, y, box, box }, 2.5f, col(rc));
        }
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
        if (v->typo) x += chip("typo: half style", x, ny, u * 0.55f, C_DIM, C_FAINT, NONE) + u * 0.4f;
        if (v->repeated) x += chip("heard that before", x, ny, u * 0.55f, C_DIM, C_FAINT, NONE) + u * 0.4f;
        if (v->word_bonus) x += chip("WORD OF THE DAY +100", x, ny, u * 0.55f, C_WARN, C_WARN, NONE) + u * 0.4f;
        if (v->word_again) chip("word already used today", x, ny, u * 0.55f, C_DIM, C_FAINT, NONE);
    }
    press(th->any_key);
}

static void draw_ending(void) {
    char up[64], line[160];
    upcase(up, sizeof up, view.ending.meter);
    if (th_id == TH_TERMINAL) snprintf(line, sizeof line, "FATAL: %s = 0", up);
    else if (th_id == TH_ORNATE) snprintf(line, sizeof line, "%s is spent", view.ending.meter);
    else snprintf(line, sizeof line, "%s HIT ZERO", up);
    float y = u * 6;
    text(bold, line, L, y, u * 1.8f, C_BAD);
    y += u * 2.6f;
    text(font, th_id == TH_ORNATE ? "And so the tale ends early." : th_id == TH_TERMINAL ? "core dumped. session terminated." : "The day is over.",
         L, y, u * 0.9f, C_DIM);
    y += u * 2.0f;
    wrapped(font, view.ending.text, L, y, W, u * 1.0f, C_TEXT, 1);
    press(th->any_key);
}

static void draw_finale(void) {
    const FinaleView *v = &view.finale;
    Color mood = v->special ? C_WARN : v->good == 2 ? C_OK : v->good == 0 ? C_BAD : C_ACCENT;
    float y = u * 5.5f, tw = th_id == TH_ABSTRACT ? W * 0.6f : W;
    if (th_id == TH_TERMINAL) {
        char line[96];
        snprintf(line, sizeof line, "%s with code %d", th->the_end, v->special ? 42 : v->good == 2 ? 0 : 1);
        text(font, line, L, y, u * 0.8f, C_DIM);
    } else {
        text(th_id == TH_ORNATE ? font : bold, th->the_end, L, y, u * 0.8f, th_id == TH_ORNATE ? C_DIM : mood);
    }
    y += u * 1.6f;
    if (th_id == TH_ORNATE && th->cap >= 0) {
        y += wrapped(fonts[th->cap], v->title, L, y, tw, u * 2.4f / th->scale, mood, 1);
        divider(L + W / 2, y + u * 0.5f, W / 2);
        y += u * 2.0f;
    } else {
        char t[200];
        if (th_id == TH_TERMINAL) snprintf(t, sizeof t, "> %s", v->title);
        else upcase(t, sizeof t, v->title);
        y += wrapped(bold, t, L, y, tw, u * 1.8f, mood, 1) + u * 1.0f;
    }
    wrapped(font, v->text, L, y, tw, u * 1.05f, C_TEXT, 1);
    press(th->any_key);
}

static void stat_row(const char *label, const char *value, float y, Color c) {
    text(font, label, L, y, u * 0.75f, C_DIM);
    text(font, value, L + u * 7, y, u * 0.75f, c);
}

static void draw_summary(void) {
    const SummaryView *v = &view.sum;
    float y = u * (th_id == TH_ORNATE ? 3.2f : 2.6f);
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
    chip(s, x, y + u * 0.2f, u * 0.6f, v->new_best ? C_WARN : C_DIM, v->new_best ? C_WARN : C_FAINT, NONE);
    if (v->today_mode) {
        y += u * 2.0f;
        snprintf(s, sizeof s, "best %d%s", v->today_best, v->new_day ? "  (that's you)" : "");
        stat_row("today", s, y, C_WARN);
    }
    press(th->any_key);
}

/* ---------- the store from above ---------- */

static Color aisle_color(int n) {
    Color pal[5] = { C_ACCENT, C_WARN, C_OK, C_BAD, C_CUE };
    return pal[n % 5];
}

static void draw_walk(void) {
    const WalkView *v = &view.walk;
    double t = now();
    draw_header(&v->scene);
    char line[160], up[64];
    upcase(up, sizeof up, v->goal_name);
    snprintf(line, sizeof line, "GET TO %s", up);
    float y = u * 4.0f;
    text(bold, line, L, y, u * 1.0f, C_WARN);
    snprintf(line, sizeof line, "you said \"%s\"     WASD or arrows", v->answer);
    text(font, line, L + W - text_w(font, line, u * 0.65f), y + u * 0.25f, u * 0.65f, C_DIM);

    // the map, as big as fits
    float top = u * 5.8f, avail_h = u * 18.0f;
    float cs = W / v->w;
    if (cs > avail_h / v->h) cs = avail_h / v->h;
    float mx = L + (W - cs * v->w) / 2, my = top;
    DrawRectangleRounded((Rectangle){ mx - cs * 0.2f, my - cs * 0.2f, cs * (v->w + 0.4f), cs * (v->h + 0.4f) }, 0.04f, 8, col(C_PANEL));
    for (int y_ = 0; y_ < v->h; y_++)
        for (int x_ = 0; x_ < v->w; x_++) {
            char c = v->rows[y_][x_];
            Rectangle r = { mx + x_ * cs, my + y_ * cs, cs, cs };
            int border = x_ == 0 || y_ == 0 || x_ == v->w - 1 || y_ == v->h - 1;
            if (c == '#' && border) {
                DrawRectangleRec(r, col(C_FAINT));
            } else if (c == '#') {          // a shelf, colored like its aisle's goods
                Color sc = aisle_color(x_ / 3);
                DrawRectangleRec((Rectangle){ r.x + cs * 0.12f, r.y, cs * 0.76f, cs }, col(ColorAlpha(sc, 0.35f)));
                for (int i = 0; i < 2; i++)
                    DrawCircleV((Vector2){ r.x + cs * (0.33f + i * 0.34f), r.y + cs * 0.5f }, cs * 0.12f, col(ColorAlpha(sc, 0.8f)));
            } else if (c >= '1' && c <= '9') {      // the goods of an aisle
                int goal = c == v->goal;
                float pulse = goal ? 0.35f + 0.25f * (float)sin(t * 6) : 0.12f;
                DrawRectangleRec(r, col(ColorAlpha(goal ? C_WARN : C_DIM, pulse)));
            } else if (c == 'K') {
                DrawRectangleRounded((Rectangle){ r.x + 1, r.y + cs * 0.1f, cs - 2, cs * 0.8f }, 0.3f, 4, col(C_DIM));
                DrawRectangle((int)(r.x + cs * 0.15f), (int)(r.y + cs * 0.45f), (int)(cs * 0.7f), (int)(cs * 0.1f), col(C_PANEL));
            } else if (c == 'k') {
                if (c == v->goal) DrawRectangleRec(r, col(ColorAlpha(C_WARN, 0.3f + 0.2f * (float)sin(t * 6))));
            } else if (c == 'i') {
                DrawCircleV((Vector2){ r.x + cs / 2, r.y + cs / 2 }, cs * 0.45f, col(C_ACCENT));
                text(bold, "i", r.x + cs * 0.38f, r.y + cs * 0.15f, cs * 0.7f, C_PANEL);
            }
        }
    // aisle numbers over each aisle
    for (int n = 1; n <= 8; n++) {
        char num[4];
        snprintf(num, sizeof num, "%d", n);
        float ax = mx + (3 * n + 2) * cs, size = cs * 0.9f;
        int goal = v->goal == '0' + n;
        text(bold, num, ax - text_w(bold, num, size) / 2, my + 1.5f * cs, size, goal ? C_WARN : C_DIM);
    }
    text(font, "IN", mx + 1.1f * cs, my + (v->h - 0.85f) * cs, cs * 0.6f, C_DIM);
    // a bouncing arrow over the goal
    for (int y_ = 0; y_ < v->h; y_++) {
        const char *row = v->rows[y_];
        const char *p = strchr(row, v->goal);
        if (!p) continue;
        float gx = mx + (float)(p - row + 1) * cs, gy = my + y_ * cs - cs * (0.6f + 0.25f * (float)sin(t * 5));
        DrawPoly((Vector2){ gx, gy }, 3, cs * 0.4f, 90, col(C_WARN));
        break;
    }
    // carts slide from cell to cell
    for (int i = 0; i < v->ncarts; i++) {
        float f = (float)((t - v->cart_moved[i]) / CART_STEP_S);
        if (f > 1) f = 1;
        if (f < 0) f = 0;
        float cx = mx + (v->cart_px[i] + (v->cart_x[i] - v->cart_px[i]) * f) * cs, cy = my + v->cart_y[i] * cs;
        Rectangle body = { cx + cs * 0.12f, cy + cs * 0.2f, cs * 0.76f, cs * 0.55f };
        DrawRectangleRoundedLinesEx(body, 0.3f, 4, cs * 0.08f, col(C_TEXT));
        float hx = v->cart_dx[i] > 0 ? body.x : body.x + body.width;
        DrawLineEx((Vector2){ hx, body.y }, (Vector2){ hx - v->cart_dx[i] * cs * 0.18f, body.y - cs * 0.15f }, cs * 0.08f, col(C_TEXT));
        DrawCircleV((Vector2){ body.x + cs * 0.15f, body.y + body.height + cs * 0.08f }, cs * 0.08f, col(C_TEXT));
        DrawCircleV((Vector2){ body.x + body.width - cs * 0.15f, body.y + body.height + cs * 0.08f }, cs * 0.08f, col(C_TEXT));
    }
    // you, sliding from cell to cell
    float f = (float)((t - v->moved_at) / WALK_STEP_S);
    if (f > 1) f = 1;
    if (f < 0) f = 0;
    float px = mx + (v->ppx + (v->px - v->ppx) * f + 0.5f) * cs, py = my + (v->ppy + (v->py - v->ppy) * f + 0.5f) * cs;
    if (v->bumped) DrawRing((Vector2){ px, py }, cs * 0.5f, cs * 0.62f, 0, 360, 32, col(C_BAD));
    DrawCircleV((Vector2){ px, py }, cs * 0.38f, col(C_ACCENT));
    DrawCircleV((Vector2){ px, py }, cs * 0.16f, col(C_PANEL));

    // time
    float ty = my + cs * (v->h + 0.6f);
    float frac = (float)(v->left / v->limit);
    Color tc = frac < 0.3f ? C_BAD : frac < 0.6f ? C_WARN : C_OK;
    char secs[32];
    snprintf(secs, sizeof secs, "%.1fs", v->left);
    float sw = text_w(font, secs, u * 0.62f) + u * 0.6f;
    DrawRectangleRounded((Rectangle){ L, ty, W - sw, u * 0.28f }, 1, 6, col(C_FAINT));
    DrawRectangleRounded((Rectangle){ L, ty, (W - sw) * frac, u * 0.28f }, 1, 6, col(tc));
    text(font, secs, L + W - sw + u * 0.6f, ty - u * 0.2f, u * 0.62f, tc);
    if (v->bumped) text(bold, "Bump! A cart.", L, ty + u * 0.8f, u * 0.8f, C_BAD);
}

/* ---------- minigames ---------- */

static void time_bar(float y, double left, double limit) {
    float frac = (float)(left / limit);
    if (frac < 0) frac = 0;
    Color tc = frac < 0.3f ? C_BAD : frac < 0.6f ? C_WARN : C_OK;
    char secs[32];
    snprintf(secs, sizeof secs, "%.1fs", left);
    float sw = text_w(font, secs, u * 0.62f) + u * 0.6f;
    DrawRectangleRounded((Rectangle){ L, y, W - sw, u * 0.28f }, 1, 6, col(C_FAINT));
    DrawRectangleRounded((Rectangle){ L, y, (W - sw) * frac, u * 0.28f }, 1, 6, col(tc));
    text(font, secs, L + W - sw + u * 0.6f, y - u * 0.2f, u * 0.62f, tc);
}

// A road from above, or an apple tree: one grid, cells of size cs.
static void draw_grid(const MinigameView *v, float top, float avail) {
    double t = now();
    float cs = W / v->w;
    if (cs > avail / v->h) cs = avail / v->h;
    float mx = L + (W - cs * v->w) / 2, my = top;
    Rectangle area = { mx, my, cs * v->w, cs * v->h };
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    if (v->kind == MG_CROSS) {
        DrawRectangleRec(area, col(C_BG));
        for (int y = 0; y < v->h; y++) {
            Rectangle row = { mx, my + y * cs, area.width, cs };
            if (!v->lane_dir[y]) {      // sidewalks
                DrawRectangleRec(row, col(C_FAINT));
                for (float x = mx; x < mx + area.width; x += cs) DrawLineEx((Vector2){ x, row.y }, (Vector2){ x, row.y + cs }, 1, col(C_PANEL));
                continue;
            }
            for (float x = mx; x < mx + area.width; x += cs * 1.5f)
                DrawRectangle((int)x, (int)(row.y + row.height - 2), (int)(cs * 0.7f), 2, col(ColorAlpha(C_DIM, 0.5f)));
            // traffic slides between cells; the lane shifts when 'next' comes
            float f = 1 - (float)(v->lane_next[y] / v->lane_step[y]);
            if (f < 0) f = 0;
            if (f > 1) f = 1;
            float off = f * v->lane_dir[y] * cs;
            for (int rep = -1; rep <= 1; rep++)
                for (int x = 0; x < v->w; x++) {
                    char c = v->cells[y][x];
                    float cx = mx + (x + rep * v->w) * cs + off;
                    if (c == 'c') {
                        int head = v->cells[y][(x + 1) % v->w] == 'c' && v->cells[y][(x + v->w - 1) % v->w] != 'c';
                        if (!head) continue;        // a car is two cells: draw it once, from its left half
                        Color cc = (y % 3 == 0) ? C_BAD : (y % 3 == 1) ? C_ACCENT : C_WARN;
                        DrawRectangleRounded((Rectangle){ cx + cs * 0.08f, row.y + cs * 0.15f, cs * 1.84f, cs * 0.7f }, 0.4f, 6, col(cc));
                        DrawRectangle((int)(cx + cs * (v->lane_dir[y] > 0 ? 1.2f : 0.4f)), (int)(row.y + cs * 0.25f), (int)(cs * 0.4f), (int)(cs * 0.5f), col(ColorAlpha(C_BG, 0.6f)));
                    } else if (c == 'b') {
                        DrawRing((Vector2){ cx + cs * 0.28f, row.y + cs * 0.6f }, cs * 0.12f, cs * 0.2f, 0, 360, 16, col(C_OK));
                        DrawRing((Vector2){ cx + cs * 0.72f, row.y + cs * 0.6f }, cs * 0.12f, cs * 0.2f, 0, 360, 16, col(C_OK));
                        DrawCircleV((Vector2){ cx + cs * 0.5f, row.y + cs * 0.28f }, cs * 0.16f, col(C_CUE));
                    }
                }
        }
    } else {                            // catch: a tree above, apples falling
        DrawRectangleRec(area, col(C_PANEL));
        DrawCircleV((Vector2){ mx + area.width / 2, my - cs * 0.5f }, area.width * 0.45f, col(ColorAlpha(C_OK, 0.25f)));
        for (int y = 0; y < v->h; y++)
            for (int x = 0; x < v->w; x++)
                if (v->cells[y][x] == 'o') {
                    Vector2 c = { mx + (x + 0.5f) * cs, my + (y + 0.5f) * cs };
                    DrawCircleV(c, cs * 0.3f, col(C_BAD));
                    DrawLineEx((Vector2){ c.x, c.y - cs * 0.3f }, (Vector2){ c.x + cs * 0.08f, c.y - cs * 0.45f }, 2, col(C_OK));
                }
    }
    // you: a dot crossing the road, or a basket
    float f = (float)(v->moved_ago / MG_STEP_S);
    if (f > 1) f = 1;
    if (f < 0) f = 0;
    float px = mx + (v->ppx + (v->px - v->ppx) * f + 0.5f) * cs, py = my + (v->ppy + (v->py - v->ppy) * f + 0.5f) * cs;
    if (v->kind == MG_CROSS) {
        DrawCircleV((Vector2){ px, py }, cs * 0.34f, col(C_ACCENT));
        DrawCircleV((Vector2){ px, py }, cs * 0.14f, col(C_PANEL));
        if (v->state == MG_LOST)
            for (int i = 0; i < 8; i++) {
                float a = i * 45 * DEG2RAD;
                DrawLineEx((Vector2){ px + cosf(a) * cs * 0.45f, py + sinf(a) * cs * 0.45f }, (Vector2){ px + cosf(a) * cs * 0.8f, py + sinf(a) * cs * 0.8f }, 3, col(C_BAD));
            }
    } else {
        DrawRectangleRounded((Rectangle){ px - cs * 0.48f, py - cs * 0.1f, cs * 0.96f, cs * 0.55f }, 0.5f, 6, col((Color){ 150, 110, 70, 255 }));
        DrawRing((Vector2){ px, py - cs * 0.1f }, cs * 0.3f, cs * 0.36f, 180, 360, 16, col((Color){ 150, 110, 70, 255 }));
    }
    EndScissorMode();
    (void)t;
}

static void draw_mini(void) {
    const MinigameView *v = &view.mini;
    double t = now();
    static const char *const how[] = {
        "mash any key!", "press Space when the needle is in the green", "W A S D: get across", "A D: catch them", "type the words",
    };
    draw_header(&v->scene);
    float y;
    if (v->kind == MG_CROSS || v->kind == MG_CATCH) {
        y = u * 3.9f;
        text(bold, v->text, L, y, u * 0.95f, C_WARN);
        text(font, how[v->kind], L + W - text_w(font, how[v->kind], u * 0.65f), y + u * 0.25f, u * 0.65f, C_DIM);
        draw_grid(v, u * 5.6f, u * 17.0f);
        y = u * 23.6f;
        if (v->kind == MG_CATCH) {
            char c[48];
            snprintf(c, sizeof c, "caught %d of %d", v->caught, v->n);
            text(bold, c, L, y - u * 1.2f, u * 0.8f, C_TEXT);
        }
    } else {
        float a = alpha;
        alpha = 1;
        stage_act = (float)v->progress;
        if (v->scene.stage) draw_stage(&v->scene, (Rectangle){ L, u * 3.6f, W, u * 10.6f });
        stage_act = 0;
        alpha = a;
        y = u * (v->scene.stage ? 15.0f : 5.0f);
        if (v->kind != MG_LYRICS) text(bold, v->text, L, y, u * 1.0f, C_WARN);
        text(font, how[v->kind], L + W - text_w(font, how[v->kind], u * 0.65f), y + u * 0.25f, u * 0.65f, C_DIM);
        y += u * 1.9f;
        if (v->kind == MG_MASH) {           // a key that throbs, a bar that leaks
            float pulse = 1 + 0.06f * (float)sin(t * 22);
            float kw = u * 6 * pulse, kh = u * 2.2f * pulse;
            Rectangle key = { L, y, kw, kh };
            DrawRectangleRounded(key, 0.3f, 8, col(C_PANEL));
            DrawRectangleRoundedLinesEx(key, 0.3f, 8, 3, col(C_ACCENT));
            text(bold, "ANY KEY", key.x + (kw - text_w(bold, "ANY KEY", u * 0.9f)) / 2, key.y + (kh - u * 0.9f) / 2, u * 0.9f, C_ACCENT);
            Rectangle bar = { L + u * 7, y + u * 0.6f, W - u * 7, u * 1.0f };
            DrawRectangleRounded(bar, 0.5f, 8, col(C_FAINT));
            DrawRectangleRounded((Rectangle){ bar.x, bar.y, bar.width * (float)v->progress, bar.height }, 0.5f, 8, col(v->progress > 0.7 ? C_OK : C_WARN));
            y += u * 3.2f;
        } else if (v->kind == MG_TIMING) {  // a needle swings over a track
            Rectangle tr = { L, y + u * 0.6f, W, u * 1.4f };
            float shake = v->missed_ago < 0.3 ? (float)sin(v->missed_ago * 80) * u * 0.3f : 0;
            tr.x += shake;
            DrawRectangleRounded(tr, 0.4f, 8, col(C_PANEL));
            DrawRectangleRec((Rectangle){ tr.x + tr.width * (float)v->zone_lo, tr.y, tr.width * (float)(v->zone_hi - v->zone_lo), tr.height }, col(ColorAlpha(C_OK, 0.6f)));
            float nx = tr.x + tr.width * (float)v->needle;
            DrawRectangle((int)(nx - u * 0.08f), (int)(tr.y - u * 0.4f), (int)(u * 0.16f), (int)(tr.height + u * 0.8f), col(v->missed_ago < 0.3 ? C_BAD : C_TEXT));
            DrawPoly((Vector2){ nx, tr.y - u * 0.6f }, 3, u * 0.3f, 90, col(C_TEXT));
            for (int i = 0; i < v->n; i++)
                DrawCircleV((Vector2){ L + W - u * 0.4f - i * u * 0.9f, y - u * 0.1f }, u * 0.25f, col(i < v->tries ? C_WARN : C_FAINT));
            y += u * 3.0f;
        } else {                            // the words to sing
            float size = u * 1.4f, shake = v->wrong_ago < 0.25 ? (float)sin(v->wrong_ago * 90) * u * 0.25f : 0;
            char done[MG_TEXT];
            snprintf(done, sizeof done, "%.*s", v->typed, v->text);
            float tw = text_w(bold, v->text, size), x0 = L + (W - tw) / 2 + shake;
            text(bold, v->text, x0, y, size, C_DIM);
            text(bold, done, x0, y, size, C_OK);
            float cx = x0 + text_w(bold, done, size);
            if (v->text[v->typed])
                DrawRectangle((int)cx, (int)(y + size * 1.15f), (int)(size * 0.55f), (int)(u * 0.12f), col(v->wrong_ago < 0.25 ? C_BAD : C_WARN));
            y += u * 2.8f;
        }
    }
    time_bar(y, v->left, v->limit);
    if (v->state != MG_GOING) {
        const char *s = v->state == MG_WON ? "DONE!" : "MISSED!";
        float size = u * 2.2f * (1 + 0.3f * (float)(1 - fmin(1, v->ended_ago / 0.25)));
        text(bold, s, L + (W - text_w(bold, s, size)) / 2, y + u * 0.9f, size, v->state == MG_WON ? C_OK : C_BAD);
    }
}

static void draw_screen(void) {
    // the terminal types things out, except where the player is racing the clock
    reveal = -1;
    if (th_id == TH_TERMINAL && (screen == S_INTRO || screen == S_DAY || screen == S_FINALE || screen == S_ENDING))
        reveal = (long)((now() - screen_since) * 700.0);

    // shapes in the background, except where a picture is the background's job
    int pictured = (screen == S_INTRO && view.intro.stage) || (screen == S_PROMPT && view.prompt.scene.stage) ||
                   (screen == S_OUTCOME && view.out.scene.stage) || screen == S_WALK || screen == S_MINI;
    if (th_id == TH_ABSTRACT && screen != S_MENU && screen != S_FINALE && !pictured) abstract_backdrop(mood_key);
    if (th_id == TH_ABSTRACT && screen == S_FINALE) abstract_finale(view.finale.good, view.finale.special);
    if (th_id == TH_ORNATE) ornate_backdrop();
    if (th_id == TH_TERMINAL) terminal_backdrop();
    if (screen != S_MENU) {
        float a = alpha;
        long r = reveal;
        alpha = 1;              // the hud does not fade with the screen
        reveal = -1;
        draw_hud();
        alpha = a;
        reveal = r;
    }
    switch (screen) {
    case S_MENU:    draw_menu();    break;
    case S_DAY:     draw_day();     break;
    case S_INTRO:   draw_intro();   break;
    case S_PROMPT:  draw_prompt();  break;
    case S_OUTCOME: draw_outcome(); break;
    case S_ENDING:  draw_ending();  break;
    case S_FINALE:  draw_finale();  break;
    case S_SUMMARY: draw_summary(); break;
    case S_WALK:    draw_walk();    break;
    case S_MINI:    draw_mini();    break;
    default: break;
    }
    reveal = -1;
}

static void frame(void) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    u = (float)sh / 28.0f;
    float margin = th_id == TH_ORNATE ? u * 9 : u * 4;
    W = (float)sw - margin;
    if (W > u * 44) W = u * 44;
    L = ((float)sw - W) / 2;
    double t = (now() - screen_since) / FADE_S;
    alpha = t >= 1 ? 1.0f : t <= 0 ? 0.0f : (float)t;

    if (th_id != TH_TERMINAL) {
        BeginDrawing();
        ClearBackground(C_BG);
        draw_screen();
        EndDrawing();
        return;
    }
    if (!crt.id || crt_w != sw || crt_h != sh) {
        if (crt.id) UnloadRenderTexture(crt);
        crt = LoadRenderTexture(sw, sh);
        crt_w = sw;
        crt_h = sh;
    }
    BeginTextureMode(crt);
    ClearBackground(C_BG);
    draw_screen();
    EndTextureMode();

    // a miss glitches hard; now and then the screen twitches by itself
    double since = now() - msg_since;
    float glitch = since < SHAKE_S ? (float)(1.0 - since / SHAKE_S) : 0;
    if (fmod(now(), 7.3) < 0.07) glitch = 0.25f;
    BeginDrawing();
    ClearBackground(C_BG);
    terminal_present(glitch);
    terminal_overlay();
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
        if (!(k >= KEY_LEFT_SHIFT && k <= KEY_KB_MENU)) presses++;
        if (k == KEY_ENTER || k == KEY_KP_ENTER) { if (alt) toggle_fullscreen(); else push(K_ENTER); }
        else if (k == KEY_F11) toggle_fullscreen();
        else if (k == KEY_BACKSPACE) push(ctrl ? K_CTRL_W : K_BACKSPACE);
        else if (ctrl && k == KEY_W) push(K_CTRL_W);
        else if (ctrl && k == KEY_U) push(K_CTRL_U);
        else if (k == KEY_UP)    push(K_UP);
        else if (k == KEY_DOWN)  push(K_DOWN);
        else if (k == KEY_LEFT)  push(K_LEFT);
        else if (k == KEY_RIGHT) push(K_RIGHT);
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

// Fonts live in assets/fonts, in the working directory or next to the program.
static Font load_font(const char *file) {
    char path[1024];
    snprintf(path, sizeof path, "assets/fonts/%s", file);
    if (!FileExists(path)) snprintf(path, sizeof path, "%sassets/fonts/%s", GetApplicationDirectory(), file);
    if (!FileExists(path)) return GetFontDefault();
    Font f = LoadFontEx(path, FONT_PX, NULL, 0);
    GenTextureMipmaps(&f.texture);
    SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
    return f;
}

static const char *const font_files[F_COUNT] = {
    [F_MONO] = "JetBrainsMono-Regular.ttf",      [F_MONO_BOLD] = "JetBrainsMono-Bold.ttf",
    [F_FELL] = "IMFellEnglish-Regular.ttf",      [F_FELL_IT] = "IMFellEnglish-Italic.ttf",
    [F_FELL_SC] = "IMFellEnglishSC-Regular.ttf", [F_FRAKTUR] = "UnifrakturMaguntia-Book.ttf",
    [F_VT] = "VT323-Regular.ttf",
};

void ui_init(void) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(cfg.width, cfg.height, "typed_stories");
    SetWindowMinSize(800, 500);
    SetExitKey(KEY_NULL);
    if (cfg.fullscreen) ToggleBorderlessWindowed();           // Esc is a key like any other, not "quit"
    SetTargetFPS(60);
    for (int i = 0; i < F_COUNT; i++) fonts[i] = load_font(font_files[i]);
    Image noise = GenImagePerlinNoise(512, 512, 0, 0, 6.0f);
    paper = LoadTextureFromImage(noise);
    UnloadImage(noise);
    SetTextureFilter(paper, TEXTURE_FILTER_BILINEAR);
}

void ui_end(void) {
    for (int i = 0; i < F_COUNT; i++)
        if (fonts[i].texture.id != GetFontDefault().texture.id) UnloadFont(fonts[i]);
    if (paper.id) UnloadTexture(paper);
    if (crt.id) UnloadRenderTexture(crt);
    CloseWindow();
}
