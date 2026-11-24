// Pictures out of the house: the lift, the crosswalk, the taxi, the entrance,
// and the office (the desk, the birthday cake, the coffee machine).
#include "stage_parts.h"

#define K  (b.height / 11.0f)
#define CX (b.x + b.width / 2)

static void floor_band(Rectangle b, float gy) {
    DrawRectangle((int)b.x, (int)gy, (int)b.width, (int)(b.y + b.height - gy) + 1, CA(P->faint, 0.45f));
    DrawLineEx(v2(b.x, gy), v2(b.x + b.width, gy), 2, C(P->faint));
}

static void note(float x, float y, float s, Color c) {
    DrawCircleV(v2(x, y), s * 0.35f, c);
    DrawLineEx(v2(x + s * 0.3f, y), v2(x + s * 0.3f, y - s * 1.2f), s * 0.12f, c);
    DrawLineEx(v2(x + s * 0.3f, y - s * 1.2f), v2(x + s * 0.75f, y - s * 0.9f), s * 0.12f, c);
}

static void notes(float x, float y, float k, Color c, double t) {
    for (int i = 0; i < 3; i++) {
        float f = wrapf((float)t * 0.5f + i * 0.33f, 1);
        note(x + sinf(f * 6 + i) * 0.6f * k + i * 0.6f * k, y - f * 3 * k, 0.8f * k, CA(c, 1 - f));
    }
}

/* ---------- the lift ---------- */

static void tuba(float x, float y, float s, Color c) {
    DrawRing(v2(x, y), 0.8f * s, 1.15f * s, 0, 360, 32, c);
    DrawRing(v2(x + 0.3f * s, y - 0.4f * s), 1.5f * s, 1.85f * s, 190, 330, 32, c);
    DrawCircleV(v2(x - 1.3f * s, y - 1.6f * s), 0.9f * s, c);
}

STAGE(st_lift) {
    float k = K, cx = CX;
    double t = GetTime();
    int tuba_ = prop_is(props, "neighbor", "tuba"), hold = is(show, "hold"), closed = is(show, "close");
    // the doors close while you think; holding opens them again
    float c = ease((float)ts / 4.5f);
    if (is(show, "act")) c = ease((float)ts / 2.0f) * (1 - P->act);        // you push the doors back
    if (hold) c = c * (1 - ease((float)tw / 0.5f));
    if (closed) c = lerp(c, 1, ease((float)tw / 0.4f));
    Rectangle op = { cx - 6 * k, b.y + 1.6f * k, 12 * k, b.height - 1.6f * k };
    // the corridor outside, and the neighbor running up
    DrawRectangleRec(op, CA(P->faint, 0.9f));
    DrawRectangle((int)op.x, (int)(op.y + op.height * 0.72f), (int)op.width, (int)(op.height * 0.28f), CA(P->dim, 0.6f));
    float near_ = hold ? 1 : ease((float)ts / 4.0f);
    float s = lerp(0.35f, 0.85f, near_) * k, gy = op.y + op.height * lerp(0.78f, 0.99f, near_);
    float nx = cx + (hold ? -2.5f * k * ease((float)tw / 0.6f) : 0);
    if (!(closed && tw > 0.4)) {
        Vector2 sh = person(nx, gy, s, -1, (float)ts * 9, !hold || tw < 0.6, C(P->cue));
        face(person_head, s, closed ? -1 : 1, P->bg);
        if (tuba_) {
            tuba(sh.x + 0.6f * s, sh.y + 1.2f * s, s * 1.7f, C(P->warn));
        } else {
            float wv = sinf((float)t * 12) * 0.5f * s;
            limb(sh, v2(sh.x + 1.1f * s + wv, sh.y - 1.9f * s), 0.3f * s, C(P->cue));   // waving
        }
    }
    // the doors
    float half = op.width / 2 * c;
    DrawRectangle((int)op.x, (int)op.y, (int)half, (int)op.height, C(P->dim));
    DrawRectangle((int)(op.x + op.width - half), (int)op.y, (int)half + 1, (int)op.height, C(P->dim));
    DrawLineEx(v2(op.x + half, op.y), v2(op.x + half, op.y + op.height), 3, C(P->faint));
    DrawLineEx(v2(op.x + op.width - half, op.y), v2(op.x + op.width - half, op.y + op.height), 3, C(P->faint));
    // the lift around it: walls, the floor display, the buttons, you
    DrawRectangle((int)b.x, (int)b.y, (int)(op.x - b.x), (int)b.height, C(P->panel));
    DrawRectangle((int)(op.x + op.width), (int)b.y, (int)(b.x + b.width - op.x - op.width), (int)b.height, C(P->panel));
    DrawRectangle((int)b.x, (int)b.y, (int)b.width, (int)(op.y - b.y), C(P->panel));
    DrawRectangleLinesEx(op, 5, C(P->faint));
    DrawRectangleRounded((Rectangle){ cx - 1.5f * k, b.y + 0.25f * k, 3 * k, 1.1f * k }, 0.3f, 4, C(P->bg));
    label(closed && tw > 0.6 ? "2 ^" : "1", cx, b.y + 0.35f * k, 0.9f * k, P->bad, 0, 1);
    float bx = op.x + op.width + 2.2f * k, by = b.y + 4 * k;
    DrawRectangleRounded((Rectangle){ bx - 0.9f * k, by - 0.6f * k, 1.8f * k, 3.4f * k }, 0.3f, 4, C(P->dim));
    DrawCircleV(v2(bx, by + 0.4f * k), 0.5f * k, C(hold ? P->ok : P->text));
    label("<>", bx, by + 0.1f * k, 0.5f * k, P->bg, 0, 1);
    DrawCircleV(v2(bx, by + 1.8f * k), 0.5f * k, C(closed ? P->ok : P->text));
    label("><", bx, by + 1.5f * k, 0.5f * k, P->bg, 0, 1);
    float gy2 = b.y + b.height * 0.98f;
    float px = bx + 4.5f * k - (hold && tuba_ ? ease((float)tw / 0.6f) * 1.2f * k : 0);
    Vector2 sh = person(px, gy2, k, -1, 0, 0, C(P->accent));
    Vector2 hand = v2(px - 1.3f * k, sh.y + 1.0f * k);
    if (hold || is(show, "act")) hand = v2(bx + 0.3f * k, by + 0.4f * k);
    if (closed && tw < 0.6) hand = v2(bx + 0.3f * k, by + 1.8f * k);
    limb(sh, hand, 0.28f * k, C(P->accent));
    face(person_head, k, hold && tuba_ ? -1 : 0, P->bg);
}

