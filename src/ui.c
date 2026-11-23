// ui: ncurses drawing: wrapped text, headers, the prompt line
#include <ctype.h>
#include <ncurses.h>

#include "ui.h"
#include "day.h"

void ui_init(void) {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();
    init_pair(C_OK,   COLOR_GREEN,  -1);
    init_pair(C_BAD,  COLOR_RED,    -1);
    init_pair(C_DIM,  COLOR_WHITE,  -1);
    init_pair(C_WARN, COLOR_YELLOW, -1);
    timeout(16);
}

void ui_end(void) {
    endwin();
}

// Prints text word-wrapped to the terminal width, starting at (row, col).
// Returns how many rows it used, so the caller can lay out what comes next.
int print_wrapped(int row, int col, const char *text) {
    int maxw = COLS - col - 2;
    if (maxw < 20) maxw = 20;
    int x = 0, r = row;
    const char *p = text;
    while (*p) {
        while (*p == ' ') p++;
        const char *w = p;
        while (*p && *p != ' ') p++;
        int wl = (int)(p - w);
        if (wl == 0) break;
        if (x > 0 && x + 1 + wl > maxw) { r++; x = 0; }
        if (x > 0) { mvaddch(r, col + x, ' '); x++; }
        mvaddnstr(r, col + x, w, wl);
        x += wl;
    }
    return r - row + 1;
}

/* Row 0 of every in-game screen: meters and the word of the day, set by the game. */
static char status[256];

void ui_set_status(const char *s) {
    snprintf(status, sizeof status, "%s", s);
}

void ui_draw_status(void) {
    if (!*status || COLS < 8) return;
    attron(COLOR_PAIR(C_DIM));
    mvaddnstr(0, 4, status, COLS - 6);
    attroff(COLOR_PAIR(C_DIM));
}

void draw_header(int idx, int total, const char *phase) {
    ui_draw_status();
    mvprintw(1, 4, "[%d/%d]", idx + 1, total);
    if (phase) {
        printw("  ");
        attron(A_BOLD);
        for (const char *p = phase; *p; p++) addch((chtype)toupper((unsigned char)*p));
        attroff(A_BOLD);
    }
}

// Title plus the variant's detail (the cue). Returns the first row after them.
int draw_situation(const Scenario *s, const Variant *v, int bold) {
    int r = 3;
    if (bold) attron(A_BOLD);
    r += print_wrapped(r, 4, s->title);
    if (bold) attroff(A_BOLD);
    if (v->detail) r += print_wrapped(r, 4, v->detail);
    return r;
}

// The hints say what each answer means ("agree / refuse"); the words are up to the player.
void draw_prompt(const Event *e, const char *input, const char *msg, double left,
                        int idx, int total) {
    const Variant *v = e->v;
    erase();
    draw_header(idx, total, e->phase);
    int r = draw_situation(e->s, v, 0) + 1;

    {
        char hint[512];
        size_t n = 0;
        hint[0] = '\0';
        for (int c = 0; c < v->nchoices && n < sizeof hint; c++)
            n += (size_t)snprintf(hint + n, sizeof hint - n, "%s%s", c ? "  /  " : "", v->choices[c].label);
        attron(COLOR_PAIR(C_DIM));
        r += print_wrapped(r, 4, hint) + 1;
        attroff(COLOR_PAIR(C_DIM));
    }

    mvaddstr(r, 4, "> ");
    attron(A_BOLD);
    addstr(input);
    attroff(A_BOLD);
    attron(A_DIM);
    addch('_');
    attroff(A_DIM);
    r += 2;

    // time bar
    int bar_w = COLS - 14;              // "[" + bar + "] 120.0s" must fit on one row
    if (bar_w > 30) bar_w = 30;
    if (bar_w < 5) bar_w = 5;
    double limit = e->s->time_limit;
    int filled = (int)(bar_w * (left / limit));
    if (filled < 0) filled = 0;
    int pair = left < limit * 0.3 ? C_BAD : (left < limit * 0.6 ? C_WARN : C_OK);
    attron(COLOR_PAIR(pair));
    mvprintw(r, 4, "[");
    for (int i = 0; i < bar_w; i++) addch(i < filled ? '#' : ' ');
    printw("] %.1fs", left > 0 ? left : 0);
    attroff(COLOR_PAIR(pair));

    if (*msg) {
        attron(COLOR_PAIR(C_BAD));
        print_wrapped(r + 2, 4, msg);
        attroff(COLOR_PAIR(C_BAD));
    }
    refresh();
}

void wait_key(void) {
    int ch;
    timeout(-1);
    flushinp();             // drop keys typed ahead so the result is not skipped
    do {
        ch = getch();
    } while (ch == KEY_RESIZE);
    timeout(16);
}

void wait_enter(void) {
    int ch;
    timeout(-1);
    flushinp();
    do {
        ch = getch();
    } while (ch != '\n' && ch != '\r' && ch != KEY_ENTER);
    timeout(16);
}

// Situation screen: the player reads the text and the cue; the input
// and the timer start only when Enter is pressed.
void show_intro(const Event *e, int idx, int total) {
    erase();
    draw_header(idx, total, e->phase);
    int r = draw_situation(e->s, e->v, 1);
    attron(COLOR_PAIR(C_DIM));
    print_wrapped(r + 1, 4, "Press Enter when ready. The timer starts right after.");
    attroff(COLOR_PAIR(C_DIM));
    refresh();
    wait_enter();
}

