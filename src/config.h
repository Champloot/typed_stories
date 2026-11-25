// config: the numbers you may want to tune, from config.ini.
// Every key has a default here; config.ini only overrides. The file is looked
// for in the working directory, then next to the program. TYPED_STORIES_CONFIG
// names another file, or "-" for the defaults only (the tests use that).
#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    // [time]
    double answer_scale;        // x every scene's 'time:'
    double walk_scale;          // x the time of walks in the store ('play:')
    double game_scale;          // x the time of minigames ('act:')
    // [day]
    int    scenes_per_phase;    // scenes in each part of the day; 0 = as the pack says
    int    minigames;           // 0: no minigames, the answer just happens
    int    walks;               // 0: no walking in the store
    int    meter_start;         // every meter starts here; 0 = as the pack says
    // [style]
    int    tier1, tier2, tier3; // points for plain, nicer, fancy phrases
    double typo_factor;         // x points with a typo
    double heat_decay;          // how much of a phrase's wear is left the next day
    int    word_bonus;          // style for the word of the day
    int    wordsmith_days;      // days with the word that open the secret
    // [mash]
    double mash_presses;        // x the presses a mash asks for
    double mash_leak;           // presses lost per second
    // [timing]
    double timing_sweep;        // seconds for the needle to cross once
    double timing_zone;         // width of the green zone, 0..1 of the track
    // [cross]
    double cross_speed;         // x the speed of the traffic
    int    cross_gap;           // the least free cells between vehicles
    // [catch]
    int    catch_apples;        // apples that fall
    double catch_fall;          // seconds per row
    double catch_every;         // seconds between apples
    // [grid]
    double grid_step;           // seconds per cell when walking in cross, catch and the store
    // [store]
    double cart_step;           // seconds per cell for a cart
    int    carts;               // how many carts roll around
    // [window]
    int    width, height;
    int    fullscreen;
    double fade;                // seconds a new screen takes to fade in
} Config;

extern Config cfg;

// Reads the config file (see above); exe_path is argv[0], to look next to it.
void config_load(const char *exe_path);

#endif
