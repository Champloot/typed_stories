// Pictures at home: the bedroom, the kitchen, the fridge, the peephole, the
// doormat, the phone. See stage_parts.h for what b, k, ts and tw are.
#include "stage_parts.h"

#define K  (b.height / 11.0f)
#define CX (b.x + b.width / 2)

static void floor_band(Rectangle b, float gy) {
    DrawRectangle((int)b.x, (int)gy, (int)b.width, (int)(b.y + b.height - gy) + 1, CA(P->faint, 0.45f));
    DrawLineEx(v2(b.x, gy), v2(b.x + b.width, gy), 2, C(P->faint));
}

static void window_(Rectangle w, Color sky, int open) {
    DrawRectangleRec(w, CA(sky, 0.25f));
    if (open) {             // both halves swung inward
        tri(v2(w.x, w.y), v2(w.x - w.width * 0.18f, w.y + w.height * 0.08f), v2(w.x, w.y + w.height), CA(P->text, 0.25f));
        tri(v2(w.x - w.width * 0.18f, w.y + w.height * 0.08f), v2(w.x - w.width * 0.18f, w.y + w.height * 0.92f), v2(w.x, w.y + w.height), CA(P->text, 0.25f));
        tri(v2(w.x + w.width, w.y), v2(w.x + w.width * 1.18f, w.y + w.height * 0.08f), v2(w.x + w.width, w.y + w.height), CA(P->text, 0.25f));
        tri(v2(w.x + w.width * 1.18f, w.y + w.height * 0.08f), v2(w.x + w.width * 1.18f, w.y + w.height * 0.92f), v2(w.x + w.width, w.y + w.height), CA(P->text, 0.25f));
    } else {
        DrawLineEx(v2(w.x + w.width / 2, w.y), v2(w.x + w.width / 2, w.y + w.height), 3, C(P->dim));
        DrawLineEx(v2(w.x, w.y + w.height / 2), v2(w.x + w.width, w.y + w.height / 2), 3, C(P->dim));
    }
    DrawRectangleLinesEx(w, 4, C(P->dim));
}

/* ---------- the alarm ---------- */