/* ---------- the crosswalk, from above ---------- */

STAGE(st_crosswalk) {
    float k = K, cx = CX;
    double t = GetTime();
    float r0 = b.y + 3.2f * k, r1 = b.y + 8.0f * k;
    DrawRectangle((int)b.x, (int)b.y, (int)b.width, (int)(r0 - b.y), CA(P->faint, 0.5f));
    DrawRectangle((int)b.x, (int)r1, (int)b.width, (int)(b.y + b.height - r1) + 1, CA(P->faint, 0.5f));
    DrawRectangle((int)b.x, (int)r0, (int)b.width, (int)(r1 - r0), C(P->bg));
    for (float x = b.x; x < b.x + b.width; x += 2.5f * k)
        DrawRectangle((int)x, (int)((r0 + r1) / 2 - 0.08f * k), (int)(1.3f * k), (int)(0.16f * k), CA(P->dim, 0.6f));
    for (int i = 0; i < 6; i++)
        DrawRectangle((int)(cx - 3.3f * k + i * 1.2f * k), (int)(r0 + 0.3f * k), (int)(0.7f * k), (int)(r1 - r0 - 0.6f * k), CA(P->text, 0.8f));
    int bus = prop_is(props, "road", "bus"), run = is(show, "run"), wait = is(show, "wait");
    // the light: green and blinking, until it is red
    int red = wait || is(show, "red") || (run && tw > 1.5);
    float lx = cx - 7 * k, ly = b.y + 8.6f * k;
    DrawRectangle((int)(lx - 0.15f * k), (int)(ly), (int)(0.3f * k), (int)(2.4f * k), C(P->dim));
    DrawRectangleRounded((Rectangle){ lx - 1.0f * k, ly - 2.4f * k, 2.0f * k, 2.6f * k }, 0.2f, 4, C(P->bg));
    int lit = red || fmodf((float)t, 0.6f) < 0.35f;
    Color lc = red ? P->bad : P->ok;
    if (lit) {
        DrawCircleV(v2(lx, ly - 1.75f * k), 0.25f * k, C(lc));
        limb(v2(lx, ly - 1.45f * k), v2(lx, ly - 0.9f * k), 0.2f * k, C(lc));
        if (red) {
            limb(v2(lx, ly - 0.9f * k), v2(lx - 0.15f * k, ly - 0.35f * k), 0.16f * k, C(lc));
            limb(v2(lx, ly - 0.9f * k), v2(lx + 0.15f * k, ly - 0.35f * k), 0.16f * k, C(lc));
        } else {
            limb(v2(lx, ly - 0.9f * k), v2(lx - 0.35f * k, ly - 0.35f * k), 0.16f * k, C(lc));
            limb(v2(lx, ly - 0.9f * k), v2(lx + 0.35f * k, ly - 0.4f * k), 0.16f * k, C(lc));
        }
    }
    if (!red) label("3", lx + 1.4f * k, ly - 2.2f * k, 0.8f * k, P->ok, 0, 1);
    // the bus turns the corner
    if (bus) {
        float bxx = b.x + b.width - ease((float)ts / 3.0f) * 10 * k;
        if (run) bxx = lerp(bxx, cx + 4.2f * k, ease((float)tw / 0.5f));
        if (wait || is(show, "red")) bxx -= ease((float)tw / 1.6f) * b.width * 1.2f;
        Rectangle bus_ = { bxx, r0 + 0.5f * k, 11 * k, 2.6f * k };
        DrawRectangleRounded(bus_, 0.2f, 6, C(P->warn));
        for (int i = 0; i < 5; i++)
            DrawRectangle((int)(bus_.x + 1.2f * k + i * 1.9f * k), (int)(bus_.y + 0.4f * k), (int)(1.4f * k), (int)(0.9f * k), C(P->bg));
        if (run && tw > 0.3 && tw < 2.5) {
            label("HONK", bus_.x - 0.5f * k, bus_.y - 1.6f * k, 1.1f * k, P->bad, 0, 1);
            buzz(bus_.x, bus_.y + 1.3f * k, 1.2f * k, P->bad);
        }
    }
    // you, at the curb; across if you run
    float py = r1 + 2.6f * k, px = cx - 1 * k;
    float f = run ? ease((float)tw / 1.2f) : 0;
    py = lerp(py, r0 - 0.3f * k, f);
    person(px, py, k * 0.5f, 1, (float)tw * 14, run && f < 1, C(P->accent));
    if (!bus && run && tw > 1.2) {          // the pigeon is impressed
        DrawCircleV(v2(cx + 3 * k, r0 - 0.6f * k), 0.35f * k, C(P->dim));
        label("!", cx + 3 * k, r0 - 2 * k, 0.8f * k, P->text, 0, 1);
    }
    if (!bus && wait) {                     // a grandma overtakes you
        float g = ease((float)tw / 2.5f);
        person(cx + 1 * k, lerp(r1 + 2.6f * k, r0 - 0.3f * k, g), k * 0.45f, 1, (float)tw * 10, g < 1, C(P->cue));
    }
}

