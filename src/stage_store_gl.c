// Pictures in the store: the info stand, the deli, the self-checkout, the cashier.
#include "stage_parts.h"

#define K  (b.height / 11.0f)
#define CX (b.x + b.width / 2)

static void floor_band(Rectangle b, float gy) {
    DrawRectangle((int)b.x, (int)gy, (int)b.width, (int)(b.y + b.height - gy) + 1, CA(P->faint, 0.45f));
    DrawLineEx(v2(b.x, gy), v2(b.x + b.width, gy), 2, C(P->faint));
    for (float x = b.x; x < b.x + b.width; x += 3 * b.height / 11.0f)     // shop tiles
        DrawLineEx(v2(x, gy), v2(x - 1.5f * b.height / 11.0f, b.y + b.height), 1, CA(P->faint, 0.6f));
}

static void cart(float x, float gy, float k) {
    DrawRectangleRoundedLinesEx((Rectangle){ x, gy - 2.6f * k, 3.4f * k, 1.8f * k }, 0.2f, 4, 3, C(P->text));
    DrawLineEx(v2(x + 3.4f * k, gy - 2.6f * k), v2(x + 4.1f * k, gy - 3.3f * k), 3, C(P->text));
    DrawCircleV(v2(x + 0.6f * k, gy - 0.4f * k), 0.35f * k, C(P->text));
    DrawCircleV(v2(x + 2.8f * k, gy - 0.4f * k), 0.35f * k, C(P->text));
}

/* ---------- the info stand ---------- */

STAGE(st_stand) {
    float k = K, cx = CX, gy = b.y + b.height * 0.95f;
    floor_band(b, gy);
    Rectangle board = { cx - 8 * k, b.y + 0.6f * k, 16 * k, 8.2f * k };
    DrawRectangle((int)(cx - 0.2f * k), (int)(board.y + board.height), (int)(0.4f * k), (int)(gy - board.y - board.height), C(P->dim));
    DrawRectangleRounded(board, 0.06f, 6, C(P->bg));
    DrawRectangleLinesEx(board, 3, C(P->accent));
    label("AISLES", cx, board.y + 0.3f * k, 0.9f * k, P->accent, 0, 1);
    static const char *const rows[8] = { "1 BREAD", "2 MILK", "3 TEA", "4 POTATOES", "5 CHEESE", "6 SOAP", "7 PET FOOD", "8 FLOWERS" };
    for (int i = 0; i < 8; i++) {
        float x = board.x + 1.0f * k + (i / 4) * 7.6f * k, y = board.y + 1.8f * k + (i % 4) * 1.5f * k;
        label(rows[i], x, y, 0.85f * k, i == 3 ? P->warn : P->text, -1, i == 3);
    }
    if (prop_is(props, "note", "moved")) {      // the cue: a note taped over aisle 4
        Rectangle n = { board.x + 4.6f * k, board.y + 6.0f * k, 4.8f * k, 2.0f * k };
        DrawRectanglePro((Rectangle){ n.x + n.width / 2, n.y + n.height / 2, n.width, n.height }, v2(n.width / 2, n.height / 2), 8, C(P->warn));
        DrawRectanglePro((Rectangle){ n.x + n.width / 2 - 0.3f * k, n.y + 0.1f * k, 1.6f * k, 0.4f * k }, v2(0.8f * k, 0.2f * k), -10, CA(P->text, 0.6f));
        DrawTextPro(P->font, "moved -> 7", v2(n.x + 0.5f * k, n.y + 0.5f * k), v2(0, 0), 8, 0.85f * k, 1, C(P->bg));
        DrawLineEx(v2(board.x + 1.0f * k, board.y + 6.7f * k), v2(board.x + 4.5f * k, board.y + 6.4f * k), 4, C(P->bad));
    }
    // you, with a cart
    float px = cx - 13.5f * k;
    Vector2 sh = person(px, gy, k, 1, 0, 0, C(P->accent));
    limb(sh, v2(px + 1.6f * k, sh.y + 1.4f * k), 0.28f * k, C(P->accent));
    face(person_head, k, 0, P->bg);
    cart(px + 1.4f * k, gy, k);
}