STAGE(st_bedroom) {
    float k = K, cx = CX, gy = b.y + b.height * 0.88f;
    double t = GetTime();
    int sat = prop_is(props, "day", "sat"), up = is(show, "get_up"), snooze = is(show, "snooze");
    int ringing = !show || is(show, "knock") || is(show, "missed");
    floor_band(b, gy);
    // the window above the bed; on a Saturday the birds are up too
    Rectangle w = { cx - 3.5f * k, b.y + 0.8f * k, 7 * k, 4.4f * k };
    window_(w, P->accent, 0);
    DrawCircleV(v2(w.x + w.width * 0.75f, w.y + w.height * 0.3f), 0.7f * k, CA(P->warn, 0.8f));
    if (up && sat)
        for (int i = 0; i < 3; i++) {
            float bx = w.x + 1.0f * k + i * 1.6f * k + sinf((float)t * 3 + i) * 0.3f * k, by = w.y + 2.2f * k + i * 0.3f * k;
            DrawLineEx(v2(bx - 0.4f * k, by - 0.2f * k), v2(bx, by), 3, C(P->bg));
            DrawLineEx(v2(bx, by), v2(bx + 0.4f * k, by - 0.2f * k), 3, C(P->bg));
            label("?", bx, by - 1.0f * k, 0.6f * k, P->bg, 0, 1);
        }
    // the calendar: this is the cue
    Rectangle cal = { cx + 8.5f * k, b.y + 0.9f * k, 6.2f * k, 6.6f * k };
    DrawRectangleRec(cal, C(P->text));
    DrawRectangle((int)cal.x, (int)cal.y, (int)cal.width, (int)(1.3f * k), C(sat ? P->ok : P->bad));
    label(sat ? "SATURDAY" : "MONDAY", cal.x + cal.width / 2, cal.y + 0.3f * k, 0.7f * k, P->bg, 0, 1);
    label(sat ? "SAT" : "MON", cal.x + cal.width / 2, cal.y + 1.6f * k, 2.4f * k, P->bg, 0, 1);
    if (sat) {
        DrawCircleV(v2(cal.x + cal.width / 2, cal.y + 5.3f * k), 0.55f * k, C(P->warn));
        for (int i = 0; i < 8; i++) {
            float a = i * 45 * DEG2RAD;
            DrawLineEx(v2(cal.x + cal.width / 2 + cosf(a) * 0.75f * k, cal.y + 5.3f * k + sinf(a) * 0.75f * k),
                       v2(cal.x + cal.width / 2 + cosf(a) * 1.05f * k, cal.y + 5.3f * k + sinf(a) * 1.05f * k), 3, C(P->warn));
        }
    } else {
        label("9:00 MEETING", cal.x + cal.width / 2, cal.y + 4.6f * k, 0.75f * k, P->bad, 0, 1);
        DrawRing(v2(cal.x + cal.width / 2, cal.y + 5.0f * k), 2.6f * k, 2.75f * k, 0, 360, 40, CA(P->bad, 0.7f));
    }
    // the bed
    float bx = cx - 12 * k, bw = 17 * k;
    DrawRectangleRounded((Rectangle){ bx - 0.7f * k, gy - 4.8f * k, 0.9f * k, 4.8f * k }, 0.4f, 6, C(P->dim));
    DrawRectangleRounded((Rectangle){ bx, gy - 2.4f * k, bw, 1.5f * k }, 0.3f, 6, C(P->dim));
    DrawRectangle((int)(bx + bw - 0.6f * k), (int)(gy - 1.2f * k), (int)(0.5f * k), (int)(1.2f * k), C(P->dim));
    DrawRectangleRounded((Rectangle){ bx + 0.3f * k, gy - 3.3f * k, 3.0f * k, 1.0f * k }, 0.6f, 8, CA(P->text, 0.9f));
    Vector2 head = v2(bx + 1.9f * k, gy - 3.4f * k);
    if (!up) {
        float breathe = sinf((float)t * 1.6f) * 0.12f * k;
        if (is(show, "act")) {          // fighting the blanket
            breathe = (0.3f + P->act) * k * fabsf(sinf((float)t * 14));
            head.y -= P->act * 1.2f * k;
        }
        DrawCircleV(head, 0.8f * k, C(P->accent));
        DrawRectangleRounded((Rectangle){ bx + 2.6f * k, gy - 3.7f * k - breathe, bw - 3.1f * k, 1.5f * k + breathe }, 0.7f, 10, CA(P->accent, 0.55f));
        face(head, k, 2, P->bg);
    } else {
        DrawRectangleRounded((Rectangle){ bx + 6 * k, gy - 2.9f * k, bw - 6.5f * k, 0.6f * k }, 0.7f, 10, CA(P->accent, 0.55f));
        float px = cx + 7.2f * k;
        Vector2 sh = person(px, gy, k, -1, 0, 0, C(P->accent));
        float s = sinf((float)tw * 4) * 0.3f * k;
        limb(sh, v2(px - 1.0f * k + s, sh.y - 2.2f * k), 0.28f * k, C(P->accent));      // a long stretch
        limb(sh, v2(px + 1.0f * k - s, sh.y - 2.2f * k), 0.28f * k, C(P->accent));
        face(person_head, k, sat ? 0 : 1, P->bg);
    }
    // the nightstand and the clock
    float nx = cx - 16.6f * k;
    DrawRectangle((int)nx, (int)(gy - 2.6f * k), (int)(3.4f * k), (int)(2.6f * k), C(P->dim));
    float shake = ringing ? sinf((float)t * 45) * 0.12f * k : 0;
    Vector2 ck = v2(nx + 1.7f * k + shake, gy - 3.6f * k);
    DrawCircleV(v2(ck.x - 0.65f * k, ck.y - 0.75f * k), 0.35f * k, C(P->warn));
    DrawCircleV(v2(ck.x + 0.65f * k, ck.y - 0.75f * k), 0.35f * k, C(P->warn));
    DrawCircleV(ck, 0.95f * k, C(P->warn));
    DrawCircleV(ck, 0.75f * k, C(P->text));
    DrawLineEx(ck, v2(ck.x, ck.y - 0.6f * k), 3, C(P->bg));                     // 7:00
    DrawLineEx(ck, v2(ck.x - 0.25f * k, ck.y + 0.35f * k), 4, C(P->bg));
    if (ringing) buzz(ck.x, ck.y, 1.3f * k, P->warn);
    if (snooze) {
        float f = ease((float)tw / 0.4f) * (1 - ease(((float)tw - 0.8f) / 0.4f));
        limb(v2(head.x - 0.2f * k, head.y + 0.4f * k), v2(lerp(head.x, ck.x, f), lerp(head.y + 0.4f * k, ck.y - 0.9f * k, f)), 0.26f * k, C(P->accent));
    }
    if (!up && (snooze || is(show, "missed") || is(show, "knock")))
        drift("z", head.x + 0.6f * k, head.y - 0.8f * k, k, P->text, t);
    if (is(show, "knock")) {                // the neighbor, through the wall
        float a = fmodf((float)tw, 1.2f) < 0.5f ? 1 : 0.2f;
        label("BANG", b.x + b.width - 2.3f * k, b.y + 2.0f * k, 1.0f * k, CA(P->bad, a), 0, 1);
        label("BANG", b.x + b.width - 2.3f * k, b.y + 3.6f * k, 1.0f * k, CA(P->bad, 1.2f - a), 0, 1);
    }
    if (is(show, "missed")) {               // the phone on the nightstand, lighting up
        DrawRectangleRounded((Rectangle){ nx + 0.4f * k, gy - 3.0f * k, 1.3f * k, 0.4f * k }, 0.4f, 4, C(P->text));
        DrawCircleV(v2(nx + 1.8f * k, gy - 3.2f * k), 0.32f * k, C(P->bad));
        label("4", nx + 1.8f * k, gy - 3.5f * k, 0.5f * k, P->text, 0, 1);
    }
}

/* ---------- the stove, and the smell afterwards ---------- */

