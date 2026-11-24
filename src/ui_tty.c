// ui_tty: the terminal version of ui.h, on ncurses. Plain text, one column of
// word-wrapped lines; the tests read exactly these screens.
#include <ctype.h>
#include <ncurses.h>

#include "ui.h"

enum { C_OK = 1, C_BAD, C_DIM, C_WARN };

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

int ui_key(int timeout_ms) {
    timeout(timeout_ms);
    int ch = getch();
    if (ch == ERR || ch == KEY_RESIZE) return K_NONE;
    if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) return K_BACKSPACE;
    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) return K_ENTER;
    if (ch == K_CTRL_U || ch == K_CTRL_W) return ch;
    if (ch >= 32 && ch <= 126) return ch;
    return K_OTHER;
}

void ui_flush(void) {
    flushinp();
}

// Prints text word-wrapped to the terminal width, starting at (row, col).
// Returns how many rows it used, so the caller can lay out what comes next.
static int print_wrapped(int row, int col, const char *text) {
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

static void upcase(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    for (; src[i] && i + 1 < cap; i++) dst[i] = (char)toupper((unsigned char)src[i]);
    dst[i] = '\0';
}

/* Row 0: "Nerves 5  Money 3!  People 6    word: hedgehog". */
static char status[256];

void ui_set_hud(const Hud *h) {
    size_t n = 0;
    status[0] = '\0';
    if (!h) return;
    for (int i = 0; i < h->nmeters && n < sizeof status; i++)
        n += (size_t)snprintf(status + n, sizeof status - n, "%s%s %d%s", i ? "  " : "",
                              h->meter_names[i], h->meters[i], h->meters[i] <= 2 ? "!" : "");
    if (h->word && n < sizeof status)
        snprintf(status + n, sizeof status - n, "%sword: %s%s", n ? "    " : "", h->word, h->word_used ? " (used)" : "");
}

static void draw_status(void) {
    if (!*status || COLS < 8) return;
    attron(COLOR_PAIR(C_DIM));
    mvaddnstr(0, 4, status, COLS - 6);
    attroff(COLOR_PAIR(C_DIM));
}

static void draw_header(const Scene *s) {
    draw_status();
    mvprintw(1, 4, "[%d/%d]", s->idx + 1, s->total);
    if (s->phase) {
        printw("  ");
        attron(A_BOLD);
        for (const char *p = s->phase; *p; p++) addch((chtype)toupper((unsigned char)*p));
        attroff(A_BOLD);
    }
}

// Title plus the cue. Returns the first row after them.
static int draw_situation(const Scene *s, int bold) {
    int r = 3;
    if (bold) attron(A_BOLD);
    r += print_wrapped(r, 4, s->title);
    if (bold) attroff(A_BOLD);
    if (s->detail) r += print_wrapped(r, 4, s->detail);
    return r;
}

void ui_menu(const MenuView *v) {
    erase();
    int r = 2 + print_wrapped(2, 4, "TYPING GAME - choose who you are today") + 1;
    for (int i = 0; i < v->npacks; i++) {
        const MenuPack *p = &v->packs[i];
        mvprintw(r + i, 4, "%d) %s", i + 1, p->name);
        char rec[80] = "";
        if (p->best >= 0) snprintf(rec, sizeof rec, "best %d", p->best);
        if (p->today >= 0) snprintf(rec + strlen(rec), sizeof rec - strlen(rec), "%stoday %d", *rec ? "  " : "", p->today);
        if (*rec) {
            attron(COLOR_PAIR(C_DIM));
            printw("   %s", rec);
            attroff(COLOR_PAIR(C_DIM));
        }
    }
    r += v->npacks + 1;
    mvprintw(r, 4, "t) mode: %s", v->today_mode ? "today" : "free play");
    attron(COLOR_PAIR(C_DIM));
    if (v->today_mode) printw("  (the same day for everyone, %s)", v->date);
    else printw("  (a random day)");
    attroff(COLOR_PAIR(C_DIM));
    mvprintw(r + 1, 4, "q) quit");
    if (v->word) mvprintw(r + 3, 4, "Word of the day: %s", v->word);
    refresh();
}

void ui_day_start(const DayStartView *v) {
    char line[320], up[64];
    erase();
    draw_status();
    int r = 2;
    attron(A_BOLD);
    r += print_wrapped(r, 4, v->pack) + 1;
    attroff(A_BOLD);
    if (v->today_mode) snprintf(line, sizeof line, "Today, %s. Everyone gets the same day.", v->date);
    else snprintf(line, sizeof line, "Free play: a random day.");
    r += print_wrapped(r, 4, line) + 1;
    if (v->word) {
        upcase(up, sizeof up, v->word);
        snprintf(line, sizeof line, "Word of the day: %s", up);
        attron(A_BOLD);
        r += print_wrapped(r, 4, line);
        attroff(A_BOLD);
        r += print_wrapped(r, 4, "Slip it into any answer today for +100 style. It does not change your rating.") + 1;
    }
    if (v->has_meters)
        r += print_wrapped(r, 4, "Keep the meters at the top above zero, or the day ends early.") + 1;
    if (v->best >= 0) {
        if (v->today >= 0) snprintf(line, sizeof line, "Your best: %d. Today's best: %d.", v->best, v->today);
        else snprintf(line, sizeof line, "Your best: %d.", v->best);
        r += print_wrapped(r, 4, line) + 1;
    }
    attron(COLOR_PAIR(C_DIM));
    print_wrapped(r, 4, "Press Enter to start.");
    attroff(COLOR_PAIR(C_DIM));
    refresh();
}

void ui_intro(const Scene *s) {
    erase();
    draw_header(s);
    int r = draw_situation(s, 1);
    attron(COLOR_PAIR(C_DIM));
    print_wrapped(r + 1, 4, "Press Enter when ready. The timer starts right after.");
    attroff(COLOR_PAIR(C_DIM));
    refresh();
}

void ui_prompt(const PromptView *v) {
    erase();
    draw_header(&v->scene);
    int r = draw_situation(&v->scene, 0) + 1;

    char hint[512];
    size_t n = 0;
    hint[0] = '\0';
    for (int c = 0; c < v->nhints && n < sizeof hint; c++)
        n += (size_t)snprintf(hint + n, sizeof hint - n, "%s%s", c ? "  /  " : "", v->hints[c]);
    attron(COLOR_PAIR(C_DIM));
    r += print_wrapped(r, 4, hint) + 1;
    attroff(COLOR_PAIR(C_DIM));

    mvaddstr(r, 4, "> ");
    attron(A_BOLD);
    addstr(v->input);
    attroff(A_BOLD);
    attron(A_DIM);
    addch('_');
    attroff(A_DIM);
    r += 2;

    // time bar
    int bar_w = COLS - 14;              // "[" + bar + "] 120.0s" must fit on one row
    if (bar_w > 30) bar_w = 30;
    if (bar_w < 5) bar_w = 5;
    int filled = (int)(bar_w * (v->left / v->limit));
    if (filled < 0) filled = 0;
    int pair = v->left < v->limit * 0.3 ? C_BAD : (v->left < v->limit * 0.6 ? C_WARN : C_OK);
    attron(COLOR_PAIR(pair));
    mvprintw(r, 4, "[");
    for (int i = 0; i < bar_w; i++) addch(i < filled ? '#' : ' ');
    printw("] %.1fs", v->left > 0 ? v->left : 0);
    attroff(COLOR_PAIR(pair));

    if (*v->msg) {
        attron(COLOR_PAIR(C_BAD));
        print_wrapped(r + 2, 4, v->msg);
        attroff(COLOR_PAIR(C_BAD));
    }
    refresh();
}

void ui_outcome(const OutcomeView *v) {
    char fx[200] = "";
    for (int i = 0; i < v->fx.n; i++) {
        size_t n = strlen(fx);
        snprintf(fx + n, sizeof fx - n, "%s%s %+d", n ? "   " : "", v->fx.name[i], v->fx.delta[i]);
    }
    erase();
    draw_header(&v->scene);
    int r = draw_situation(&v->scene, 0) + 1;
    if (!v->typed) {
        attron(COLOR_PAIR(C_BAD));
        mvprintw(r, 4, "TIME'S UP");
        attroff(COLOR_PAIR(C_BAD));
        r += 2;
        r += print_wrapped(r, 4, v->text);
        if (v->fate) r += print_wrapped(r, 4, v->fate);
        if (*fx) {
            attron(COLOR_PAIR(C_WARN));
            r += 1 + print_wrapped(r + 1, 4, fx);
            attroff(COLOR_PAIR(C_WARN));
        }
        mvprintw(r + 1, 4, "(press any key)");
        refresh();
        return;
    }
    attron(COLOR_PAIR(C_OK));
    mvprintw(r, 4, "\"%s\"", v->typed);
    attroff(COLOR_PAIR(C_OK));
    r += 2;
    r += print_wrapped(r, 4, v->text);
    if (*fx) {
        attron(COLOR_PAIR(C_WARN));
        r += 1 + print_wrapped(r + 1, 4, fx);
        attroff(COLOR_PAIR(C_WARN));
    }
    char note[240];
    snprintf(note, sizeof note, "style %c +%d%s%s%s   %.1fs", v->rank, v->points,
             v->typo ? "  (typo)" : "", v->repeated ? "  (heard that before)" : "",
             v->word_bonus ? "  WORD OF THE DAY +100" : v->word_again ? "  (word already used today)" : "", v->seconds);
    attron(COLOR_PAIR(C_DIM));
    int nl = print_wrapped(r + 1, 4, note);
    attroff(COLOR_PAIR(C_DIM));
    mvprintw(r + 1 + nl + 1, 4, "(press any key)");
    refresh();
}

void ui_ending(const EndingView *v) {
    char line[200], up[64];
    erase();
    draw_status();
    upcase(up, sizeof up, v->meter);
    snprintf(line, sizeof line, "%s HIT ZERO. THE DAY IS OVER.", up);
    attron(COLOR_PAIR(C_BAD) | A_BOLD);
    int r = 3 + print_wrapped(3, 4, line) + 1;
    attroff(COLOR_PAIR(C_BAD) | A_BOLD);
    r += print_wrapped(r, 4, v->text);
    mvprintw(r + 1, 4, "(press any key)");
    refresh();
}

// The terminal has one look for every pack.
void ui_set_theme(const char *style) { (void)style; }

void ui_finale(const FinaleView *v) {
    char line[200], up[160];
    erase();
    draw_status();
    upcase(up, sizeof up, v->title);
    snprintf(line, sizeof line, "THE END: %s", up);
    int pair = v->special ? C_WARN : v->good == 2 ? C_OK : v->good == 0 ? C_BAD : 0;
    if (pair) attron(COLOR_PAIR(pair));
    attron(A_BOLD);
    int r = 3 + print_wrapped(3, 4, line) + 1;
    attroff(A_BOLD);
    if (pair) attroff(COLOR_PAIR(pair));
    r += print_wrapped(r, 4, v->text);
    mvprintw(r + 1, 4, "(press any key)");
    refresh();
}

void ui_summary(const SummaryView *v) {
    erase();
    draw_status();
    char line[320];
    snprintf(line, sizeof line, "%s: %s", v->ended_early ? "DAY OVER EARLY" : "RUN COMPLETE", v->pack);
    int r = 2 + print_wrapped(2, 4, line);
    attron(A_BOLD);
    mvprintw(r + 1, 4, "Rating: %d/10", v->rating);
    attroff(A_BOLD);
    r += 3;
    r += print_wrapped(r, 4, v->verdict);
    r += 1;
    mvprintw(r++, 4, "Style:    %c (avg %d per answer)", v->style_rank, v->style_avg);
    if (v->word)
        mvprintw(r++, 4, "Word:     %s, %s", v->word, v->word_used ? "used (+100)" : "not used");
    if (v->meters.nmeters) {
        size_t n = (size_t)snprintf(line, sizeof line, "Meters:  ");
        for (int m = 0; m < v->meters.nmeters && n < sizeof line; m++)
            n += (size_t)snprintf(line + n, sizeof line - n, " %s %d", v->meters.meter_names[m], v->meters.meters[m]);
        r += print_wrapped(r, 4, line);
    }
    mvprintw(r++, 4, "Typing:   %.0f%% accuracy, %.0f wpm", v->accuracy, v->wpm);
    mvprintw(r++, 4, "Timeouts: %d, not understood: %d", v->timeouts, v->misses);
    r++;
    attron(A_BOLD);
    if (v->new_best && v->prev_best >= 0) mvprintw(r++, 4, "Score:    %d  NEW RECORD! (was %d)", v->score, v->prev_best);
    else if (v->new_best)                 mvprintw(r++, 4, "Score:    %d  (first record)", v->score);
    else                                  mvprintw(r++, 4, "Score:    %d  (best %d)", v->score, v->prev_best);
    attroff(A_BOLD);
    if (v->today_mode)
        mvprintw(r++, 4, "Today:    best %d%s", v->today_best, v->new_day ? " (that's you)" : "");
    mvprintw(r + 1, 4, "(press any key)");
    refresh();
}
