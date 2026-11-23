// style: style points and phrase heat, kept between runs
#ifndef STYLE_H
#define STYLE_H

#include "common.h"

double *heat_of(const char *phrase, int create);
void style_load(void);
void style_save(void);
void style_new_run(void);
int style_points(int tier, int typo, double heat);
char style_rank(int pts);

#endif