STAGE(st_kitchen) {
    float k = K, cx = CX, gy = b.y + b.height * 0.9f;
    double t = GetTime();
    int smell = has_key(props, "smell"), rising = prop_is(props, "milk", "rising");
    floor_band(b, gy);
    // tiles behind the stove
    for (float x = cx - 9 * k; x < cx + 4 * k; x += 1.1f * k)
        DrawLineEx(v2(x, b.y + 2 * k), v2(x, gy - 4.3f * k), 1, CA(P->faint, 0.6f));
    for (float y = b.y + 2 * k; y < gy - 4.3f * k; y += 1.1f * k)
        DrawLineEx(v2(cx - 9 * k, y), v2(cx + 4 * k, y), 1, CA(P->faint, 0.6f));
    int open = is(show, "air_out");
    window_((Rectangle){ cx + 8 * k, b.y + 1 * k, 7 * k, 4.6f * k }, P->accent, open);
    // the stove
    Rectangle st = { cx - 7 * k, gy - 4.3f * k, 9 * k, 4.3f * k };
    DrawRectangleRec(st, C(P->dim));
    DrawRectangle((int)(st.x + 0.8f * k), (int)(st.y + 1.6f * k), (int)(st.width - 1.6f * k), (int)(2.2f * k), CA(P->bg, 0.5f));
    for (int i = 0; i < 4; i++) DrawCircleV(v2(st.x + 1.2f * k + i * 2.2f * k, st.y + 0.8f * k), 0.32f * k, C(P->bg));
    // the pot
    Rectangle pot = { cx - 5.6f * k, st.y - 2.2f * k, 6 * k, 2.1f * k };
    int flame = !smell && !is(show, "off");
    if (flame)
        for (int i = 0; i < 6; i++) {
            float fx = pot.x + 0.6f * k + i * 0.95f * k, h = (0.45f + 0.2f * sinf((float)t * 20 + i * 2)) * k;
            tri(v2(fx - 0.25f * k, st.y), v2(fx + 0.25f * k, st.y), v2(fx, st.y - h), C(P->accent));
        }
    DrawLineEx(v2(pot.x, pot.y + 0.4f * k), v2(pot.x - 2.2f * k, pot.y + 0.1f * k), 0.3f * k, C(P->text));
    DrawRectangleRec(pot, smell ? C(P->bg) : CA(P->text, 0.85f));
    if (smell) {            // what is left of the milk: a black ring and the smell
        for (int i = 0; i < 5; i++) {
            float f = wrapf((float)t * 0.12f + i * 0.2f, 1), dx = open ? f * 18 * k : sinf(f * 7 + i) * 3 * k;
            float a = open ? (1 - f) * 0.5f : 0.4f;
            DrawCircleV(v2(cx - 12 * k + i * 5 * k + dx, b.y + 3 * k + sinf((float)t + i) * k), (1.6f + i % 2) * k, CA(P->dim, a));
        }
        drift("~", pot.x + 3 * k, pot.y - 0.5f * k, k, P->dim, t);
        Vector2 sh = person(cx - 14 * k, gy, k, 1, 0, 0, C(P->accent));
        face(person_head, k, -1, P->bg);
        if (is(show, "mask")) {         // spray: pink clouds into the grey
            limb(sh, v2(cx - 12.6f * k, sh.y - 0.8f * k), 0.28f * k, C(P->accent));
            DrawRectangle((int)(cx - 12.8f * k), (int)(sh.y - 1.7f * k), (int)(0.5f * k), (int)(1.1f * k), C(P->text));
            for (int i = 0; i < 6; i++)
                DrawCircleV(v2(cx - 11 * k + i * 1.7f * k, sh.y - 1.5f * k + sinf((float)t * 2 + i) * 0.5f * k),
                            0.9f * k * ease((float)tw / 0.6f), CA(P->bad, 0.25f));
        } else {
            limb(sh, v2(person_head.x + 0.5f * k, person_head.y + 0.2f * k), 0.28f * k, C(P->accent));    // holding the nose
        }
        return;
    }
    // the milk
    if (rising) {
        float h = ease((float)ts / 2.5f) * 1.4f * k;
        int spill = is(show, "boil_over");
        if (is(show, "off")) h *= 1 - ease((float)tw / 1.2f);
        if (spill) h = 1.8f * k;
        for (int i = 0; i < 8; i++) {
            float fx = pot.x + 0.35f * k + i * 0.76f * k, bob = sinf((float)t * 7 + i * 1.3f) * 0.12f * k;
            DrawCircleV(v2(fx, pot.y - h * 0.5f + bob), 0.5f * k + h * 0.25f, C(P->text));
        }
        if (spill) {                    // over the edge and down the stove
            float d = ease((float)tw / 1.5f) * 2.6f * k;
            DrawRectangleRounded((Rectangle){ pot.x - 0.3f * k, pot.y - 0.2f * k, 0.6f * k, d }, 1, 6, C(P->text));
            DrawRectangleRounded((Rectangle){ pot.x + pot.width - 0.3f * k, pot.y - 0.2f * k, 0.6f * k, d * 0.8f }, 1, 6, C(P->text));
            DrawRectangleRounded((Rectangle){ pot.x - 1.2f * k, st.y - 0.25f * k, (pot.width + 2.4f * k) * ease((float)tw / 1.5f), 0.4f * k }, 1, 6, C(P->text));
            drift("~", pot.x + 3 * k, pot.y - 1.5f * k, k, P->dim, t);
        }
    } else {
        DrawRectangle((int)(pot.x + 0.15f * k), (int)(pot.y + 0.2f * k), (int)(pot.width - 0.3f * k), (int)(0.25f * k), C(P->text));
        if (is(show, "warm")) drift("~", pot.x + 2.5f * k, pot.y - 0.4f * k, k, P->text, t);
    }
    // you, by the stove
    Vector2 sh = person(cx - 13 * k, gy, k, 1, 0, 0, C(P->accent));
    if (is(show, "warm")) {         // on the phone while it warms up
        limb(sh, v2(sh.x + 0.7f * k, sh.y - 0.7f * k), 0.28f * k, C(P->accent));
        DrawRectangleRounded((Rectangle){ sh.x + 0.5f * k, sh.y - 1.6f * k, 0.7f * k, 1.1f * k }, 0.3f, 4, C(P->text));
    } else {
        limb(sh, v2(sh.x + 1.0f * k, sh.y + 1.2f * k), 0.28f * k, C(P->accent));
    }
    face(person_head, k, is(show, "boil_over") ? -1 : 0, P->bg);
}

/* ---------- the fridge ---------- */

