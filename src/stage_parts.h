// stage_parts: what the pictures share. Only the *_gl.c picture files include it.
// Every picture gets its box b; k = b.height / 11 is the size of things in it.
// ts: seconds since the scene appeared; tw: seconds since the reaction ('show:') began.
#ifndef STAGE_PARTS_H
#define STAGE_PARTS_H

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "stage_gl.h"

extern const StagePal *P;       // the palette of the picture being drawn
extern Vector2 person_head;     // where the last person() put its head

Color   C(Color c);             // a palette color, faded with the picture
Color   CA(Color c, float a);   // ... at alpha a
float   clamp01(float x);
float   ease(float x);          // smooth 0 -> 1
float   lerp(float a, float b, float t);
float   wrapf(float v, float m);
Vector2 v2(float x, float y);

int prop_is(const char *props, const char *key, const char *val);   // "dog=growl" in props?
int has_key(const char *props, const char *key);                    // "dog=..." in props?
int is(const char *show, const char *s);

void    tri(Vector2 a, Vector2 b, Vector2 c, Color col);            // any winding
void    limb(Vector2 a, Vector2 b, float th, Color c);              // a thick line with round ends
Vector2 person(float x, float gy, float k, int dir, float phase, int moving, Color c);   // returns the shoulder
void    face(Vector2 head, float k, int mood, Color c);             // mood: 1 smile, -1 frown, 2 flat, 0 eyes only
void    heart(float x, float y, float s, Color c);
void    label(const char *s, float x, float y, float size, Color c, int align, int heavy);  // align -1 left, 0 center, 1 right
void    bubble(Rectangle r, float tx, float ty, Color fill);
void    buzz(float x, float y, float r, Color c);
void    drift(const char *mark, float x, float y, float k, Color c, double t);
void    room(Rectangle b, float gy, Color wall, Color floor);

#define UNUSED_OK __attribute__((unused))
#define STAGE(name) void name(UNUSED_OK const char *props, UNUSED_OK const char *show, Rectangle b, UNUSED_OK double ts, UNUSED_OK double tw)
STAGE(st_bedroom); STAGE(st_kitchen); STAGE(st_fridge); STAGE(st_peephole); STAGE(st_doormat); STAGE(st_phone);
STAGE(st_lift); STAGE(st_crosswalk); STAGE(st_taxi); STAGE(st_entrance); STAGE(st_desk); STAGE(st_cake); STAGE(st_coffee);
STAGE(st_stand); STAGE(st_deli); STAGE(st_checkout); STAGE(st_cashier);

#endif
