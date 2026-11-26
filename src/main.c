// typed_stories: a typing game in C, in a window (raylib) or a terminal (ncurses).
// Packs (text files) -> scenarios with hidden variants -> answers typed in your own words.
// A lexicon maps phrases to intents ("of course" means yes). Every outcome carries a mark that
// adds up to the run's rating; fancier and fresher phrasing earns style points on top.
//
// Modules: util (helpers), lexicon (intents), pack (loading files, flags, meters), match (reading
// answers), style (style points), day (building a day), rng (seeded randomness), daily (date,
// word of the day), records (best results), game (playing a run), and ui.h with two
// screens behind it: ui_gl (raylib window) and ui_tty (ncurses terminal).
#include "config.h"
#include "common.h"
#include "daily.h"
#include "game.h"
#include "lexicon.h"
#include "pack.h"
#include "progress.h"
#include "records.h"
#include "style.h"
#include "ui.h"

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "usage: %s [packs_dir]\n", argv[0]);
        return 1;
    }
    // everything is loaded and checked before the screen is taken over,
    // so file errors print normally
    const char *dir = argc == 2 ? argv[1] : "packs";
    config_load(argv[0]);
    load_lexicon(dir);
    load_packs(dir);
    load_words(dir);
    style_load();
    records_load();
    progress_load();

    ui_init();
    int p;
    while ((p = choose_pack()) >= 0) run_pack(&packs[p]);
    ui_end();
    return 0;
}