/* ---------- the taxi ---------- */

STAGE(st_taxi) {
    float k = K, cx = CX, hy = b.y + 3.6f * k, dash = b.y + 7.6f * k;
    double t = GetTime();
    int left = is(show, "left"), right = is(show, "right"), straight = is(show, "straight");
    float turn = left ? -ease((float)tw / 1.0f) : right ? ease((float)tw / 1.0f) : 0;
    DrawRectangle((int)b.x, (int)b.y, (int)b.width, (int)(hy - b.y), CA(P->accent, 0.15f));
    DrawRectangle((int)b.x, (int)hy, (int)b.width, (int)(dash - hy), CA(P->ok, 0.12f));
    // the road splits ahead
    float sh = -turn * 10 * k;
    // one road toward you, two going off to the sides at the fork
    Color road = CA(P->dim, 0.8f);
    float fy = hy + 1.4f * k;
    tri(v2(cx - 13 * k + sh * 0.2f, dash), v2(cx + 13 * k + sh * 0.2f, dash), v2(cx - 2.2f * k + sh, fy), road);
    tri(v2(cx + 13 * k + sh * 0.2f, dash), v2(cx + 2.2f * k + sh, fy), v2(cx - 2.2f * k + sh, fy), road);
    tri(v2(cx - 2.2f * k + sh, fy), v2(cx + sh, fy), v2(cx - 14 * k + sh * 1.4f, hy + 0.2f * k), road);
    tri(v2(cx - 14 * k + sh * 1.4f, hy + 0.2f * k), v2(cx - 14 * k + sh * 1.4f, hy - 0.2f * k), v2(cx - 1.0f * k + sh, fy - 0.3f * k), road);
    tri(v2(cx + 2.2f * k + sh, fy), v2(cx + sh, fy), v2(cx + 14 * k + sh * 1.4f, hy + 0.2f * k), road);
    tri(v2(cx + 14 * k + sh * 1.4f, hy + 0.2f * k), v2(cx + 14 * k + sh * 1.4f, hy - 0.2f * k), v2(cx + 1.0f * k + sh, fy - 0.3f * k), road);
    for (int i = 0; i < 4; i++) {           // lane marks rushing toward you
        float f = wrapf((float)t * 0.8f + i * 0.25f, 1);
        float y = lerp(hy + 1.6f * k, dash, f * f);
        DrawRectangle((int)(cx + sh * (1 - f) - 0.1f * k * (1 + f * 3)), (int)y, (int)(0.2f * k * (1 + f * 3)), (int)(0.5f * k * (1 + f * 3)), CA(P->text, 0.7f));
    }
    if (straight)           // rush hour: brake lights all the way
        for (int i = 0; i < 4; i++) {
            float y = hy + 1.8f * k + i * 1.2f * k, w = (2 + i) * k;
            DrawRectangleRounded((Rectangle){ cx - w / 2, y, w, 0.7f * k * (1 + i * 0.4f) }, 0.3f, 4, C(P->dim));
            DrawCircleV(v2(cx - w / 2 + 0.3f * k, y + 0.3f * k), 0.18f * k * (1 + i * 0.3f), C(P->bad));
            DrawCircleV(v2(cx + w / 2 - 0.3f * k, y + 0.3f * k), 0.18f * k * (1 + i * 0.3f), C(P->bad));
        }
    if (prop_is(props, "sign", "right")) {  // the road sign, if you look up
        Rectangle s = { cx + 6 * k + sh, b.y + 0.6f * k, 7.5f * k, 1.9f * k };
        DrawRectangle((int)(s.x + s.width / 2 - 0.1f * k), (int)(s.y + s.height), (int)(0.2f * k), (int)(1.6f * k), C(P->dim));
        DrawRectangleRounded(s, 0.15f, 4, C(P->accent));
        label("CITY CENTER >", s.x + s.width / 2, s.y + 0.5f * k, 0.8f * k, P->text, 0, 1);
    }
    // inside: the dashboard, the driver, his phone
    DrawRectangle((int)b.x, (int)dash, (int)b.width, (int)(b.y + b.height - dash) + 1, C(P->panel));
    DrawRectangleRounded((Rectangle){ cx - 13 * k, dash - 0.3f * k, 26 * k, 0.8f * k }, 0.5f, 6, C(P->dim));
    float dx = cx - 9 * k;
    DrawCircleV(v2(dx, b.y + 5.4f * k), 1.0f * k, C(P->bg));
    DrawRectangleRounded((Rectangle){ dx - 2 * k, b.y + 6.2f * k, 4 * k, 4 * k }, 0.5f, 8, C(P->bg));
    DrawRing(v2(dx + turn * 0.3f * k, dash + 1.6f * k), 1.5f * k, 1.85f * k, 180 + turn * 40, 360 + turn * 40, 32, C(P->faint));
    Rectangle ph = { cx + 0.5f * k, b.y + 5.2f * k, 3.6f * k, 4.6f * k };
    DrawRectangleRounded(ph, 0.2f, 6, C(P->bg));
    Rectangle scr = { ph.x + 0.3f * k, ph.y + 0.3f * k, ph.width - 0.6f * k, ph.height - 0.6f * k };
    if (prop_is(props, "nav", "left")) {
        DrawRectangleRec(scr, C(P->ok));
        float ax = scr.x + scr.width / 2, ay = scr.y + 1.6f * k;
        tri(v2(ax - 1.2f * k, ay), v2(ax, ay - 0.9f * k), v2(ax, ay + 0.9f * k), C(P->text));
        DrawRectangle((int)ax, (int)(ay - 0.35f * k), (int)(1.1f * k), (int)(0.7f * k), C(P->text));
        label("LEFT", ax, scr.y + 2.9f * k, 0.7f * k, P->text, 0, 1);
    } else {            // dead: an empty battery
        DrawRectangleRec(scr, C((Color){ 8, 8, 10, 255 }));
        Rectangle bat = { scr.x + scr.width / 2 - 0.9f * k, scr.y + 1.6f * k, 1.8f * k, 0.9f * k };
        DrawRectangleLinesEx(bat, 2, C(P->dim));
        DrawRectangle((int)(bat.x + bat.width), (int)(bat.y + 0.25f * k), (int)(0.15f * k), (int)(0.4f * k), C(P->dim));
        DrawRectangle((int)(bat.x + 0.15f * k), (int)(bat.y + 0.15f * k), (int)(0.2f * k), (int)(0.6f * k), C(P->bad));
    }
    if (!show) {
        Rectangle bb = { dx - 1 * k, b.y + 1.2f * k, 6.5f * k, 1.6f * k };
        bubble(bb, dx + 0.5f * k, b.y + 4.4f * k, P->text);
        label("left or right?", bb.x + bb.width / 2, bb.y + 0.4f * k, 0.75f * k, P->bg, 0, 1);
    }
}