STAGE(st_fridge) {
    float k = K, cx = CX, gy = b.y + b.height * 0.93f;
    double t = GetTime();
    int green = prop_is(props, "sausage", "green");
    floor_band(b, gy);
    Rectangle f = { cx - 4.5f * k, b.y + 0.5f * k, 9 * k, gy - b.y - 0.5f * k };
    DrawRectangleRec(f, C(P->text));
    DrawRectangleRec((Rectangle){ f.x + 0.4f * k, f.y + 0.4f * k, f.width - 0.8f * k, f.height - 0.8f * k }, CA(P->warn, 0.25f));
    for (int i = 1; i <= 3; i++) DrawLineEx(v2(f.x + 0.4f * k, f.y + i * 2.4f * k), v2(f.x + f.width - 0.4f * k, f.y + i * 2.4f * k), 3, C(P->dim));
    // the open door with bottles on it
    tri(v2(f.x + f.width, f.y), v2(f.x + f.width + 3.2f * k, f.y + 0.8f * k), v2(f.x + f.width, f.y + f.height), CA(P->text, 0.8f));
    tri(v2(f.x + f.width + 3.2f * k, f.y + 0.8f * k), v2(f.x + f.width + 3.2f * k, f.y + f.height - 0.8f * k), v2(f.x + f.width, f.y + f.height), CA(P->text, 0.8f));
    for (int i = 0; i < 3; i++)
        DrawRectangleRounded((Rectangle){ f.x + f.width + 0.6f * k + i * 0.8f * k, f.y + 1.6f * k, 0.5f * k, 1.4f * k }, 0.5f, 4, CA(i == 1 ? P->ok : P->accent, 0.7f));
    // a few things, and the sausage in the middle
    DrawRectangleRounded((Rectangle){ f.x + 0.8f * k, f.y + 1.2f * k, 1.6f * k, 1.2f * k }, 0.3f, 4, C(P->warn));
    DrawCircleV(v2(f.x + 6.6f * k, f.y + 6.6f * k), 0.8f * k, CA(P->ok, 0.8f));
    Color sc = green ? (Color){ 150, 170, 90, 255 } : (Color){ 210, 98, 98, 255 };
    Vector2 home = v2(cx, f.y + 4.4f * k), at = home;
    Vector2 hand = v2(cx - 9.6f * k, gy - 3.6f * k);
    int eat = is(show, "eat"), throw_ = is(show, "throw");
    if (eat) at = v2(lerp(home.x, hand.x + 0.6f * k, ease((float)tw / 0.6f)), lerp(home.y, hand.y - 0.8f * k, ease((float)tw / 0.6f)));
    float binx = b.x + b.width - 4 * k;
    if (throw_) {
        float f2 = ease((float)tw / 0.9f);
        at = v2(lerp(home.x, binx, f2), lerp(home.y, gy - 2.2f * k, f2) - sinf(f2 * PI) * 3 * k);
    }
    if (!(throw_ && tw > 0.9)) {
        DrawRectanglePro((Rectangle){ at.x, at.y, 4 * k, 0.9f * k }, v2(2 * k, 0.45f * k), throw_ ? (float)tw * 400 : 0, C(sc));
        if (!eat && !throw_) {
            if (green) {        // the smell, and the flies that like it
                drift("~", at.x - 1 * k, at.y - 0.6f * k, k, (Color){ 150, 170, 90, 255 }, t);
                for (int i = 0; i < 3; i++) {
                    float a = (float)t * (3 + i) + i * 2;
                    DrawCircleV(v2(at.x + cosf(a) * 2.2f * k, at.y - 1.2f * k + sinf(a * 1.3f) * 0.8f * k), 0.13f * k, C(P->bg));
                }
            } else {
                drift("*", at.x - 1 * k, at.y - 0.6f * k, k, P->warn, t);
            }
        }
    }
    // the bin, if it is needed
    if (throw_) {
        DrawRectangleRounded((Rectangle){ binx - 1.3f * k, gy - 2.6f * k, 2.6f * k, 2.6f * k }, 0.2f, 4, C(P->dim));
        DrawRectangle((int)(binx - 1.5f * k), (int)(gy - 2.9f * k), (int)(3 * k), (int)(0.4f * k), C(P->dim));
        if (green && tw > 0.9) face(v2(binx, gy - 1.4f * k), k, -1, P->bg);
    }
    // you
    float px = cx - 11 * k;
    Vector2 sh = person(px, gy, k, 1, 0, 0, C(P->accent));
    limb(sh, eat ? v2(hand.x + 0.4f * k, hand.y - 0.8f * k) : hand, 0.28f * k, C(P->accent));
    int mood = eat ? (green ? -1 : 1) : 0;
    face(person_head, k, mood, P->bg);
    if (eat && green && tw > 1) buzz(person_head.x, person_head.y, 1.0f * k, P->ok);
}

/* ---------- the peephole: someone at the door ---------- */

static void hat(Vector2 head, float k, Color c) {
    DrawRectangle((int)(head.x - 0.7f * k), (int)(head.y - 0.75f * k), (int)(1.4f * k), (int)(0.45f * k), c);
    DrawRectangle((int)(head.x), (int)(head.y - 0.42f * k), (int)(0.95f * k), (int)(0.15f * k), c);
}

