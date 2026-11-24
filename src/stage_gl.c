#include <math.h>
#include <stdio.h>
#include <string.h>

#include "stage_gl.h"

static const StagePal *P;

static Color C(Color c) { return ColorAlpha(c, P->alpha * (float)c.a / 255.0f); }
static Color CA(Color c, float a) { return ColorAlpha(c, P->alpha * a); }

static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
static float ease(float x) { x = clamp01(x); return x * x * (3 - 2 * x); }
static float lerp(float a, float b, float t) { return a + (b - a) * t; }

// "dog=growl stranger=near": is dog=growl there? Is there a stranger at all?
static int prop_is(const char *props, const char *key, const char *val) {
    if (!props) return 0;
    char want[64];
    snprintf(want, sizeof want, "%s=%s", key, val);
    size_t n = strlen(want);
    for (const char *p = props; (p = strstr(p, want)); p++)
        if ((p == props || p[-1] == ' ') && (p[n] == ' ' || p[n] == '\0')) return 1;
    return 0;
}

static int has_key(const char *props, const char *key) {
    if (!props) return 0;
    char want[64];
    snprintf(want, sizeof want, "%s=", key);
    size_t n = strlen(want);
    for (const char *p = props; (p = strstr(p, want)); p++)
        if (p == props || p[-1] == ' ') return 1;
    (void)n;
    return 0;
}

static int is(const char *show, const char *s) { return show && !strcmp(show, s); }

// raylib wants triangles in one winding; take them in any.
static void tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0) DrawTriangle(a, c, b, col);
    else DrawTriangle(a, b, c, col);
}

static void limb(Vector2 a, Vector2 b, float th, Color c) {
    DrawLineEx(a, b, th, c);
    DrawCircleV(a, th / 2, c);
    DrawCircleV(b, th / 2, c);
}

static Vector2 v2(float x, float y) { return (Vector2){ x, y }; }

/* ---------- figures ---------- */

// A person: legs, a body, a head. Returns the shoulder; arms are the caller's.
static Vector2 person(float x, float gy, float k, int dir, float phase, int moving, Color c) {
    float hip = gy - 2.2f * k, a = moving ? sinf(phase) * 0.45f : 0;
    limb(v2(x, hip), v2(x + sinf(a) * 2.1f * k, gy - 0.1f * k), 0.3f * k, c);
    limb(v2(x, hip), v2(x - sinf(a) * 2.1f * k, gy - 0.1f * k), 0.3f * k, c);
    float bob = moving ? fabsf(sinf(phase)) * 0.12f * k : 0;
    DrawRectangleRounded((Rectangle){ x - 0.5f * k, hip - 2.5f * k - bob, 1.0f * k, 2.7f * k }, 0.6f, 8, c);
    DrawCircleV(v2(x + dir * 0.1f * k, hip - 3.25f * k - bob), 0.62f * k, c);
    return v2(x, hip - 2.2f * k - bob);
}

enum { DOG_WALK, DOG_WAG, DOG_GROWL, DOG_CALM, DOG_CHEW, DOG_STIFF, DOG_SIT };