/* ---------- the deli ---------- */

STAGE(st_deli) {
    float k = K, cx = CX, gy = b.y + b.height * 0.95f;
    double t = GetTime();
    int old = prop_is(props, "made", "old"), buy = is(show, "buy"), no = is(show, "walk_away"), sold = is(show, "sold_out");
    floor_band(b, gy);
    // the sign
    Rectangle sign = { cx - 8 * k, b.y + 0.4f * k, 16 * k, 1.6f * k };
    DrawRectangleRounded(sign, 0.3f, 6, C(P->warn));
    label("SUSPICIOUS SAUSAGE  -50%", cx, sign.y + 0.35f * k, 0.9f * k, P->bg, 0, 1);
    // the deli guy behind the counter
    float gx = cx + 5 * k;
    Vector2 gs = person(gx, gy, k, -1, 0, 0, C(P->cue));
    DrawRectangle((int)(person_head.x - 0.6f * k), (int)(person_head.y - 1.2f * k), (int)(1.2f * k), (int)(0.7f * k), C(P->text));   // white hat
    face(person_head, k, (old && no) ? 1 : (old && buy) ? -1 : 0, P->bg);
    if (buy && tw < 1.0) limb(gs, v2(gx - 2.5f * k, gs.y + 0.5f * k), 0.28f * k, C(P->cue));
    // the counter and what is in it
    Rectangle ct = { cx - 11 * k, gy - 4.2f * k, 22 * k, 4.2f * k };
    DrawRectangleRec(ct, C(P->dim));
    DrawRectangle((int)(ct.x + 0.5f * k), (int)(ct.y - 1.8f * k), (int)(ct.width - 1 * k), (int)(1.8f * k), CA(P->accent, 0.15f));
    DrawLineEx(v2(ct.x + 0.5f * k, ct.y - 1.8f * k), v2(ct.x + ct.width - 0.5f * k, ct.y - 1.8f * k), 3, CA(P->text, 0.5f));
    Color sc = old ? (Color){ 160, 150, 90, 255 } : (Color){ 210, 98, 98, 255 };
    if (!sold)
        for (int i = 0; i < 7; i++)
            DrawRectangleRounded((Rectangle){ ct.x + 1.5f * k + i * 2.8f * k, ct.y - 1.2f * k, 2.4f * k, 0.8f * k }, 1, 6, C(sc));
    else label("SOLD OUT", cx, ct.y - 1.5f * k, 1.0f * k, P->bad, 0, 1);
    // the label: this is the cue
    Rectangle tag = { cx - 3.5f * k, ct.y + 0.6f * k, 7 * k, 1.6f * k };
    DrawRectangleRec(tag, old ? C((Color){ 205, 190, 140, 255 }) : C(P->text));
    label(old ? "MADE: LAST YEAR" : "MADE: TODAY", cx, tag.y + 0.4f * k, 0.8f * k, old ? P->bad : P->ok, 0, 1);
    if (old && !sold)               // flies like it
        for (int i = 0; i < 4; i++) {
            float a = (float)t * (2.5f + i) + i * 1.7f;
            DrawCircleV(v2(cx + cosf(a) * 7 * k, ct.y - 2.3f * k + sinf(a * 1.4f) * 0.9f * k), 0.12f * k, C(P->bg));
        }
    // you
    float px = cx - 14 * k - (no ? ease((float)tw / 1.5f) * 8 * k : 0);
    Vector2 sh = person(px, gy, k, no ? -1 : 1, (float)tw * 8, no && tw < 1.5, C(P->accent));
    face(person_head, k, buy ? 1 : 0, P->bg);
    if (buy) {          // a paper parcel
        Vector2 h = v2(px + 1.2f * k, sh.y + 1.2f * k);
        limb(sh, h, 0.28f * k, C(P->accent));
        DrawRectangleRounded((Rectangle){ h.x - 0.2f * k, h.y - 0.6f * k, 2 * k, 1.2f * k }, 0.4f, 6, C((Color){ 220, 200, 160, 255 }));
        if (!old) drift("~", h.x + 0.8f * k, h.y - 0.8f * k, k * 0.7f, P->warn, t);
    }
    if (sold) {         // the happy man who bought it all
        Vector2 ms = person(cx - 8 * k, gy, k, 1, 0, 0, C(P->cue));
        face(person_head, k, 1, P->bg);
        limb(ms, v2(cx - 6.6f * k, ms.y + 0.8f * k), 0.28f * k, C(P->cue));
        for (int i = 0; i < 3; i++) DrawRectangleRounded((Rectangle){ cx - 7.4f * k, ms.y + 0.2f * k + i * 0.5f * k, 2.2f * k, 0.45f * k }, 0.4f, 4, C((Color){ 220, 200, 160, 255 }));
    }
}