/* ---------- the entrance, in the evening ---------- */

STAGE(st_entrance) {
    float k = K, cx = CX, gy = b.y + b.height * 0.92f;
    double t = GetTime();
    int grin = prop_is(props, "neighbor", "grin");
    DrawRectangle((int)b.x, (int)b.y, (int)b.width, (int)(gy - b.y), CA(P->faint, 0.5f));
    for (float y = b.y + 0.8f * k; y < gy; y += 1.0f * k)          // bricks
        for (float x = b.x + fmodf(y, 2 * k); x < b.x + b.width; x += 2.4f * k)
            DrawRectangleLines((int)x, (int)y, (int)(2.4f * k), (int)(1.0f * k), CA(P->dim, 0.25f));
    floor_band(b, gy);
    // the door: open for you, or slowly closing
    int closes = is(show, "door_shut");
    float open = grin ? 1 : 0.5f;
    if (is(show, "door_open")) open = lerp(0.5f, 1, ease((float)tw / 0.6f));
    if (closes) open = lerp(open, 0, ease(((float)tw - 0.8f) / 1.6f));
    Rectangle fr = { cx + 2 * k, gy - 8 * k, 5.5f * k, 8 * k };
    DrawRectangleRec(fr, C(P->bg));
    float dw = fr.width * (1 - open * 0.75f);
    DrawRectangle((int)fr.x, (int)fr.y, (int)dw, (int)fr.height, C((Color){ 90, 66, 46, 255 }));
    DrawCircleV(v2(fr.x + dw - 0.5f * k, fr.y + 4.2f * k), 0.18f * k, C(P->warn));
    DrawRectangleLinesEx(fr, 4, C(P->dim));
    // the neighbor, holding it, or not
    float nx = fr.x + fr.width + 1.6f * k;
    Vector2 nsh = person(nx, gy, k, -1, 0, 0, C(P->cue));
    int mood = grin ? 1 : -1;
    if (is(show, "thank")) mood = 1;
    if (is(show, "ignore")) mood = (tw > 0.8) ? 0 : 1;
    if (is(show, "door_open")) mood = 2;
    face(person_head, k, mood, P->bg);
    if (closes && tw > 0.8) limb(nsh, v2(nsh.x - 0.6f * k, nsh.y + 1.6f * k), 0.28f * k, C(P->cue));
    else limb(nsh, v2(fr.x + dw - 0.4f * k, fr.y + 4.2f * k), 0.28f * k, C(P->cue));
    // you, with heavy bags
    float px = lerp(b.x + 2 * k, cx - 5 * k, ease((float)ts / 1.5f));
    Vector2 sh = person(px, gy, k, 1, (float)ts * 7, ts < 1.5, C(P->accent));
    for (int i = -1; i <= 1; i += 2) {
        Vector2 h = v2(px + i * 0.9f * k, sh.y + 2.0f * k);
        limb(sh, h, 0.28f * k, C(P->accent));
        DrawRectangleRounded((Rectangle){ h.x - 0.9f * k, h.y, 1.8f * k, 2.0f * k }, 0.2f, 4, C(P->warn));
    }
    face(person_head, k, 2, P->bg);
    DrawCircleV(v2(person_head.x + 0.8f * k, person_head.y - 0.1f * k + wrapf((float)t, 1) * 0.6f * k), 0.12f * k, C(P->accent));  // sweat
    const char *say = is(show, "thank") ? "thanks!" : is(show, "door_open") ? "sorry about this morning" :
                      is(show, "door_shut") ? "hold it, please?" : NULL;
    if (say && tw < 3) {
        float w = MeasureTextEx(P->font, say, 0.75f * k, 1).x + 1.2f * k;
        Rectangle bb = { px - w / 2, b.y + 0.6f * k, w, 1.5f * k };
        bubble(bb, px, person_head.y - 1 * k, P->text);
        label(say, px, bb.y + 0.35f * k, 0.75f * k, P->bg, 0, 1);
    }
    if (closes && tw > 1.6 && tw < 3.2) label("SLAM", fr.x + fr.width / 2, fr.y - 1.4f * k, 1.0f * k, P->bad, 0, 1);
}