// The dog faces right. Returns where the leash ties on; *mouth gets the snout's tip.
static Vector2 dog(float x, float gy, float k, int st, double t, float phase, int moving, Color c, Color bg, Vector2 *mouth) {
    float crouch = st == DOG_GROWL ? 0.18f * k : 0;
    float by = gy - 1.5f * k + crouch;
    // legs
    float lx[4] = { -0.95f, -0.55f, 0.55f, 0.95f };
    for (int i = 0; i < 4; i++) {
        float sw = moving ? sinf(phase * 1.3f + (i % 2) * PI) * 0.35f * k : 0;
        Vector2 top = v2(x + lx[i] * k, by + 0.3f * k), foot = v2(x + lx[i] * k + sw, gy - 0.08f * k);
        if (st == DOG_SIT && i < 2) foot = v2(x + lx[i] * k + 0.5f * k, gy - 0.1f * k), top.y = gy - 0.6f * k;
        if (st == DOG_STIFF && i == 3) foot = v2(x + 1.25f * k, by + 0.75f * k);       // a paw up, pointing
        limb(top, foot, 0.26f * k, c);
    }
    // body
    if (st == DOG_SIT)
        DrawRectanglePro((Rectangle){ x, by + 0.1f * k, 2.6f * k, 1.0f * k }, v2(1.3f * k, 0.5f * k), -24, c);
    else
        DrawRectangleRounded((Rectangle){ x - 1.3f * k, by - 0.5f * k, 2.6f * k, 1.0f * k }, 0.9f, 10, c);
    // head
    float hx = x + 1.6f * k, hy = gy - 2.3f * k;
    if (st == DOG_CHEW)  { hx = x + 1.7f * k; hy = gy - 0.9f * k + sinf((float)t * 10) * 0.08f * k; }
    if (st == DOG_STIFF) { hx = x + 1.8f * k; hy = gy - 2.0f * k; }
    if (st == DOG_GROWL) { hx = x + 1.65f * k + sinf((float)t * 55) * 0.03f * k; hy = gy - 1.8f * k; }
    if (st == DOG_SIT)   { hx = x + 1.35f * k; hy = gy - 2.75f * k; }
    limb(v2(x + 1.0f * k, by - 0.2f * k), v2(hx, hy), 0.55f * k, c);
    DrawCircleV(v2(hx, hy), 0.6f * k, c);
    DrawRectangleRounded((Rectangle){ hx + 0.2f * k, hy - 0.12f * k, 0.8f * k, 0.46f * k }, 0.7f, 6, c);
    DrawCircleV(v2(hx + 0.15f * k, hy - 0.2f * k), 0.09f * k, bg);
    DrawCircleV(v2(hx + 1.0f * k, hy - 0.02f * k), 0.1f * k, bg);         // nose
    *mouth = v2(hx + 0.95f * k, hy + 0.3f * k);
    // ears: up, or flat back when it growls
    if (st == DOG_GROWL)
        tri(v2(hx - 0.1f * k, hy - 0.45f * k), v2(hx - 1.05f * k, hy - 0.5f * k), v2(hx - 0.25f * k, hy - 0.1f * k), c);
    else
        tri(v2(hx - 0.35f * k, hy - 0.35f * k), v2(hx - 0.15f * k, hy - 1.15f * k), v2(hx + 0.2f * k, hy - 0.45f * k), c);
    if (st == DOG_GROWL) {          // teeth and a rumble
        for (int i = 0; i < 3; i++) {
            float tx = hx + 0.45f * k + i * 0.18f * k;
            tri(v2(tx, hy + 0.32f * k), v2(tx + 0.16f * k, hy + 0.32f * k), v2(tx + 0.08f * k, hy + 0.12f * k), bg);
        }
        for (int i = 0; i < 3; i++) {
            float a = 0.35f + 0.25f * sinf((float)t * 20 + i);
            DrawLineEx(v2(hx + 1.35f * k, hy - 0.3f * k + i * 0.35f * k), v2(hx + 1.75f * k, hy - 0.4f * k + i * 0.4f * k),
                       0.1f * k, ColorAlpha(c, a));
        }
    }
    // tail: the thing to read
    float ang, len = 1.35f * k;
    switch (st) {
    case DOG_WAG:   ang = -125 + sinf((float)t * 17) * 32; break;
    case DOG_GROWL: ang = 158; break;                           // low and still
    case DOG_STIFF: ang = -95; break;                           // straight up, rigid
    case DOG_CHEW:  ang = -135 + sinf((float)t * 8) * 14; break;
    case DOG_SIT:   ang = 175; break;
    default:        ang = -140 + sinf((float)t * 4) * 10; break;
    }
    float bx = x - 1.25f * k, byy = by - 0.25f * k, a = ang * DEG2RAD;
    limb(v2(bx, byy), v2(bx + cosf(a) * len, byy + sinf(a) * len), 0.24f * k, c);
    if (st == DOG_WAG) {            // motion marks
        for (int i = -1; i <= 1; i += 2) {
            float ma = (-125 + i * 42) * DEG2RAD;
            Vector2 p = v2(bx + cosf(ma) * len * 1.15f, byy + sinf(ma) * len * 1.15f);
            DrawLineEx(p, v2(p.x + cosf(ma) * 0.35f * k, p.y + sinf(ma) * 0.35f * k), 0.08f * k, ColorAlpha(c, 0.5f));
        }
    }
    return v2(hx - 0.35f * k, hy + 0.15f * k);
}

