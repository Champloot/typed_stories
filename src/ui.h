// ui: ncurses drawing: wrapped text, headers, the prompt line
#ifndef UI_H
#define UI_H

#include "common.h"
#include "day.h"

enum { C_OK = 1, C_BAD, C_DIM, C_WARN };

void ui_init(void);
void ui_end(void);
void ui_set_status(const char *s);
void ui_draw_status(void);
int print_wrapped(int row, int col, const char *text);
void draw_header(int idx, int total, const char *phase);
int draw_situation(const Scenario *s, const Variant *v, int bold);
void draw_prompt(const Event *e, const char *input, const char *msg, double left, int idx, int total);
void wait_key(void);
void wait_enter(void);
void show_intro(const Event *e, int idx, int total);

#endif