STAGE(st_peephole) {
    float k = K, cx = CX, cy = b.y + b.height * 0.5f;
    double t = GetTime();
    int open = is(show, "open"), leave = is(show, "leave"), soup = is(show, "soup_left");
    int night = prop_is(props, "who", "mom");
    float R = 4.7f * k;
    if (open) R = lerp(R, b.width * 0.62f, ease((float)tw / 0.6f));
    // the landing behind the door
    DrawRectangleRec(b, night ? CA(P->accent, 0.12f) : CA(P->faint, 0.7f));
    float fk = k * 0.8f, gy = cy + 4.3f * k;
    DrawRectangle((int)b.x, (int)gy, (int)b.width, (int)(b.height), CA(P->dim, 0.5f));
    float gone = (leave || soup) ? ease((float)tw / 1.2f) : 0;
    float shift = gone * 7 * k;
    Color pc = CA(P->cue, 1 - gone * 0.9f);
    if (prop_is(props, "who", "courier")) {
        Vector2 sh = person(cx + shift, gy, fk * 1.2f, 1, (float)tw * 8, gone > 0, pc);
        hat(person_head, fk * 1.2f, CA(P->accent, 1 - gone));
        float grow = open ? ease((float)tw / 0.8f) : 0;
        Rectangle box = { cx - 1.4f * fk - grow * 2 * k + shift, sh.y - 0.4f * fk, (2.8f + grow * 3) * fk, (2.2f + grow * 2) * fk };
        limb(sh, v2(box.x + box.width / 2, box.y + box.height / 2), 0.3f * fk, pc);
        DrawRectangleRec(box, CA(P->warn, 1 - gone));
        DrawLineEx(v2(box.x + box.width / 2, box.y), v2(box.x + box.width / 2, box.y + box.height), 3, CA(P->bg, 0.5f * (1 - gone)));
        face(person_head, fk * 1.2f, 1, CA(P->bg, 1 - gone));
    } else if (prop_is(props, "who", "pamphlets") || prop_is(props, "who", "band")) {
        int band = prop_is(props, "who", "band");
        int n = band ? 4 : 2;
        for (int i = 0; i < n; i++) {
            float px = cx + (i - (n - 1) / 2.0f) * 2.6f * k + shift;
            Vector2 sh = person(px, gy, fk * 1.15f, 1, (float)tw * 8 + i, gone > 0, pc);
            face(person_head, fk * 1.15f, 1, CA(P->bg, 1 - gone));
            DrawCircleV(v2(person_head.x, person_head.y + 0.25f * fk), 0.3f * fk, CA(P->bg, 0.8f * (1 - gone)));  // big smiles
            if (band && i == 1) {           // the guitar
                DrawCircleV(v2(sh.x + 0.4f * fk, sh.y + 2.3f * fk), 1.0f * fk, CA(P->warn, 1 - gone));
                DrawLineEx(v2(sh.x + 0.4f * fk, sh.y + 2.3f * fk), v2(sh.x + 2.6f * fk, sh.y - 0.6f * fk), 0.3f * fk, CA(P->warn, 1 - gone));
            } else {
                limb(sh, v2(sh.x + 0.9f * fk, sh.y + 1.2f * fk), 0.28f * fk, pc);
                for (int j = 0; j < 3; j++)
                    DrawRectangle((int)(sh.x + 0.6f * fk), (int)(sh.y + (0.9f - j * 0.2f) * fk), (int)(1.1f * fk), (int)(0.5f * fk), CA(P->text, 1 - gone));
            }
        }
        if (band && !leave) drift("o", cx, gy - 7 * fk, k, P->text, t);
        if (open) {
            Rectangle bb = { cx - 6 * k, b.y + 0.6f * k, 12 * k, 1.7f * k };
            bubble(bb, cx, b.y + 3.2f * k, P->text);
            label(band ? "SING WITH US!" : "THE END IS NEAR!", cx, bb.y + 0.35f * k, 0.9f * k, P->bg, 0, 1);
        }
    } else if (prop_is(props, "who", "fog")) {
        Vector2 sh = person(cx + shift, gy, fk * 1.2f, 1, (float)tw * 8, gone > 0, pc);
        if (open) {     // the neighbor, with an empty salt shaker
            face(person_head, fk * 1.2f, 1, P->bg);
            limb(sh, v2(sh.x + 1.2f * fk, sh.y - 0.4f * fk), 0.3f * fk, pc);
            DrawRectangleRounded((Rectangle){ sh.x + 1.0f * fk, sh.y - 1.3f * fk, 0.7f * fk, 1.1f * fk }, 0.3f, 4, C(P->text));
        }
        float clear = open ? ease((float)tw / 0.6f) : 0;
        for (int i = 0; i < 14; i++) {      // fog on the glass
            float a = i * 2.4f + (float)t * 0.2f;
            DrawCircleV(v2(cx + cosf(a) * (1 + i % 4) * k, cy + sinf(a * 1.3f) * (1 + i % 3) * k), (1.6f + (i % 3) * 0.5f) * k,
                        CA(P->text, 0.22f * (1 - clear)));
        }
    } else if (night) {
        Vector2 sh = person(cx + shift, gy, fk * 1.2f, 1, (float)tw * 8, gone > 0, pc);
        DrawCircleV(v2(person_head.x - 0.3f * fk, person_head.y - 0.9f * fk), 0.45f * fk * 1.2f, pc);   // her bun
        face(person_head, fk * 1.2f, open ? 1 : 2, CA(P->bg, 1 - gone));
        float px = soup ? cx + 1.0f * k : sh.x + 1.0f * fk, py = soup ? gy - 0.8f * k : sh.y + 0.6f * fk;
        if (!soup || gone < 1) limb(sh, v2(sh.x + 1.0f * fk + shift * 0, sh.y + 0.8f * fk), 0.3f * fk, pc);
        DrawRectangleRounded((Rectangle){ px - 0.9f * k, py, 1.8f * k, 1.0f * k }, 0.3f, 4, C(P->text));       // the soup
        drift("~", px, py - 0.4f * k, k * 0.7f, P->text, t);
        if (soup && tw > 1.2)
            DrawRectangle((int)(px + 1.1f * k), (int)(py + 0.3f * k), (int)(1.2f * k), (int)(0.8f * k), C(P->warn));   // the note
        if (open) heart(cx + 2.5f * k, gy - 6 * k - (float)wrapf((float)tw, 2) * k, 0.4f * k, P->bad);
    }
    // the door around the glass
    Color door = night ? (Color){ 40, 34, 30, 255 } : (Color){ 72, 54, 40, 255 };
    DrawRing(v2(cx, cy), R, R + b.width, 0, 360, 96, C(door));
    DrawRing(v2(cx, cy), R - 0.3f * k, R + 0.15f * k, 0, 360, 96, C(P->dim));
    DrawRing(v2(cx, cy), R - 1.4f * k, R - 1.1f * k, 200, 250, 24, CA(P->text, 0.25f));       // glare
    if (!show && ts < 2.0 && fmodf((float)ts, 0.6f) < 0.35f) {
        label("knock", cx + R + 3 * k, cy - 2 * k, 1.0f * k, P->text, 0, 1);
        label("knock", cx - R - 3 * k, cy + 1 * k, 1.0f * k, P->text, 0, 1);
    }
    if (night && !open) label("11:00 PM", b.x + 1.2f * k, b.y + 0.8f * k, 0.9f * k, P->dim, -1, 1);
}