static void item(const char *what, float x, float y, float k, float rot) {
    if (!strcmp(what, "sock")) {
        DrawRectanglePro((Rectangle){ x, y, 0.32f * k, 0.7f * k }, v2(0.16f * k, 0.7f * k), rot, C(P->text));
        DrawRectanglePro((Rectangle){ x, y, 0.6f * k, 0.3f * k }, v2(0.16f * k, 0.3f * k), rot, C(P->text));
        DrawRectanglePro((Rectangle){ x, y - 0.5f * k, 0.32f * k, 0.12f * k }, v2(0.16f * k, 0.06f * k), rot, C(P->bad));
    } else if (!strcmp(what, "stick")) {
        DrawRectanglePro((Rectangle){ x, y, 1.5f * k, 0.16f * k }, v2(0.75f * k, 0.08f * k), rot - 8, C((Color){ 160, 116, 70, 255 }));
        DrawLineEx(v2(x + 0.2f * k, y), v2(x + 0.45f * k, y - 0.3f * k), 0.1f * k, C((Color){ 160, 116, 70, 255 }));
    } else if (!strcmp(what, "sausage")) {
        DrawRectanglePro((Rectangle){ x, y, 0.95f * k, 0.34f * k }, v2(0.47f * k, 0.17f * k), rot, C((Color){ 222, 112, 112, 255 }));
        DrawCircleV(v2(x - 0.2f * k, y), 0.05f * k, C((Color){ 110, 150, 255, 255 }));     // the blue powder
    }
}

static void heart(float x, float y, float s, Color c) {
    DrawCircleV(v2(x - s * 0.5f, y), s * 0.55f, c);
    DrawCircleV(v2(x + s * 0.5f, y), s * 0.55f, c);
    tri(v2(x - s * 1.02f, y + s * 0.15f), v2(x + s * 1.02f, y + s * 0.15f), v2(x, y + s * 1.3f), c);
}

/* ---------- the dog walk ---------- */

static float wrapf(float v, float m) { v = fmodf(v, m); return v < 0 ? v + m : v; }

