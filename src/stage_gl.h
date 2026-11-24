// stage_gl: the pictures above a scene ('stage:' in a pack), drawn in code.
// A scenario names the stage, its variant says what is in the picture
// ('props: dog=growl stranger=near'), and an outcome says how the picture
// reacts ('show: stranger_leaves'). The cue the player has to notice is in the
// picture itself: a wagging tail, flat ears, an empty leash.
#ifndef STAGE_GL_H
#define STAGE_GL_H

#include "raylib.h"

typedef struct {
    Color bg, panel, faint, dim, text, cue, ok, bad, warn, accent;
    float alpha;                // fade of the whole picture
} StagePal;

// since_scene: seconds since this scene first appeared (things walk in, stop);
// since_show: seconds since the reaction started (show != NULL).
void stage_draw(const char *stage, const char *props, const char *show, Rectangle box,
                double since_scene, double since_show, const StagePal *pal);

#endif