/* ---------- the doormat: a letter, a flyer ---------- */

STAGE(st_doormat) {
    float k = K, cx = CX;
    double t = GetTime();
    for (float y = b.y + 2.2f * k; y < b.y + b.height; y += 1.4f * k)       // floorboards
        DrawLineEx(v2(b.x, y), v2(b.x + b.width, y), 2, CA(P->faint, 0.7f));
    DrawRectangle((int)b.x, (int)b.y, (int)b.width, (int)(2.2f * k), C(P->dim));     // the bottom of the door
    DrawRectangle((int)b.x, (int)(b.y + 2.1f * k), (int)b.width, (int)(0.15f * k), CA(P->warn, 0.6f));
    Rectangle mat = { cx - 10 * k, b.y + 3.0f * k, 20 * k, 7.2f * k };
    DrawRectangleRounded(mat, 0.1f, 6, C((Color){ 112, 82, 52, 255 }));
    DrawRectangleRoundedLinesEx((Rectangle){ mat.x + 0.4f * k, mat.y + 0.4f * k, mat.width - 0.8f * k, mat.height - 0.8f * k }, 0.1f, 6, 3, C((Color){ 150, 115, 75, 255 }));
    label("WELCOME", cx, mat.y + mat.height - 1.6f * k, 1.0f * k, (Color){ 150, 115, 75, 255 }, 0, 1);
    if (has_key(props, "letter")) {
        int yes = is(show, "accept"), no = is(show, "drawer"), hog = is(show, "hedgehog");
        float slide = no ? ease((float)tw / 0.8f) : 0;
        Rectangle e = { cx - 3.8f * k, b.y + 3.8f * k + slide * 8 * k, 7.6f * k, 4.4f * k };
        if (hog) {          // the letter folds itself into a hedgehog and walks off
            float hx = cx + (float)tw * 3 * k, hy = b.y + 6.5f * k;
            for (int i = 0; i < 6; i++)
                tri(v2(hx - 1.6f * k + i * 0.55f * k, hy), v2(hx - 1.3f * k + i * 0.55f * k, hy - 1.2f * k), v2(hx - 1.0f * k + i * 0.55f * k, hy), C(P->text));
            DrawRectangleRounded((Rectangle){ hx - 1.8f * k, hy - 0.2f * k, 3.8f * k, 1.0f * k }, 0.6f, 6, C(P->text));
            DrawCircleV(v2(hx + 1.6f * k, hy + 0.1f * k), 0.12f * k, C(P->bg));
            float st = sinf((float)tw * 14) * 0.3f * k;
            DrawLineEx(v2(hx - 1 * k, hy + 0.8f * k), v2(hx - 1 * k + st, hy + 1.3f * k), 3, C(P->text));
            DrawLineEx(v2(hx + 1 * k, hy + 0.8f * k), v2(hx + 1 * k - st, hy + 1.3f * k), 3, C(P->text));
            return;
        }
        if (yes) {          // the letter comes out
            float up = ease((float)tw / 0.8f) * 3 * k;
            DrawRectangle((int)(e.x + 0.6f * k), (int)(e.y + 0.4f * k - up), (int)(e.width - 1.2f * k), (int)(3.4f * k), C(P->text));
            for (int i = 0; i < 4; i++)
                DrawLineEx(v2(e.x + 1.2f * k, e.y + 1.0f * k - up + i * 0.6f * k), v2(e.x + e.width - 1.2f * k - (i == 3) * 2 * k, e.y + 1.0f * k - up + i * 0.6f * k), 2, C(P->dim));
        }
        DrawRectangleRec(e, CA(P->text, 1 - slide));
        tri(v2(e.x, e.y), v2(e.x + e.width, e.y), v2(cx, e.y + e.height * 0.55f), CA((Color){ 205, 200, 190, 255 }, 1 - slide));
        if (!yes) {
            DrawCircleV(v2(cx, e.y + e.height * 0.55f), 0.75f * k, CA(P->bad, 1 - slide));      // a wax seal
            label("W", cx, e.y + e.height * 0.55f - 0.42f * k, 0.8f * k, CA(P->bg, 1 - slide), 0, 1);
        }
    } else if (has_key(props, "flyer")) {
        int crumple = is(show, "stay_home");
        Rectangle fl = { cx - 5 * k, b.y + 3.4f * k, 10 * k, 6.4f * k };
        if (crumple && tw > 0.5) {
            DrawCircleV(v2(cx, b.y + 7 * k), 1.2f * k, C(P->text));
            drift("~", cx + 4 * k, b.y + 6 * k, k, P->dim, t);
            return;
        }
        DrawRectanglePro((Rectangle){ cx, b.y + 6.6f * k, fl.width, fl.height }, v2(fl.width / 2, fl.height / 2), -4, C(P->text));
        // a tuba
        DrawRing(v2(cx - 2.6f * k, b.y + 6.8f * k), 0.9f * k, 1.3f * k, 0, 360, 32, C(P->warn));
        DrawRing(v2(cx - 2.6f * k, b.y + 6.8f * k), 1.6f * k, 2.0f * k, 200, 340, 32, C(P->warn));
        DrawCircleV(v2(cx - 4.2f * k, b.y + 5.3f * k), 0.8f * k, C(P->warn));
        label("TUBA", cx + 1.8f * k, b.y + 4.0f * k, 1.3f * k, P->bg, 0, 1);
        label("NIGHT 8 PM", cx + 1.8f * k, b.y + 5.6f * k, 0.8f * k, P->bg, 0, 1);
        Font f = P->font;
        DrawTextPro(f, "for my lift buddy!", v2(cx - 0.6f * k, b.y + 7.4f * k), v2(0, 0), -8, 0.65f * k, 1, C(P->accent));
        if (is(show, "go")) drift("o", cx + 3 * k, b.y + 4 * k, k, P->warn, t);
    }
}