static void walk_stage(const char *props, const char *show, Rectangle b, double ts, double tw) {
    float k = b.height / 11.0f, gy = b.y + b.height * 0.84f;
    double t = GetTime();
    // walking at first, then everyone stops: the thing has happened
    float x01 = clamp01((float)ts / 1.8f);
    float travel = 1.8f * (x01 - x01 * x01 / 2) * 7.0f * k;    // distance walked so far
    int moving = x01 < 1;
    float phase = travel / (0.9f * k);

    int st = DOG_WALK;
    if (prop_is(props, "dog", "wag"))   st = DOG_WAG;
    if (prop_is(props, "dog", "growl")) st = DOG_GROWL;
    if (prop_is(props, "dog", "calm"))  st = DOG_CALM;
    if (prop_is(props, "dog", "chew"))  st = x01 > 0.85f ? DOG_CHEW : DOG_WALK;
    if (prop_is(props, "dog", "stiff")) st = x01 > 0.7f ? DOG_STIFF : DOG_WALK;
    if (is(show, "sit")) st = DOG_SIT;
    if (is(show, "drop") || is(show, "eat")) st = DOG_CALM;
    int road = prop_is(props, "squirrel", "road"), park = prop_is(props, "squirrel", "park");

    // sky: a couple of slow clouds
    for (int i = 0; i < 3; i++) {
        float cx = b.x + wrapf(i * b.width * 0.41f + (float)t * 6 - travel * 0.1f, b.width + 6 * k) - 3 * k;
        float cy = b.y + (1.2f + i * 0.9f) * k;
        DrawCircleV(v2(cx, cy), 0.9f * k, CA(P->faint, 0.35f));
        DrawCircleV(v2(cx + 0.9f * k, cy + 0.2f * k), 0.7f * k, CA(P->faint, 0.35f));
        DrawCircleV(v2(cx - 0.8f * k, cy + 0.25f * k), 0.6f * k, CA(P->faint, 0.35f));
    }
    // far away: trees going by, or the road, or the park fence
    if (!road && !park)
        for (int i = 0; i < 5; i++) {
            float tx = b.x + wrapf(i * 7.5f * k - travel * 0.45f, 5 * 7.5f * k) - 2 * k;
            DrawRectangle((int)(tx - 0.12f * k), (int)(gy - 4.2f * k), (int)(0.24f * k), (int)(2.5f * k), CA(P->faint, 0.8f));
            DrawCircleV(v2(tx, gy - 4.6f * k), 1.3f * k, CA(P->faint, 0.6f));
        }
    if (road) {
        float ry = gy - 6.0f * k;
        DrawRectangle((int)b.x, (int)ry, (int)b.width, (int)(1.5f * k), CA(P->faint, 0.9f));
        for (float dx = 0; dx < b.width; dx += 1.6f * k)
            DrawRectangle((int)(b.x + dx), (int)(ry + 0.7f * k), (int)(0.8f * k), (int)(0.1f * k), CA(P->dim, 0.6f));
        for (int i = 0; i < 3; i++) {       // cars, both ways
            int dir = i % 2 ? -1 : 1;
            float cx = b.x + wrapf(i * b.width * 0.37f + dir * (float)t * 16 * k, b.width + 5 * k) - 2.5f * k;
            float cy = ry + (dir > 0 ? 0.05f : 0.75f) * k;
            Color cc = i == 1 ? P->bad : i == 2 ? P->accent : P->ok;
            DrawRectangleRounded((Rectangle){ cx, cy, 2.3f * k, 0.65f * k }, 0.5f, 6, CA(cc, 0.75f));
        }
        DrawRectangle((int)b.x, (int)(ry - 0.3f * k), (int)b.width, (int)(0.3f * k), CA(P->dim, 0.35f));   // far sidewalk
    }
    if (park) {
        for (float fx = b.x + 0.5f * k; fx < b.x + b.width; fx += 1.4f * k)
            DrawRectangle((int)fx, (int)(gy - 3.0f * k), (int)(0.16f * k), (int)(3.0f * k), CA(P->faint, 0.7f));
        DrawRectangle((int)b.x, (int)(gy - 2.6f * k), (int)b.width, (int)(0.12f * k), CA(P->faint, 0.7f));
        DrawRectangle((int)b.x, (int)(gy - 1.5f * k), (int)b.width, (int)(0.12f * k), CA(P->faint, 0.7f));
        float tx = b.x + b.width * 0.88f;
        DrawRectangle((int)(tx - 0.25f * k), (int)(gy - 5.0f * k), (int)(0.5f * k), (int)(5.0f * k), CA(P->dim, 0.8f));
        DrawCircleV(v2(tx, gy - 5.8f * k), 2.0f * k, CA(P->ok, 0.35f));
    }
    // ground and grass
    DrawRectangle((int)b.x, (int)gy, (int)b.width, (int)(b.y + b.height - gy), CA(P->faint, 0.45f));
    DrawLineEx(v2(b.x, gy), v2(b.x + b.width, gy), 0.12f * k, C(P->faint));
    for (int i = 0; i < 14; i++) {
        float gx = b.x + wrapf(i * 2.3f * k - travel, 14 * 2.3f * k);
        float sway = sinf((float)t * 2 + i) * 0.08f * k;
        DrawLineEx(v2(gx, gy), v2(gx - 0.15f * k + sway, gy - 0.45f * k), 0.08f * k, CA(P->ok, 0.5f));
        DrawLineEx(v2(gx, gy), v2(gx + 0.15f * k + sway, gy - 0.4f * k), 0.08f * k, CA(P->ok, 0.5f));
    }

    // the squirrel, across the road or by the tree
    if (road || park) {
        float sx = b.x + b.width * (road ? 0.8f : 0.86f), sy = road ? gy - 6.5f * k : gy - 0.45f * k;
        if (is(show, "dog_runs") && park) sy -= ease((float)tw / 0.6f) * 3.5f * k, sx += 0.3f * k;   // up the tree
        float sc = road ? 1.2f : 1.4f;
        Color sq = CA(P->cue, 1);
        DrawCircleV(v2(sx, sy), 0.38f * k * sc, sq);
        DrawCircleV(v2(sx + 0.35f * k * sc, sy - 0.3f * k * sc), 0.24f * k * sc, sq);
        float flick = sinf((float)t * 9) * 12;
        DrawRing(v2(sx - 0.45f * k * sc, sy - 0.45f * k * sc), 0.3f * k * sc, 0.55f * k * sc, 120 + flick, 400 + flick, 20, sq);
    }

    // where everyone stands
    float ox = b.x + b.width * 0.2f, dx = b.x + b.width * 0.43f;
    float run = is(show, "dog_runs") ? ease((float)tw / 0.8f) : 0;
    dx += run * b.width * 0.7f;
    int stranger = has_key(props, "stranger");
    float sx_t = dx + 5.6f * k, sx0 = b.x + b.width + 2.5f * k;
    float sx = lerp(sx0, sx_t, ease((float)ts / 1.8f));
    int s_moving = ts < 1.8, s_dir = -1;
    if (is(show, "stranger_leaves")) { sx = sx_t + (float)tw * 5.5f * k; s_moving = 1; s_dir = 1; }
    float grab = is(show, "grab") ? ease((float)tw / 0.4f) : 0;
    dx += grab * 1.4f * k;

    // the owner (you)
    Vector2 osh = person(ox, gy, k, 1, phase, moving, C(P->accent));
    Vector2 hand = v2(ox + 1.0f * k, gy - 2.6f * k);
    if (run > 0) hand = v2(ox + 1.5f * k, gy - 3.6f * k);        // reaching after the dog
    limb(osh, hand, 0.28f * k, C(P->accent));

    // the dog and its leash
    Vector2 mouth;
    int dog_moving = moving || run > 0;
    float dk = k * 1.35f;      // the dog is drawn larger than life: the cue is on it
    Vector2 collar = dog(dx, gy, dk, st, t, phase * (run > 0 ? 3 : 1) + (float)tw * (run > 0 ? 25 : 0), dog_moving, C(P->warn), C(P->panel), &mouth);
    if (run > 0) {          // the leash slipped: it trails behind
        Vector2 end = v2(collar.x - 3.2f * k, gy - 0.1f * k);
        DrawSplineSegmentBezierQuadratic(collar, v2((collar.x + end.x) / 2, gy - 0.2f * k), end, 0.12f * k, C(P->text));
    } else {
        float sag = grab > 0 ? 0.1f * k : 0.6f * k;
        Vector2 mid = v2((hand.x + collar.x) / 2, (hand.y + collar.y) / 2 + sag);
        DrawSplineSegmentBezierQuadratic(hand, mid, collar, 0.12f * k, C(P->text));
    }

    // what the dog found
    const char *what = prop_is(props, "item", "sock") ? "sock" : prop_is(props, "item", "stick") ? "stick" :
                       prop_is(props, "item", "sausage") ? "sausage" : NULL;
    if (what && !is(show, "eat")) {
        if (is(show, "drop")) item(what, mouth.x + 0.6f * k, gy - 0.2f * k, k, 0);
        else if (st == DOG_CHEW) item(what, mouth.x, mouth.y, k, sinf((float)t * 10) * 10);
        else item(what, dx + 2.65f * 1.35f * k + (6.3f * k - travel), gy - 0.2f * k, k, 0);    // in the grass ahead
        if (!strcmp(what, "sausage") && !is(show, "drop"))       // more pieces in the grass
            for (int i = 0; i < 3; i++) item(what, mouth.x + (1.5f + i * 1.3f) * k, gy - 0.12f * k, k * 0.6f, 20.0f * i);
    }

    // the stranger
    if (stranger && sx < b.x + b.width + 3 * k) {
        Color sc = C(P->cue);
        Vector2 ssh = person(sx, gy, k, s_dir, (float)ts * 7, s_moving, sc);
        float reach = s_dir < 0 ? ease(((float)ts - 1.6f) / 0.7f) : 0;
        Vector2 rest = v2(sx - 0.5f * k * -s_dir, gy - 2.3f * k);
        Vector2 dog_head = v2(mouth.x - 0.9f * k, mouth.y - 1.0f * k);
        Vector2 to = v2(lerp(rest.x, dog_head.x + 0.4f * k, reach), lerp(rest.y, dog_head.y - 0.5f * k, reach));
        if (is(show, "pet")) to.y += sinf((float)tw * 9) * 0.2f * k;
        if (is(show, "bite") && tw > 0.15) to = v2(to.x + 1.0f * k, to.y - 1.4f * k);     // pulled back
        if (grab > 0) to = v2(lerp(to.x, collar.x + 0.2f * k, grab), lerp(to.y, collar.y, grab));
        limb(ssh, to, 0.28f * k, sc);
        if (prop_is(props, "stranger", "leash")) {      // an empty leash in the other hand
            Vector2 h2 = v2(sx + 0.55f * k, gy - 2.2f * k);
            limb(ssh, h2, 0.28f * k, sc);
            float swing = sinf((float)t * 2.6f) * 0.5f * k;
            Vector2 end = v2(sx + 1.3f * k + swing, gy - 0.55f * k);
            DrawSplineSegmentBezierQuadratic(h2, v2(h2.x + 0.9f * k + swing * 0.5f, (h2.y + end.y) / 2), end, 0.1f * k, C(P->text));
            DrawRing(end, 0.32f * k, 0.52f * k, 0, 360, 24, C(P->bad));        // a collar, nobody in it
        }
        if (is(show, "bite") && tw < 0.8) {
            for (int i = 0; i < 6; i++) {
                float a = i * 60 * DEG2RAD;
                DrawLineEx(v2(to.x + cosf(a) * 0.3f * k, to.y + sinf(a) * 0.3f * k),
                           v2(to.x + cosf(a) * 0.8f * k, to.y + sinf(a) * 0.8f * k), 0.1f * k, CA(P->bad, 1 - (float)tw / 0.8f));
            }
        }
    }
    if (is(show, "pet"))        // hearts float up
        for (int i = 0; i < 3; i++) {
            float f = wrapf((float)tw * 0.6f + i * 0.33f, 1);
            heart(mouth.x - 0.6f * k + sinf(f * 6 + i) * 0.4f * k, mouth.y - 1.6f * k - f * 3 * k, 0.25f * k, CA(P->bad, 1 - f));
        }
}

void stage_draw(const char *stage, const char *props, const char *show, Rectangle box,
                double since_scene, double since_show, const StagePal *pal) {
    P = pal;
    DrawRectangleRounded(box, 0.06f, 8, C(pal->panel));
    BeginScissorMode((int)box.x, (int)box.y, (int)box.width, (int)box.height);
    if (stage && !strcmp(stage, "walk")) walk_stage(props, show, box, since_scene, since_show);
    EndScissorMode();
}