/* ---------- the self-checkout ---------- */

STAGE(st_checkout) {
    float k = K, cx = CX, gy = b.y + b.height * 0.95f;
    double t = GetTime();
    int bag = prop_is(props, "bag", "yours"), lift = is(show, "lift"), staff = is(show, "staff"), queue = is(show, "queue");
    int calm = (lift && bag) || (staff && tw > 1.6);
    floor_band(b, gy);
    // the machine
    Rectangle m = { cx - 3.5f * k, b.y + 0.5f * k, 7 * k, gy - b.y - 0.5f * k };
    DrawRectangleRounded(m, 0.08f, 6, C(P->dim));
    Rectangle scr = { m.x + 0.5f * k, m.y + 0.5f * k, m.width - 1 * k, 3.8f * k };
    int blink = fmodf((float)t, 0.5f) < 0.3f;
    DrawRectangleRec(scr, calm ? C(P->ok) : (blink ? C(P->bad) : CA(P->bad, 0.6f)));
    if (calm) label("THANK YOU", scr.x + scr.width / 2, scr.y + 1.4f * k, 0.9f * k, P->bg, 0, 1);
    else {
        label("UNEXPECTED", scr.x + scr.width / 2, scr.y + 0.5f * k, 0.8f * k, P->text, 0, 1);
        label("ITEM IN THE", scr.x + scr.width / 2, scr.y + 1.5f * k, 0.8f * k, P->text, 0, 1);
        label("BAGGING AREA", scr.x + scr.width / 2, scr.y + 2.5f * k, 0.8f * k, P->text, 0, 1);
        buzz(scr.x + scr.width / 2, scr.y + 1.9f * k, 3.6f * k, P->bad);
    }
    DrawRectangle((int)(m.x + 1 * k), (int)(m.y + 5.0f * k), (int)(5 * k), (int)(0.4f * k), CA(P->bad, 0.7f));     // the scanner
    // the bagging area: empty, or your own bag on it
    Rectangle area = { cx + 4.2f * k, gy - 3.6f * k, 6 * k, 0.5f * k };
    DrawRectangle((int)(area.x + 2.7f * k), (int)(area.y), (int)(0.6f * k), (int)(gy - area.y), C(P->dim));
    DrawRectangleRec(area, C(P->text));
    label(bag && !lift ? "1.20 kg" : "0.00 kg", area.x + area.width / 2, area.y + 0.7f * k, 0.55f * k, P->dim, 0, 0);
    float px = cx - 11 * k;
    Vector2 sh = person(px, gy, k, 1, 0, 0, C(P->accent));
    Vector2 bagat = v2(area.x + 3 * k, area.y - 1.8f * k);
    if (lift) {
        float f = ease((float)tw / 0.6f);
        Vector2 up = v2(px + 1.0f * k, sh.y - 1.8f * k);
        limb(sh, up, 0.28f * k, C(P->accent));
        limb(sh, v2(px - 0.8f * k, sh.y - 1.8f * k), 0.28f * k, C(P->accent));
        if (bag) bagat = v2(lerp(bagat.x, up.x, f), lerp(bagat.y, up.y - 1.6f * k, f));
        else label("?", px, sh.y - 4.5f * k, 1.2f * k, P->text, 0, 1);
    }
    if (bag) {
        DrawRectangleRounded((Rectangle){ bagat.x - 1.3f * k, bagat.y, 2.6f * k, 1.8f * k }, 0.2f, 4, C(P->ok));
        DrawRing(v2(bagat.x, bagat.y), 0.6f * k, 0.8f * k, 180, 360, 16, C(P->ok));
    }
    face(person_head, k, calm ? 1 : -1, P->bg);
    if (staff) {        // someone in a green vest, one tap
        float sx = lerp(b.x + b.width + 2 * k, cx + 4.5f * k, ease((float)tw / 1.0f));
        Vector2 ss = person(sx, gy, k, -1, (float)tw * 8, tw < 1.0, C(P->cue));
        DrawRectangle((int)(sx - 0.55f * k), (int)(ss.y - 0.1f * k), (int)(1.1f * k), (int)(1.5f * k), C(P->ok));
        face(person_head, k, 2, P->bg);
        limb(ss, tw > 1.0 && tw < 1.8 ? v2(scr.x + scr.width - 0.4f * k, scr.y + 2 * k) : v2(sx - 0.6f * k, ss.y + 1.4f * k), 0.28f * k, C(P->cue));
    }
    if (queue)          // people behind you, getting a look
        for (int i = 0; i < 3; i++) {
            float qx = px - 3 * k - i * 2.6f * k + (1 - ease((float)tw / (1.0f + i * 0.3f))) * -6 * k;
            person(qx, gy, k * 0.95f, 1, 0, 0, CA(P->cue, 0.9f));
            face(person_head, k * 0.95f, -1, P->bg);
        }
}