/* ---------- the phone: calls and messages ---------- */

static void msg(const char *s, float x, float y, float k, int mine) {
    float size = 0.75f * k, w = MeasureTextEx(P->font, s, size, size * 0.02f).x + 1.2f * k;
    Rectangle r = { mine ? x - w : x, y, w, 1.4f * k };
    DrawRectangleRounded(r, 0.6f, 8, C(mine ? P->accent : P->text));
    label(s, r.x + 0.6f * k, y + 0.3f * k, size, P->bg, -1, 0);
}

STAGE(st_phone) {
    float k = K, cx = CX;
    double t = GetTime();
    if (prop_is(props, "place", "shower")) {
        // tiles, water, you full of shampoo, and the phone ringing on the sink
        for (float x = b.x; x < b.x + b.width; x += 1.3f * k) DrawLineEx(v2(x, b.y), v2(x, b.y + b.height), 1, CA(P->faint, 0.6f));
        for (float y = b.y; y < b.y + b.height; y += 1.3f * k) DrawLineEx(v2(b.x, y), v2(b.x + b.width, y), 1, CA(P->faint, 0.6f));
        float px = cx - 7 * k, gy = b.y + b.height * 0.95f;
        DrawRectangle((int)(px - 2.4f * k), (int)(b.y + 0.6f * k), (int)(0.4f * k), (int)(1.2f * k), C(P->dim));
        for (int i = 0; i < 9; i++) {
            float x = px - 2.4f * k + i * 0.55f * k, f = wrapf((float)t * 2 + i * 0.37f, 1);
            DrawLineEx(v2(x, b.y + 1.8f * k + f * 6 * k), v2(x + 0.1f * k, b.y + 2.4f * k + f * 6 * k), 2, CA(P->accent, 0.7f));
        }
        Vector2 sh = person(px, gy, k, 1, 0, 0, C(P->accent));
        for (int i = 0; i < 6; i++)
            DrawCircleV(v2(person_head.x - 0.7f * k + i * 0.3f * k, person_head.y - 0.6f * k + sinf(i * 2.0f) * 0.25f * k), 0.35f * k, C(P->text));
        face(person_head, k, 2, P->bg);
        int answer = is(show, "drop_in_bath"), decline = is(show, "decline");
        Vector2 ph = v2(cx + 9 * k, b.y + 5.5f * k);
        if (answer) {
            float f = ease((float)tw / 0.7f);
            ph = v2(lerp(ph.x, px + 1.5f * k, f), lerp(ph.y, gy - 0.5f * k, f));
            limb(sh, v2(px + 1.3f * k, sh.y + 0.3f * k), 0.28f * k, C(P->accent));
            if (tw > 0.7) for (int i = 0; i < 4; i++)
                DrawCircleV(v2(ph.x + (i - 1.5f) * 0.8f * k, ph.y - wrapf((float)tw * 2 + i * 0.3f, 1) * 1.5f * k), 0.2f * k, CA(P->accent, 0.8f));
        } else {
            limb(sh, v2(person_head.x + 0.2f * k, person_head.y - 0.6f * k), 0.28f * k, C(P->accent));     // rubbing the eyes
            DrawRectangle((int)(cx + 7 * k), (int)(b.y + 6.4f * k), (int)(4 * k), (int)(0.4f * k), C(P->dim));     // the sink shelf
        }
        DrawRectangleRounded((Rectangle){ ph.x - 0.5f * k, ph.y - 0.9f * k, 1.0f * k, 1.8f * k }, 0.3f, 4, C(P->bg));
        DrawRectangle((int)(ph.x - 0.4f * k), (int)(ph.y - 0.75f * k), (int)(0.8f * k), (int)(1.4f * k), C(decline ? P->bad : P->ok));
        if (!show) buzz(ph.x, ph.y, 1.2f * k, P->text);
        return;
    }
    // a phone held sideways, big
    Rectangle ph = { cx - 11 * k, b.y + 0.5f * k, 22 * k, b.height - 1.0f * k };
    int ringing = !show || is(show, "again");
    float shake = ringing && has_key(props, "call") ? sinf((float)t * 50) * 0.1f * k : 0;
    ph.x += shake;
    DrawRectangleRounded(ph, 0.12f, 10, C(P->bg));
    Rectangle sc = { ph.x + 0.6f * k, ph.y + 0.5f * k, ph.width - 1.2f * k, ph.height - 1.0f * k };
    if (has_key(props, "chat")) {
        DrawRectangleRounded(sc, 0.08f, 8, CA(P->faint, 0.9f));
        DrawCircleV(v2(sc.x + 1.0f * k, sc.y + 0.9f * k), 0.5f * k, C(P->cue));
        label("Max", sc.x + 1.8f * k, sc.y + 0.5f * k, 0.8f * k, P->text, -1, 1);
        float y = sc.y + 2.0f * k, l = sc.x + 0.6f * k, r = sc.x + sc.width - 0.6f * k;
        if (prop_is(props, "chat", "karaoke")) {       // what happened last time, further up the chat
            label("last week", sc.x + sc.width / 2, y - 0.1f * k, 0.55f * k, P->dim, 0, 0);
            msg("anything", r, y + 0.6f * k, k, 1);
            msg("raw vegan karaoke!! :D", l, y + 2.2f * k, k, 0);
            y += 4.1f;
            y = sc.y + 6.2f * k;
            msg("dinner? where?", l, y, k, 0);
        } else {
            msg("dinner? where?", l, y, k, 0);
            msg("I found a great new place!!", l, y + 1.8f * k, k, 0);
            drift("*", sc.x + 11 * k, y + 2 * k, k, P->warn, t);
            y += 1.8f * k;
        }
        if (is(show, "pizza")) msg("pizza", r, y + 1.9f * k, k, 1);
        if (is(show, "anything")) msg("anything", r, y + 1.9f * k, k, 1);
        if (tw > 1.0 && (is(show, "pizza") || is(show, "anything")))
            msg(prop_is(props, "chat", "karaoke") == is(show, "pizza") ? "!!!" : "oh. okay.", l, y + 3.6f * k, k, 0);
        return;
    }
    int vet = prop_is(props, "call", "vet");
    int answered = is(show, "answer"), declined = is(show, "decline"), paid = is(show, "pay"), haggle = is(show, "haggle");
    Color bgc = answered || paid ? P->ok : declined ? P->bad : P->faint;
    DrawRectangleRounded(sc, 0.08f, 8, CA(bgc, answered || declined || paid ? 0.35f : 0.9f));
    float ax = sc.x + 4 * k, ay = sc.y + sc.height / 2;
    DrawCircleV(v2(ax, ay), 2.2f * k, C(vet ? P->ok : P->cue));
    label(vet ? "+" : "M", ax, ay - 1.1f * k, 2.2f * k, P->bg, 0, 1);
    label(vet ? "VET" : "MOM", sc.x + 7.5f * k, sc.y + 1.4f * k, 1.8f * k, P->text, -1, 1);
    char line[64];
    if (answered) snprintf(line, sizeof line, "00:%02d", (int)tw % 60);
    else if (declined) snprintf(line, sizeof line, "declined");
    else snprintf(line, sizeof line, "calling...");
    label(line, sc.x + 7.5f * k, sc.y + 3.6f * k, 0.9f * k, P->text, -1, 0);
    if (prop_is(props, "missed", "3")) {        // the cue: she has called before
        int n = 3 + (is(show, "again") ? 1 + (int)(tw / 0.8) % 4 : 0);
        snprintf(line, sizeof line, "%d missed calls", n);
        DrawRectangleRounded((Rectangle){ sc.x + 7.3f * k, sc.y + 5.2f * k, 7.6f * k, 1.4f * k }, 0.6f, 8, C(P->bad));
        label(line, sc.x + 7.8f * k, sc.y + 5.45f * k, 0.8f * k, P->text, -1, 1);
    }
    if (answered && tw > 0.6) {
        Rectangle bb = { sc.x + 7.3f * k, sc.y + 6.6f * k, 11 * k, 1.6f * k };
        bubble(bb, ax + 2 * k, ay + 1 * k, P->text);
        label("FINALLY! Did you eat?", bb.x + 0.5f * k, bb.y + 0.35f * k, 0.8f * k, P->bg, -1, 1);
    }
    if (vet) {          // the bill, next to the phone
        Rectangle bill = { sc.x + sc.width - 6.2f * k, sc.y + 0.6f * k, 5.4f * k, sc.height - 1.2f * k };
        float grow = haggle ? ease((float)tw / 0.8f) * 1.2f * k : 0;
        DrawRectangleRec((Rectangle){ bill.x, bill.y, bill.width, bill.height + grow - 1.0f * k }, C(P->text));
        label("BILL", bill.x + bill.width / 2, bill.y + 0.3f * k, 0.8f * k, P->bg, 0, 1);
        for (int i = 0; i < 3; i++) DrawLineEx(v2(bill.x + 0.5f * k, bill.y + 1.6f * k + i * 0.7f * k), v2(bill.x + bill.width - 0.5f * k, bill.y + 1.6f * k + i * 0.7f * k), 2, C(P->dim));
        label(haggle ? "TOTAL 560" : "TOTAL 420", bill.x + bill.width / 2, bill.y + 4.0f * k, 0.8f * k, P->bg, 0, 1);
        if (haggle) label("+ late fee", bill.x + bill.width / 2, bill.y + 5.0f * k, 0.65f * k, P->bad, 0, 1);
        if (paid) DrawTextPro(P->bold, "PAID", v2(bill.x + 0.7f * k, bill.y + 4.6f * k), v2(0, 0), -15, 1.6f * k, 1, CA(P->ok, ease((float)tw / 0.3f)));
    }
    if (ringing) buzz(ax, ay, 2.6f * k, P->text);
}