/* ---------- the office: the desk ---------- */

static void wall_clock(float x, float y, float r, int h, int m, Color face_, Color hand) {
    DrawCircleV(v2(x, y), r, C(face_));
    DrawRing(v2(x, y), r * 0.92f, r, 0, 360, 40, C(P->dim));
    float am = (m * 6 - 90) * DEG2RAD, ah = ((h % 12) * 30 + m * 0.5f - 90) * DEG2RAD;
    DrawLineEx(v2(x, y), v2(x + cosf(am) * r * 0.8f, y + sinf(am) * r * 0.8f), r * 0.08f, C(hand));
    DrawLineEx(v2(x, y), v2(x + cosf(ah) * r * 0.5f, y + sinf(ah) * r * 0.5f), r * 0.12f, C(hand));
}

STAGE(st_desk) {
    float k = K, cx = CX, gy = b.y + b.height * 0.94f;
    double t = GetTime();
    floor_band(b, gy);
    if (prop_is(props, "boss", "door")) {           // late: he is waiting by the door
        wall_clock(cx - 1 * k, b.y + 2.3f * k, 1.6f * k, 9, 47, P->text, P->bad);
        label("9:47", cx - 1 * k, b.y + 4.2f * k, 0.8f * k, P->bad, 0, 1);
        Rectangle door = { cx + 7 * k, gy - 8 * k, 5 * k, 8 * k };
        DrawRectangleRec(door, C(P->bg));
        DrawRectangleLinesEx(door, 4, C(P->dim));
        int bow = is(show, "apologize");
        float px = door.x + 2.5f * k;
        Vector2 sh = person(px, gy, k, -1, 0, 0, C(P->accent));
        face(person_head, k, bow ? 2 : -1, P->bg);
        if (bow) DrawCircleV(v2(px - 0.4f * k, person_head.y + 0.6f * k), 0.5f * k, CA(P->panel, 0.4f));
        (void)sh;
        float bx = cx + 2.2f * k - (bow && tw > 1 ? ease(((float)tw - 1) / 1.2f) * 9 * k : 0);
        Vector2 bs = person(bx, gy, k * 1.08f, 1, (float)tw * 8, bow && tw > 1, C(P->cue));
        face(person_head, k * 1.08f, bow ? 2 : -1, P->bg);
        limb(bs, v2(bs.x + 0.9f * k, bs.y + 1.0f * k), 0.3f * k, C(P->cue));       // arms crossed
        limb(bs, v2(bs.x - 0.9f * k, bs.y + 1.0f * k), 0.3f * k, C(P->cue));
        DrawLineEx(v2(bs.x - 0.9f * k, bs.y + 1.0f * k), v2(bs.x + 0.9f * k, bs.y + 1.0f * k), 0.3f * k, C(P->cue));
        if (!show) {        // tapping a foot
            float tap = fmodf((float)t, 0.5f) < 0.25f ? 0.2f * k : 0;
            DrawLineEx(v2(bx + 0.6f * k, gy - tap), v2(bx + 1.1f * k, gy - tap), 4, C(P->cue));
        }
        const char *say = is(show, "excuse") ? "traffic! terrible traffic!" : NULL;
        if (say) {
            Rectangle bb = { px - 9 * k, b.y + 0.5f * k, 9.5f * k, 1.5f * k };
            bubble(bb, px - 0.5f * k, gy - 6.6f * k, P->text);
            label(say, bb.x + bb.width / 2, bb.y + 0.35f * k, 0.7f * k, P->bg, 0, 1);
            if (tw > 1.2) {
                Rectangle b2 = { bx - 12 * k, b.y + 2.4f * k, 9.5f * k, 1.5f * k };
                bubble(b2, bx - 0.4f * k, gy - 6.5f * k, P->cue);
                label("you take the metro", b2.x + b2.width / 2, b2.y + 0.35f * k, 0.7f * k, P->bg, 0, 1);
            }
        }
        return;
    }
    int fri = prop_is(props, "day", "friday");
    // the window: sunset on a Friday, morning on a Tuesday
    Rectangle w = { cx + 9 * k, b.y + 0.8f * k, 7.5f * k, 5 * k };
    DrawRectangleGradientV((int)w.x, (int)w.y, (int)w.width, (int)w.height, CA(fri ? P->bad : P->accent, 0.45f), CA(fri ? P->warn : P->accent, 0.15f));
    DrawCircleV(v2(w.x + w.width * 0.5f, w.y + (fri ? 4.3f : 1.3f) * k), 0.9f * k, CA(P->warn, 0.9f));
    DrawRectangleLinesEx(w, 4, C(P->dim));
    wall_clock(cx - 13 * k, b.y + 2.4f * k, 1.6f * k, fri ? 17 : 9, fri ? 55 : 10, P->text, P->bg);
    // a desk calendar: the day, in big letters
    Rectangle cal = { cx + 5 * k, gy - 5.2f * k, 2.6f * k, 1.9f * k };
    // the desk, the screen, the message
    DrawRectangle((int)(cx - 9 * k), (int)(gy - 3.3f * k), (int)(18 * k), (int)(0.6f * k), C(P->dim));
    DrawRectangle((int)(cx - 8.5f * k), (int)(gy - 2.7f * k), (int)(0.5f * k), (int)(2.7f * k), C(P->dim));
    DrawRectangle((int)(cx + 8 * k), (int)(gy - 2.7f * k), (int)(0.5f * k), (int)(2.7f * k), C(P->dim));
    DrawRectangle((int)(cal.x), (int)(cal.y), (int)cal.width, (int)cal.height, C(P->text));
    DrawRectangle((int)(cal.x), (int)(cal.y), (int)cal.width, (int)(0.45f * k), C(P->bad));
    label(fri ? "FRI" : "TUE", cal.x + cal.width / 2, cal.y + 0.55f * k, 1.0f * k, P->bg, 0, 1);
    Rectangle m = { cx - 4.8f * k, gy - 9.0f * k, 9 * k, 5.4f * k };
    DrawRectangle((int)(cx - 0.3f * k), (int)(m.y + m.height), (int)(0.6f * k), (int)(0.9f * k), C(P->dim));
    DrawRectangleRounded(m, 0.08f, 6, C(P->bg));
    Rectangle scr = { m.x + 0.3f * k, m.y + 0.3f * k, m.width - 0.6f * k, m.height - 0.6f * k };
    DrawRectangleRec(scr, CA(P->faint, 0.8f));
    DrawCircleV(v2(scr.x + 0.8f * k, scr.y + 0.9f * k), 0.45f * k, C(P->cue));
    label("Boss", scr.x + 1.5f * k, scr.y + 0.55f * k, 0.6f * k, P->text, -1, 1);
    DrawRectangleRounded((Rectangle){ scr.x + 0.4f * k, scr.y + 1.7f * k, 5.6f * k, 1.2f * k }, 0.5f, 6, C(P->text));
    label("got a minute?", scr.x + 0.8f * k, scr.y + 1.95f * k, 0.6f * k, P->bg, -1, 0);
    if (is(show, "busy")) {
        DrawRectangleRounded((Rectangle){ scr.x + scr.width - 5.0f * k, scr.y + 3.1f * k, 4.6f * k, 1.1f * k }, 0.5f, 6, C(P->accent));
        label("busy, sorry!", scr.x + scr.width - 4.6f * k, scr.y + 3.3f * k, 0.6f * k, P->bg, -1, 0);
    }
    if (is(show, "seen")) label("read", scr.x + 6.2f * k, scr.y + 2.2f * k, 0.5f * k, P->dim, -1, 0);
    // you at the desk
    float px = cx - 9.5f * k;
    Vector2 sh = person(px, gy, k, 1, 0, 0, C(P->accent));
    limb(sh, v2(cx - 7 * k, gy - 3.4f * k), 0.28f * k, C(P->accent));
    face(person_head, k, is(show, "yes") ? (fri ? -1 : 1) : 0, P->bg);
    // the boss comes over: with a pile of work, or with the good project
    if (is(show, "yes") || is(show, "seen")) {
        float bx = lerp(b.x + b.width + 2 * k, cx + 12.5f * k, ease((float)tw / 1.2f));
        Vector2 bs = person(bx, gy, k * 1.08f, -1, (float)tw * 8, tw < 1.2, C(P->cue));
        face(person_head, k * 1.08f, 1, P->bg);
        limb(bs, v2(bx - 1.2f * k, bs.y + 0.6f * k), 0.3f * k, C(P->cue));
        if (fri || is(show, "seen"))
            for (int i = 0; i < 6; i++)
                DrawRectangle((int)(bx - 2.4f * k), (int)(bs.y + 0.4f * k - i * 0.35f * k), (int)(2 * k), (int)(0.3f * k), C(i % 2 ? P->text : P->dim));
        else {
            DrawRectangle((int)(bx - 2.4f * k), (int)(bs.y - 0.2f * k), (int)(2 * k), (int)(1.4f * k), C(P->warn));
            drift("*", bx - 1.4f * k, bs.y - 0.6f * k, k, P->warn, t);
        }
    }
}