/* ---------- the cashier ---------- */

STAGE(st_cashier) {
    float k = K, cx = CX, gy = b.y + b.height * 0.95f;
    double t = GetTime();
    int apples = prop_is(props, "apples", "14"), bag = is(show, "bag"), no = is(show, "no_bag");
    floor_band(b, gy);
    // the cashier and the register
    float cx2 = cx + 9 * k;
    Vector2 cs = person(cx2, gy, k, -1, 0, 0, C(P->cue));
    face(person_head, k, (apples && no) ? -1 : 1, P->bg);
    if (is(show, "juggle")) {           // the cashier claps
        float c = sinf((float)t * 14) * 0.3f * k;
        limb(cs, v2(cx2 - 0.8f * k + c, cs.y - 1.2f * k), 0.26f * k, C(P->cue));
    }
    DrawRectangleRounded((Rectangle){ cx + 4.5f * k, gy - 5.2f * k, 3 * k, 2 * k }, 0.2f, 4, C(P->dim));
    DrawRectangle((int)(cx + 4.8f * k), (int)(gy - 4.9f * k), (int)(2.4f * k), (int)(0.8f * k), C(P->ok));
    // the belt, moving
    Rectangle belt = { cx - 9 * k, gy - 3.2f * k, 16 * k, 0.8f * k };
    DrawRectangleRec((Rectangle){ belt.x, belt.y, belt.width, gy - belt.y }, C(P->dim));
    DrawRectangleRec(belt, C(P->bg));
    for (float x = belt.x + wrapf((float)t * 2 * k, 1.6f * k); x < belt.x + belt.width; x += 1.6f * k)
        DrawLineEx(v2(x, belt.y), v2(x, belt.y + belt.height), 2, C(P->faint));
    // you, and what you are buying
    float px = cx - 13 * k;
    Vector2 sh = person(px, gy, k, 1, 0, 0, C(P->accent));
    face(person_head, k, (apples && no) ? -1 : 1, P->bg);
    if (apples) {
        if (is(show, "juggle")) {   // all fourteen, in your arms, not one dropped
            limb(sh, v2(px + 1.2f * k, sh.y + 1.0f * k), 0.28f * k, C(P->accent));
            for (int i = 0; i < 14; i++)
                DrawCircleV(v2(px + 0.6f * k + (i % 4) * 0.55f * k, sh.y + 0.3f * k - (i / 4) * 0.5f * k), 0.3f * k, C(P->bad));
            face(person_head, k, 1, P->bg);
        } else if (no) {    // in your arms; three of them fall
            limb(sh, v2(px + 1.2f * k, sh.y + 1.0f * k), 0.28f * k, C(P->accent));
            for (int i = 0; i < 11; i++)
                DrawCircleV(v2(px + 0.6f * k + (i % 4) * 0.55f * k, sh.y + 0.3f * k - (i / 4) * 0.5f * k), 0.3f * k, C(P->bad));
            for (int i = 0; i < 3; i++) {
                float f = clamp01(((float)tw - i * 0.4f) / 0.6f);
                DrawCircleV(v2(px + 1.5f * k + f * (2 + i) * k, lerp(sh.y + 0.6f * k, gy - 0.3f * k, f * f)), 0.3f * k, C(P->bad));
            }
        } else if (bag) {
            limb(sh, v2(px + 1.2f * k, sh.y + 1.6f * k), 0.28f * k, C(P->accent));
            DrawRectangleRounded((Rectangle){ px + 0.4f * k, sh.y + 1.6f * k, 2.2f * k, 2.4f * k }, 0.3f, 6, C(P->text));
            for (int i = 0; i < 3; i++) DrawCircleV(v2(px + 0.9f * k + i * 0.6f * k, sh.y + 1.7f * k), 0.3f * k, C(P->bad));
        } else {
            for (int i = 0; i < 14; i++)        // the cue: fourteen apples and nothing to put them in
                DrawCircleV(v2(belt.x + 2 * k + (i % 7) * 0.75f * k, belt.y - 0.35f * k - (i / 7) * 0.6f * k), 0.33f * k, C(P->bad));
            label("14", belt.x + 4.2f * k, belt.y - 2.4f * k, 0.8f * k, P->text, 0, 1);
        }
    } else {
        // the cue: your own bag, on your shoulder
        DrawRectangleRounded((Rectangle){ px - 1.6f * k, sh.y + 0.6f * k, 1.6f * k, 1.9f * k }, 0.2f, 4, C(P->ok));
        DrawLineEx(v2(px - 0.8f * k, sh.y + 0.6f * k), v2(px + 0.2f * k, sh.y - 0.1f * k), 3, C(P->ok));
        for (int i = 0; i < 3; i++)
            DrawRectangleRounded((Rectangle){ belt.x + 2 * k + i * 2.2f * k, belt.y - 1.0f * k, 1.6f * k, 1.0f * k }, 0.2f, 4, C(i == 1 ? P->warn : P->text));
        if (bag) DrawRectangleRounded((Rectangle){ px + 0.6f * k, sh.y + 1.4f * k, 1.8f * k, 2.2f * k }, 0.3f, 6, C(P->text));
        if (no) drift("+1", px - 0.8f * k, sh.y - 0.4f * k, k, P->ok, t);
    }
    if (!show) {
        Rectangle bb = { cx2 - 6 * k, b.y + 0.5f * k, 4.6f * k, 1.6f * k };
        bubble(bb, cx2 - 0.6f * k, person_head.y - 0.8f * k, P->text);
        label("bag?", bb.x + bb.width / 2, bb.y + 0.4f * k, 0.8f * k, P->bg, 0, 1);
    } else if (!no && !bag) {       // timeout: a bag, handed over with a sigh
        limb(cs, v2(cx2 - 2 * k, cs.y + 0.4f * k), 0.28f * k, C(P->cue));
        DrawRectangleRounded((Rectangle){ cx2 - 3.2f * k, cs.y, 1.4f * k, 1.6f * k }, 0.3f, 4, C(P->text));
        label("sigh", cx2, person_head.y - 1.8f * k, 0.6f * k, P->dim, 0, 0);
    }
}