/* ---------- the birthday cake ---------- */

static void party_hat(Vector2 head, float k) {
    tri(v2(head.x - 0.5f * k, head.y - 0.45f * k), v2(head.x + 0.5f * k, head.y - 0.45f * k), v2(head.x, head.y - 1.7f * k), C(P->bad));
    DrawCircleV(v2(head.x, head.y - 1.75f * k), 0.18f * k, C(P->warn));
}

STAGE(st_cake) {
    float k = K, cx = CX, gy = b.y + b.height * 0.96f;
    double t = GetTime();
    int mine = prop_is(props, "cake", "you"), sing = is(show, "sing"), eat = is(show, "eat"), mumble = is(show, "mumble");
    floor_band(b, gy);
    // colleagues behind the table, singing
    float xs[4] = { -7.5f, -3.0f, 3.0f, 7.5f };
    for (int i = 0; i < 4; i++) {
        float sway = (sing || !show || mumble) ? sinf((float)t * 3 + i) * 0.2f * k : 0;
        Vector2 sh = person(cx + xs[i] * k + sway, gy, k, i < 2 ? 1 : -1, 0, 0, C(P->cue));
        int mood = eat && !mine ? -1 : 1;
        if (eat && mine) mood = 1;
        face(person_head, k, mood, P->bg);
        if (!mine && i == 3) {
            party_hat(person_head, k);          // the birthday girl
            if (sing) for (int j = 0; j < 2; j++)
                DrawCircleV(v2(person_head.x + (j ? 0.25f : -0.25f) * k, person_head.y + wrapf((float)t + j * 0.5f, 1) * 0.9f * k), 0.1f * k, C(P->accent));
        }
        if (eat && mine) {          // applause
            float c = sinf((float)t * 14) * 0.3f * k;
            limb(sh, v2(sh.x + c, sh.y - 1.6f * k), 0.26f * k, C(P->cue));
        }
        if (!eat) notes(person_head.x, person_head.y - 1.2f * k, k * 0.7f, P->text, t + i * 0.7);
    }
    // the table and the cake
    DrawRectangle((int)(cx - 10 * k), (int)(gy - 3.4f * k), (int)(20 * k), (int)(0.5f * k), C(P->dim));
    DrawRectangle((int)(cx - 9 * k), (int)(gy - 2.9f * k), (int)(18 * k), (int)(2.9f * k), CA(P->dim, 0.5f));
    Rectangle cake = { cx - 3.6f * k, gy - 6.2f * k, 7.2f * k, 2.8f * k };
    DrawRectangleRounded(cake, 0.2f, 6, C(P->text));
    DrawRectangle((int)cake.x, (int)(cake.y + 1.3f * k), (int)cake.width, (int)(0.3f * k), C(P->bad));
    if (eat) tri(v2(cx + 1.2f * k, cake.y), v2(cx + 3.6f * k, cake.y), v2(cx + 2.4f * k, cake.y + 2.8f * k), C(P->panel));   // a slice gone
    label("HAPPY BIRTHDAY", cx, cake.y + 0.25f * k, 0.6f * k, P->bad, 0, 1);
    label(mine ? "TO YOU!" : "OLGA", cx, cake.y + 1.75f * k, 0.75f * k, P->accent, 0, 1);
    for (int i = 0; i < 5; i++) {
        float x = cake.x + 1.0f * k + i * 1.3f * k;
        DrawRectangle((int)(x - 0.1f * k), (int)(cake.y - 1.0f * k), (int)(0.2f * k), (int)(1.0f * k), C(P->accent));
        DrawCircleV(v2(x, cake.y - 1.2f * k + sinf((float)t * 15 + i) * 0.05f * k), 0.17f * k, C(P->warn));
    }
    // you, at the end of the table
    float px = cx - 13.5f * k;
    Vector2 sh = person(px, gy, k, 1, 0, 0, C(P->accent));
    if (mine) party_hat(person_head, k);
    face(person_head, k, eat ? 1 : mumble ? 2 : 1, P->bg);
    if (eat) {
        limb(sh, v2(px + 1.2f * k, sh.y + 0.5f * k), 0.28f * k, C(P->accent));
        DrawRectangle((int)(px + 0.8f * k), (int)(sh.y + 0.2f * k), (int)(1.6f * k), (int)(0.3f * k), C(P->text));
        tri(v2(px + 1.0f * k, sh.y + 0.2f * k), v2(px + 2.2f * k, sh.y + 0.2f * k), v2(px + 1.6f * k, sh.y - 0.6f * k), C(P->text));
    }
    if (sing || is(show, "act")) notes(person_head.x + 0.5f * k, person_head.y - 1.2f * k, k, P->accent, t);
    if (mumble) label("...mm...", person_head.x + 0.8f * k, person_head.y - 2.0f * k, 0.6f * k, P->dim, -1, 0);
}

/* ---------- the coffee machine ---------- */

STAGE(st_coffee) {
    float k = K, cx = CX, gy = b.y + b.height * 0.94f;
    double t = GetTime();
    int empty = prop_is(props, "tank", "empty"), sticker = prop_is(props, "sticker", "kick");
    int water = is(show, "water"), kick = is(show, "kick");
    int works = (water && empty) || (kick && sticker);
    floor_band(b, gy);
    float shake = kick && tw < 0.6 ? sinf((float)tw * 60) * 0.25f * k : 0;
    Rectangle m = { cx - 4 * k + shake, b.y + 0.7f * k, 8 * k, gy - b.y - 0.7f * k };
    DrawRectangleRounded(m, 0.08f, 6, C(P->dim));
    Rectangle scr = { m.x + 0.8f * k, m.y + 0.8f * k, 6.4f * k, 1.8f * k };
    DrawRectangleRec(scr, C(P->bg));
    int fixed = works && tw > 0.8;
    int blink = fmodf((float)t, 0.8f) < 0.45f;
    if (!fixed) label(empty ? "NO WATER" : "ERROR", scr.x + scr.width / 2, scr.y + 0.45f * k, 0.9f * k, blink ? P->bad : P->dim, 0, 1);
    else label("READY", scr.x + scr.width / 2, scr.y + 0.45f * k, 0.9f * k, P->ok, 0, 1);
    DrawCircleV(v2(m.x + m.width - 0.7f * k, m.y + 3.3f * k), 0.3f * k, C(fixed ? P->ok : (blink ? P->bad : P->dim)));
    // the tank on the side: empty, or full to the top
    Rectangle tank = { m.x + m.width + 0.2f * k, m.y + 1.4f * k, 2.6f * k, 5.2f * k };
    float level = empty ? 0 : 1;
    if (water) level = empty ? ease((float)tw / 1.0f) : 1;
    DrawRectangleRec((Rectangle){ tank.x, tank.y + tank.height * (1 - level), tank.width, tank.height * level }, CA(P->accent, 0.55f));
    DrawRectangleLinesEx(tank, 3, C(P->text));
    if (sticker) {
        DrawRectanglePro((Rectangle){ m.x + 4 * k, m.y + 5.6f * k, 4.2f * k, 1.4f * k }, v2(2.1f * k, 0.7f * k), -6, C(P->warn));
        DrawTextPro(P->bold, "KICK HERE", v2(m.x + 2.3f * k, m.y + 5.3f * k), v2(0, 0), -6, 0.7f * k, 1, C(P->bg));
    }
    // the spout and the cup
    DrawRectangle((int)(cx - 0.4f * k + shake), (int)(m.y + 3.4f * k), (int)(0.8f * k), (int)(0.8f * k), C(P->bg));
    Rectangle cup = { cx - 0.9f * k, gy - 1.8f * k, 1.8f * k, 1.4f * k };
    DrawRectangle((int)(m.x + 0.6f * k), (int)(gy - 0.4f * k), (int)(m.width - 1.2f * k), (int)(0.4f * k), C(P->bg));
    DrawRectangleRec(cup, C(P->text));
    if (fixed) {
        DrawRectangle((int)(cx - 0.1f * k), (int)(m.y + 4.2f * k), (int)(0.2f * k), (int)(cup.y - m.y - 4.2f * k), C((Color){ 120, 80, 50, 255 }));
        drift("~", cx, cup.y - 0.5f * k, k * 0.7f, P->text, t);
    }
    if (water && !empty) {          // the tank was full: now so is the floor
        for (int i = 0; i < 3; i++)
            DrawCircleV(v2(tank.x + 0.5f * k + i * 0.8f * k, tank.y + wrapf((float)tw * 1.5f + i * 0.3f, 1) * 5 * k), 0.18f * k, CA(P->accent, 0.8f));
        DrawRectangleRounded((Rectangle){ tank.x - 3 * k, gy - 0.25f * k, 8 * k * ease((float)tw / 1.5f), 0.3f * k }, 1, 6, CA(P->accent, 0.6f));
    }
    if (kick && empty && tw > 0.3) DrawRing(v2(m.x + 1.5f * k, m.y + 7 * k), 0.6f * k, 0.75f * k, 200, 340, 12, C(P->bg));   // a dent
    // you
    float px = cx - 11 * k;
    Vector2 sh = person(px, gy, k, 1, 0, 0, C(P->accent));
    if (water) {
        float f = ease((float)tw / 0.4f);
        Vector2 jug = v2(lerp(px + 1 * k, tank.x + 1 * k, f), lerp(sh.y + 1 * k, tank.y - 1.4f * k, f));
        if (f < 1) limb(sh, jug, 0.28f * k, C(P->accent));
        DrawRectangleRounded((Rectangle){ jug.x - 0.7f * k, jug.y - 0.6f * k, 1.4f * k, 1.6f * k }, 0.3f, 4, CA(P->accent, 0.8f));
        if (f >= 1 && tw < 1.4) DrawLineEx(v2(jug.x + 0.7f * k, jug.y), v2(jug.x + 0.9f * k, tank.y + 0.5f * k), 0.2f * k, CA(P->accent, 0.8f));
    }
    if (kick && tw < 0.5) {
        float hip = gy - 2.2f * k;
        limb(v2(px, hip), v2(m.x - 0.2f * k, gy - 1.2f * k), 0.32f * k, C(P->accent));
    }
    face(person_head, k, fixed ? 1 : (show ? -1 : 0), P->bg);
    if (is(show, "taken")) {        // someone else got there first
        float cx2 = lerp(b.x + b.width + 2 * k, cx + 10 * k, ease((float)tw / 1.0f));
        Vector2 os = person(cx2, gy, k, -1, (float)tw * 8, tw < 1.0, C(P->cue));
        face(person_head, k, 1, P->bg);
        limb(os, v2(cx2 - 1 * k, os.y + 0.6f * k), 0.28f * k, C(P->cue));
        DrawRectangle((int)(cx2 - 1.6f * k), (int)(os.y + 0.1f * k), (int)(1.2f * k), (int)(1.0f * k), C(P->text));
    }
}
